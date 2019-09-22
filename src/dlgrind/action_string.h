#include <cstdint>
#include <array>
#include <ostream>

#include <kj/debug.h>

#include <dlgrind/schema.capnp.h>

// Design notes:
//
// Originally, I stored individual actions (e.g., ActionCode) in a
// string.  In this
// setting, on S1/S2 only Erik, combo length is something like
// frames/53+1 (this underestimates is some cases).  After 60s you end
// up with rotation length 67.
//
// However, action code currently has 5 elements (x f s1 s2 s3),
// which means you can't actually fit it in two bits.  So instead we
// opted for a variable length code that uses 4-bits, but can encode
// runs of basic combo up to five.
//
// Empirically, encoding 60 frames of actions requires only 25
// in the variable length coding.  This is how we chose 16 bytes
// (32 codes).  This also satisfies our memory budget: we use
// 9G to compute Erik with S3.

// Order of this class matters!  It determines what the "preferred"
// action at any given point in time is (bottom is "most preferred")
// So for example suppose we have c1fs c5fs and c5fs c1fs as possible
// optimal combos; we prefer to take the latter ("front loading" long
// combos).
enum class ActionFragment : uint8_t { // actually only four bit
  NIL = 0,
  FS,
  // NB: We rely on C1-C5/C1FS-C5FS being interleaved in this way.
  // You can reorder them but then you need to edit ActionString::pack()
  // to handle it correctly.  I think C5 should be preferred over C4FS
  // which is why I interleaved, but that's something that's up to
  // taste.
  C1,
  C1FS,
  C2,
  C2FS,
  C3,
  C3FS,
  C4,
  C4FS,
  C5,
  C5FS,
  S1,
  S2,
  S3,
  // 15 (one free slot)
};

// Fixed size encoding of action strings.  Supports "compressed" action
// string of size up to 32, in 16 bytes of space.  NIL terminated.
struct ActionString {
  std::array<uint8_t, 16> buffer_ = {};
  static ActionFragment i2f(uint8_t c) {
    return static_cast<ActionFragment>(c);
  }
  static uint8_t f2i(ActionFragment c) {
    return static_cast<uint8_t>(c);
  }
  static ActionFragment _unpack(uint8_t c, int i) {
    if (i == 0) {
      return i2f((c & 0xF0) >> 4);
    } else {
      return i2f(c & 0x0F);
    }
  }
  static uint8_t _pack(ActionFragment first, ActionFragment second) {
    // NB: It matters that the first element is higher order bits;
    // this means that comparison treats first as most significant,
    // which coincides with lexicographic ordering
    return (f2i(first) << 4) | f2i(second);
  }
  static int _null_at(uint8_t c) {
    if (c == 0) return 0;
    if (_unpack(c, 1) == ActionFragment::NIL) {
      KJ_ASSERT(_unpack(c, 0) != ActionFragment::NIL, c);
      return 1;
    }
    return -1;
  }
  // Get the ith action fragment in an action string.  Valid to read
  // beyond the end of the action string, but not beyond the internal
  // buffer.  In practice this means we can only actually store
  // strings of size 31 (since we don't have any out-of-bounds
  // checking).
  ActionFragment get(int i) const {
    uint8_t c = buffer_[i / 2];
    return _unpack(c, i % 2);
  }
  // Assignment to af[i] not supported.  vector<bool>, rest in peace!
  ActionFragment operator[](int i) const {
    return get(i);
  }
  // Set the ith action fragment in an action string.
  void set(int i, ActionFragment f) {
    KJ_ASSERT(i < 32, i);
    uint8_t c = buffer_[i / 2];
    ActionFragment first = _unpack(c, 0);
    ActionFragment second = _unpack(c, 1);
    if (i % 2 == 0) {
      first = f;
    } else {
      second = f;
    }
    buffer_[i / 2] = _pack(first, second);
  }
  // Push an action code to an action string.  The code will
  // be coalesced with the latest action fragment if possible.
  void push(Action ac) {
    int loc = -1;
    for (int i = 0; i < 16; i++) {
      int j = _null_at(buffer_[i]);
      if (j != -1) {
        loc = i * 2 + j;
        break;
      }
    }
    KJ_ASSERT(loc != -1, loc);
    // Check if we can absorb this into the
    // previous entry
    if (loc != 0) {
      ActionFragment p_c = get(loc - 1);
      switch (ac) {
        case Action::X:
          switch (p_c) {
            case ActionFragment::C1:
            case ActionFragment::C2:
            case ActionFragment::C3:
            case ActionFragment::C4:
              set(loc - 1, i2f(f2i(p_c) + 2));
              return;
            default:
              break;
          }
          break;
        case Action::FS:
          switch (p_c) {
            case ActionFragment::C1:
            case ActionFragment::C2:
            case ActionFragment::C3:
            case ActionFragment::C4:
            case ActionFragment::C5:
              set(loc - 1, i2f(f2i(p_c) + 1));
              return;
            default:
              break;
          }
          break;
        default:
          break;
      }
    }
    ActionFragment c;
    switch (ac) {
      case Action::X:
        c = ActionFragment::C1;
        break;
      case Action::FS:
        c = ActionFragment::FS;
        break;
      case Action::S1:
        c = ActionFragment::S1;
        break;
      case Action::S2:
        c = ActionFragment::S2;
        break;
      case Action::S3:
        c = ActionFragment::S3;
        break;
      default:
        KJ_ASSERT(0, ac);
    }
    set(loc, c);
  }
};

std::ostream& operator<<(std::ostream& os, const ActionString& as) {
  for (int i = 0; i < 32; i++) {
    ActionFragment f = as.get(i);
    switch(f) {
      case ActionFragment::NIL:
        return os;
      case ActionFragment::C1:
        os << "c1 ";
        break;
      case ActionFragment::C2:
        os << "c2 ";
        break;
      case ActionFragment::C3:
        os << "c3 ";
        break;
      case ActionFragment::C4:
        os << "c4 ";
        break;
      case ActionFragment::C5:
        os << "c5 ";
        break;
      case ActionFragment::C1FS:
        os << "c1fs ";
        break;
      case ActionFragment::C2FS:
        os << "c2fs ";
        break;
      case ActionFragment::C3FS:
        os << "c3fs ";
        break;
      case ActionFragment::C4FS:
        os << "c4fs ";
        break;
      case ActionFragment::C5FS:
        os << "c5fs ";
        break;
      case ActionFragment::FS:
        os << "fs ";
        break;
      case ActionFragment::S1:
        os << "s1  ";
        break;
      case ActionFragment::S2:
        os << "s2  ";
        break;
      case ActionFragment::S3:
        os << "s3  ";
        break;
    }
  }
  return os;
}
