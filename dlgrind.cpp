#include <kj/main.h>

#include <dlgrind/schema.capnp.h>
#include <dlgrind/hopcroft.h>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include <magic_enum.h>

#include <unordered_map>
#include <vector>
#include <optional>

#include <fcntl.h>
#include <unistd.h>

using frames_t = uint32_t;

uint64_t checked_mul(uint64_t a, uint64_t b) {
  uint64_t x = a * b;
  KJ_ASSERT(!(a != 0 && x / a != b), a, b, "overflow");
  return x;
}

// Return the index of an enum in magic_enum::enum_values
template <typename T>
size_t enum_index(T val) {
  size_t i = 0;
  for (auto v : magic_enum::enum_values<T>()) {
    if (v == val) return i;
    i++;
  }
  KJ_ASSERT(0, val, "invalid enum");
}

struct AdventurerState {
  AfterAction afterAction_;
  uint8_t uiHiddenFramesLeft_;
  uint16_t sp_[3];

  bool operator==(const AdventurerState& other) const {
    return afterAction_ == other.afterAction_ &&
           uiHiddenFramesLeft_ == other.uiHiddenFramesLeft_ &&
           sp_[0] == other.sp_[0] &&
           sp_[1] == other.sp_[1] &&
           sp_[2] == other.sp_[2];
  }
};

inline uint KJ_HASHCODE(const AdventurerState& st) {
  return kj::hashCode(static_cast<uint>(st.afterAction_), st.uiHiddenFramesLeft_, st.sp_[0], st.sp_[1], st.sp_[2]);
}

struct AdventurerStateHasher {
  std::size_t operator()(const AdventurerState& st) const { return kj::hashCode(st); }
};

template <typename T>
using AdventurerStateMap = std::unordered_map<AdventurerState, T, AdventurerStateHasher>;

using state_code_t = uint64_t;
using action_code_t = uint8_t;

class DLGrindMain {
public:
  explicit DLGrindMain(kj::ProcessContext& context)
      : context(context) {}
  kj::MainFunc getMain() {
    return kj::MainBuilder(context, "dlgrind",
        "Compute optimal rotations for characters in Dragalia Lost")
      .callAfterParsing(KJ_BIND_METHOD(*this, run))
      .build();
  }

  kj::MainBuilder::Validity run() {
    weapon_class_ = read<WeaponClass>("dat/axe.bin");
    weapon_ = read<Weapon>("dat/axe5b1.bin");
    adventurer_ = read<Adventurer>("dat/Erik.bin");

    KJ_LOG(INFO, *weapon_class_);
    KJ_LOG(INFO, *weapon_);
    KJ_LOG(INFO, *adventurer_);

    // Compute reachable states
    using InverseMap = AdventurerStateMap<std::vector<std::pair<AdventurerState, Action>>>;
    InverseMap inverse_map;
    size_t inverse_size = 0;
    {
      AdventurerState init_st =  {
        .afterAction_ = AfterAction::AFTER_NOTHING,
        .uiHiddenFramesLeft_ = 0,
        .sp_ = {0, 0, 0}
      };
      std::vector<AdventurerState> todo{init_st};
      inverse_map[init_st];
      while (todo.size()) {
        auto s = todo.back();
        // KJ_LOG(INFO, s.afterAction_, s.uiHiddenFramesLeft_, s.sp_[0], s.sp_[1], s.sp_[2], "loop");
        todo.pop_back();
        auto push = [&](AdventurerState n_s, Action a) {
          if (inverse_map.count(n_s) == 0) {
            todo.emplace_back(n_s);
          }
          inverse_map[n_s].emplace_back(s, a);
          inverse_size++;
        };
        for (auto a : magic_enum::enum_values<Action>()) {
          auto mb_n_s = applyAction(s, a);
          if (mb_n_s) {
            push(*mb_n_s, a);
          }
        }
      }
    }

    KJ_LOG(INFO, inverse_map.size(), "initial states");

    // Number states
    AdventurerStateMap<state_code_t> state_encode;
    std::vector<AdventurerState> state_decode;

    for (const auto& kv : inverse_map) {
      state_encode.emplace(kv.first, state_decode.size());
      state_decode.emplace_back(kv.first);
    }

    // Number actions
    std::unordered_map<Action, action_code_t> action_encode;
    std::vector<Action> action_decode;

    for (auto val : magic_enum::enum_values<Action>()) {
      action_encode.emplace(val, action_decode.size());
      action_decode.emplace_back(val);
    }

    // Minimize states
    capnp::MallocMessageBuilder hopcroft_input_msg;
    auto hopcroft_input = hopcroft_input_msg.initRoot<HopcroftInput>();
    {
      hopcroft_input.setNumStates(state_decode.size());
      hopcroft_input.setNumActions(action_decode.size());

      {
        auto inverse = hopcroft_input.initInverse();
        auto states = inverse.initStates(inverse_size);
        auto actions = inverse.initActions(inverse_size);
        auto index = inverse.initIndex(state_decode.size() + 1);
        size_t inverse_index = 0;
        for (state_code_t i = 0; i < state_decode.size(); i++) {
          index.set(i, inverse_index);
          for (const auto& sa : inverse_map[state_decode[i]]) {
            state_code_t s_code = state_encode[sa.first];
            action_code_t a_code = action_encode[sa.second];
            states.set(inverse_index, s_code);
            actions.set(inverse_index, a_code);
            inverse_index++;
          }
        }
        KJ_ASSERT(inverse_index == inverse_size, inverse_index, inverse_size);
        index.set(state_decode.size(), inverse_index);
      }

      auto initialPartition = hopcroft_input.initInitialPartition(state_decode.size());
      AdventurerStateMap<uint32_t> partition_map;
      for (state_code_t i = 0; i < state_decode.size(); i++) {
        AdventurerState s = state_decode[i];
        // coarsen the state
        s.sp_[0] = 0;
        s.sp_[1] = 0;
        s.sp_[2] = 0;
        auto it = partition_map.find(s);
        uint32_t v;
        if (it == partition_map.end()) {
          v = partition_map.size();
          partition_map.emplace(s, v);
        } else {
          v = it->second;
        }
        initialPartition.set(i, v);
      }
      KJ_LOG(INFO, partition_map.size(), "initial number of partitions");
    }
    capnp::MallocMessageBuilder hopcroft_output_msg;
    auto hopcroft_output = hopcroft_output_msg.initRoot<HopcroftOutput>();

    hopcroft(hopcroft_input, &hopcroft_output);

    return true;
  }

private:

