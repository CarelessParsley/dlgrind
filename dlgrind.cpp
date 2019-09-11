#include <kj/main.h>

#include <dlgrind/schema.capnp.h>
#include <dlgrind/hopcroft.h>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include <magic_enum.h>

#include <unordered_map>
#include <vector>

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

struct AdventurerStateM {
  AfterAction afterAction_;
  uint8_t uiHiddenFramesLeft_;
  uint16_t sp_[3];
};

using packed_state_t = uint64_t;

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

    // Compute reachable states
    using InverseMap = std::unordered_map<packed_state_t, std::vector<std::pair<AdventurerStateM, Action>>>;
    InverseMap inverse_map;
    {
      std::vector<AdventurerStateM> todo{
        {
          .afterAction_ = AfterAction::AFTER_NOTHING,
          .uiHiddenFramesLeft_ = 0,
          .sp_ = {0, 0, 0}
        }
      };
      while (todo.size()) {
        auto s = todo.back();
        todo.pop_back();
        auto push = [&](AdventurerStateM n_s, Action a) {
          auto code = packAdventurerStateM(n_s);
          if (inverse_map.count(code) == 0) {
            todo.emplace_back(n_s);
          }
          inverse_map[code].emplace_back(s, a);
        };
        // Skills
        // TODO: Option to never hold skills
        for (size_t i = 0; i < num_skills_; i++) {
          if (s.sp_[i] == getSkillStat(i).getSp()) {
            auto n_s = s;
            n_s.sp_[i] = 0;
            n_s.afterAction_ = afterSkill(i);
            // TODO: Handle buffs here, if necessary!
            push(n_s, skill(i));
          }
        }
        // Combo
        {
          auto n_s = s;
          size_t combo_index;
          std::tie(n_s.afterAction_, combo_index) = afterCombo(s.afterAction_);
          for (size_t i = 0; i < num_skills_; i++) {
            n_s.sp_[i] = std::min(
                n_s.sp_[i] + getComboStat(i).getSp(),
                getSkillStat(i).getSp()
            );
          }
          push(n_s, Action::X);
        }
        // Force strike
        {
          auto n_s = s;
          n_s.afterAction_ = AfterAction::AFTER_FS;
          for (size_t i = 0; i < num_skills_; i++) {
            n_s.sp_[i] = std::min(
                n_s.sp_[i] + weapon_class_->getFsStat().getSp(),
                getSkillStat(i).getSp()
            );
          }
          push(n_s, Action::FS);
        }
      }
    }

    KJ_LOG(INFO, inverse_map.size(), "initial states");

    // Number states and actions
    std::unordered_map<packed_state_t, state_code_t> state_encode;
    std::vector<AdventurerStateM> state_decode;

    capnp::MallocMessageBuilder hopcroft_input_msg;
    auto hopcroft_input = hopcroft_input_msg.initRoot<HopcroftInput>();
    hopcroft_input.setNumStates(inverse_map.size());
    hopcroft_input.setNumActions(magic_enum::enum_count<Action>());

    capnp::MallocMessageBuilder hopcroft_output;


    /*
    const int frames = 3600;

    const frames_t s1_recovery = 111;
    const frames_t s2_recovery = 114;
    */

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

  packed_state_t packAdventurerStateM(AdventurerStateM state) {

    uint64_t result = 0;
    uint64_t stride = 1;

    for (size_t i = 0; i < num_skills_; i++) {
      result += state.sp_[i] * stride;
      stride = checked_mul(stride, getSkillStat(i).getSp() + 1);
    }

    result += state.uiHiddenFramesLeft_ * stride;
    // NB: this actually can be reduced further:
    // 114 - min(skill_recovery) + 1
    // 114 is NOT off by one because there is no such thing as a
    // zero frame skill
    stride = checked_mul(stride, 114);

    result += enum_index(state.afterAction_);
    stride = checked_mul(stride, magic_enum::enum_count<AfterAction>());

    return result;
  }

  AdventurerStateM unpackAdventurerStateM(packed_state_t code) {
    AdventurerStateM state;
    for (size_t i = 0; i < num_skills_; i++) {
      uint64_t stride = getSkillStat(i).getSp() + 1;
      state.sp_[i] = code % stride;
      code /= stride;
    }
    {
      uint64_t stride = 114;
      state.uiHiddenFramesLeft_ = code % stride;
      code /= stride;
    }
    {
      uint64_t stride = magic_enum::enum_count<AfterAction>();
      state.afterAction_ = magic_enum::enum_value<AfterAction>(code % stride);
      code /= stride;
    }
    KJ_ASSERT(code == 1, code);
    return state;
  }

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

  static AfterAction afterSkill(size_t i) {
    switch (i) {
      case 0:
        return AfterAction::AFTER_S1;
      case 1:
        return AfterAction::AFTER_S2;
      case 2:
        return AfterAction::AFTER_S3;
      default: KJ_ASSERT(0, i, "out of bounds");
    }
  }

  static std::pair<AfterAction, int> afterCombo(AfterAction a) {
    switch (a) {
      case AfterAction::AFTER_C1: return {AfterAction::AFTER_C2, 1};
      case AfterAction::AFTER_C2: return {AfterAction::AFTER_C3, 2};
      case AfterAction::AFTER_C3: return {AfterAction::AFTER_C4, 3};
      case AfterAction::AFTER_C4: return {AfterAction::AFTER_C5, 4};
      default: return {AfterAction::AFTER_C1, 0};
    }
  }

  static Action skill(size_t i) {
    switch (i) {
      case 0: return Action::S1;
      case 1: return Action::S2;
      case 2: return Action::S3;
      default: KJ_ASSERT(0, i, "out of bounds");
    }
  }

  kj::Own<WeaponClass::Reader> weapon_class_;
  kj::Own<Weapon::Reader> weapon_;
  kj::Own<Adventurer::Reader> adventurer_;

  size_t num_skills_ = 3;  // can toggle to two

  kj::ProcessContext& context;
};

KJ_MAIN(DLGrindMain);
