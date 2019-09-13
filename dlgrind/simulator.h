#pragma once

#include <dlgrind/schema.capnp.h>
#include <dlgrind/state.h>

#include <kj/debug.h>

#include <memory>
#include <optional>
#include <unordered_map>

class Simulator {
public:
  std::optional<AdventurerState> applyAction(
      AdventurerState prev,
      Action a,
      frames_t* frames_out = nullptr
      );

  frames_t computeFrames(AdventurerState prev, Action a) {
    frames_t r;
    auto ok = applyAction(prev, a, &r);
    KJ_ASSERT(!!ok);
    return r;
  }

  void setWeaponClass(kj::Own<WeaponClass::Reader> weapon_class) {
    weapon_class_ = std::move(weapon_class);
  }
  void setWeapon(kj::Own<Weapon::Reader> weapon) {
    weapon_ = std::move(weapon);
  }
  void setAdventurer(kj::Own<Adventurer::Reader> adventurer) {
    adventurer_ = std::move(adventurer);
  }

private:
  ActionStat::Reader getComboStat(size_t i);
  ActionStat::Reader getSkillStat(size_t i);
  uint32_t afterActionSp(AfterAction after);

  frames_t prevRecoveryFrames(AfterAction prev, Action a);
  frames_t afterStartupFrames(AfterAction prev, Action a, AfterAction after);

  kj::Own<WeaponClass::Reader> weapon_class_;
  kj::Own<Weapon::Reader> weapon_;
  kj::Own<Adventurer::Reader> adventurer_;

  size_t num_skills_ = 3;  // can toggle to two
  frames_t ui_hidden_frames_ = 114;
};