  template <typename T>
  static kj::Own<typename T::Reader> read(const char* fn) {
    int fd = open(fn, O_RDONLY);
    auto r = capnp::clone(capnp::StreamFdMessageReader(fd).getRoot<T>());
    close(fd);
    return r;
  }

  // Indexed stat retrieval

  ActionStat::Reader getComboStat(size_t i) {
    return weapon_class_->getXStats()[i];
  }

  ActionStat::Reader getSkillStat(size_t i) {
    switch (i) {
      case 0:
        return adventurer_->getS1Stat();
      case 1:
        return adventurer_->getS2Stat();
      case 2:
        return weapon_->getS3Stat();
      default:
        KJ_ASSERT(0, i, "out of bounds");
    }
  }

  // Selector functions

  static std::optional<size_t> afterSkillIndex(AfterAction after) {
    switch (after) {
      case AfterAction::AFTER_S1: return 0;
      case AfterAction::AFTER_S2: return 1;
      case AfterAction::AFTER_S3: return 2;
      default: return std::nullopt;
    }
  }

  static std::optional<size_t> afterComboIndex(AfterAction after) {
    switch (after) {
      case AfterAction::AFTER_C1: return 0;
      case AfterAction::AFTER_C2: return 1;
      case AfterAction::AFTER_C3: return 2;
      case AfterAction::AFTER_C4: return 3;
      case AfterAction::AFTER_C5: return 4;
      default: return std::nullopt;
    }
  }

  static std::optional<size_t> skillIndex(Action a) {
    switch (a) {
      case Action::S1: return 0;
      case Action::S2: return 1;
      case Action::S3: return 2;
      default: return std::nullopt;
    }
  }

  // State machine transition function

  uint32_t afterActionSp(AfterAction after) {
    switch (after) {
      case AfterAction::AFTER_FS:
        return weapon_class_->getFsStat().getSp();
      case AfterAction::AFTER_S1:
      case AfterAction::AFTER_S2:
      case AfterAction::AFTER_S3:
        return 0;
      case AfterAction::AFTER_C1:
      case AfterAction::AFTER_C2:
      case AfterAction::AFTER_C3:
      case AfterAction::AFTER_C4:
      case AfterAction::AFTER_C5:
        return getComboStat(*afterComboIndex(after)).getSp();
      case AfterAction::AFTER_NOTHING:
        return 0;
    }
  }

  std::optional<AdventurerState> applyAction(AdventurerState prev, Action a) {
    auto after = prev;

    // Wait for recovery to see if we can legally skill
    after.uiHiddenFramesLeft_ = std::max(
        0,
        static_cast<int32_t>(prev.uiHiddenFramesLeft_) -
        static_cast<int32_t>(prevRecoveryFrames(prev.afterAction_, a))
    );

    // Check if we can legally skill, and
    // apply effects of skill if so.
    auto mb_skill_index = skillIndex(a);
    if (mb_skill_index) {
      if (after.uiHiddenFramesLeft_ > 0) return std::nullopt;
      if (after.sp_[*mb_skill_index] != getSkillStat(*mb_skill_index).getSp()) return std::nullopt;
      after.uiHiddenFramesLeft_ = ui_hidden_frames_;
      after.sp_[*mb_skill_index] = 0;
    }

    // Apply state machine change
    switch (a) {
      case Action::FS:
        after.afterAction_ = AfterAction::AFTER_FS;
        break;
      case Action::X:
        switch (after.afterAction_) {
          case AfterAction::AFTER_C1:
            after.afterAction_ = AfterAction::AFTER_C2;
            break;
          case AfterAction::AFTER_C2:
            after.afterAction_ = AfterAction::AFTER_C3;
            break;
          case AfterAction::AFTER_C3:
            after.afterAction_ = AfterAction::AFTER_C4;
            break;
          case AfterAction::AFTER_C4:
            after.afterAction_ = AfterAction::AFTER_C5;
            break;
          default:
            after.afterAction_ = AfterAction::AFTER_C1;
            break;
        }
        break;
      case Action::S1:
        after.afterAction_ = AfterAction::AFTER_S1;
        break;
      case Action::S2:
        after.afterAction_ = AfterAction::AFTER_S2;
        break;
      case Action::S3:
        after.afterAction_ = AfterAction::AFTER_S3;
        break;
    }

    // Account for startup cost in UI
    after.uiHiddenFramesLeft_ = std::max(
        0,
        static_cast<int32_t>(after.uiHiddenFramesLeft_) -
        static_cast<int32_t>(afterStartupFrames(prev.afterAction_, a, after.afterAction_))
    );

    // Apply skill change
    for (size_t i = 0; i < num_skills_; i++) {
      after.sp_[i] = std::min(
        // SP is combo dependent, that's why we feed it afterAction
        after.sp_[i] + afterActionSp(after.afterAction_),
        getSkillStat(i).getSp()
      );
    }
    return after;
  }

  // Frame computation

  // Invariant: action in this state is legal
  frames_t prevRecoveryFrames(AfterAction prev, Action a) {

    // Some actions can cancel recovery frames
    bool cancelling;
    switch (a) {
      case Action::S1:
      case Action::S2:
      case Action::S3:
        cancelling = true;
        break;
      default:
        cancelling = false;
        break;
    }

    // Process recovery frames, cancelling if applicable
    switch (prev) {
      case AfterAction::AFTER_C1:
      case AfterAction::AFTER_C2:
      case AfterAction::AFTER_C3:
      case AfterAction::AFTER_C4:
      case AfterAction::AFTER_C5:
        // skills cancel basic combos
        if (skillIndex(a)) return 0;
        // fs cancels basic combos on some weapon types
        if (weapon_class_->getXfsStartups().size() &&
            a == Action::FS) return 0;
        return getComboStat(*afterComboIndex(prev)).getTiming().getRecovery();
      case AfterAction::AFTER_FS:
        // skills cancel force strikes
        if (skillIndex(a)) return 0;
        // NB: force strikes do not cancel force strikes
        return weapon_class_->getFsStat().getTiming().getRecovery();
      case AfterAction::AFTER_S1:
      case AfterAction::AFTER_S2:
      case AfterAction::AFTER_S3:
        // nothing cancels skills
        return getSkillStat(*afterSkillIndex(prev)).getTiming().getRecovery();
      case AfterAction::AFTER_NOTHING:
        return 0;
    }
  }

  frames_t afterStartupFrames(AfterAction prev, Action a, AfterAction after) {
    switch (a) {
      case Action::S1:
      case Action::S2:
      case Action::S3:
        return getSkillStat(*skillIndex(a)).getTiming().getStartup();
      case Action::X:
        return getComboStat(*afterComboIndex(after)).getTiming().getStartup();
      case Action::FS:
        // Startup time is adjusted on some weapon types (XFS rule)
        // Note use of prev; after here is always AFTER_FS!
        auto mb_combo_index = afterComboIndex(prev);
        if (mb_combo_index && weapon_class_->getXfsStartups().size()) {
          return weapon_class_->getXfsStartups()[*mb_combo_index];
        } else {
          return weapon_class_->getFsStat().getTiming().getStartup();
        }
    }
  }

  // Compute the number of frames to get to the next action point.
  // Note that this considers the recovery of your previous action
  // (not the one here), but NOT the recovery of this action.  The
  // transition is assumed to be legal.
  frames_t computeFrames(AfterAction prev, Action a, AfterAction after) {
    return prevRecoveryFrames(prev, a) +
           afterStartupFrames(prev, a, after);
  }

  kj::Own<WeaponClass::Reader> weapon_class_;
  kj::Own<Weapon::Reader> weapon_;
  kj::Own<Adventurer::Reader> adventurer_;

  size_t num_skills_ = 2;  // can toggle to two
  frames_t ui_hidden_frames_ = 114;

  kj::ProcessContext& context;
};

KJ_MAIN(DLGrindMain);
