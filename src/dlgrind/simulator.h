#pragma once

#include <dlgrind/schema.capnp.h>
#include <dlgrind/state.h>

#include <kj/debug.h>

#include <memory>
#include <optional>
#include <unordered_map>

#include <capnp/serialize.h>
#include <fcntl.h>
#include <unistd.h>

class Simulator {
public:
  std::optional<AdventurerState> applyAction(
      AdventurerState prev,
      Action a,
      frames_t* frames_out = nullptr,
      double* dmg_out = nullptr
      );

  AdventurerState applyPrep(
      AdventurerState prev,
      std::optional<uint8_t> prep = std::nullopt // percentage, e.g. 50 or 100
      );

  frames_t computeFrames(AdventurerState prev, Action a) {
    frames_t r;
    auto ok = applyAction(prev, a, &r);
    KJ_ASSERT(!!ok);
    return r;
  }

  void setConfig(kj::Own<Config::Reader> config) {
    config_ = std::move(config);
  }

  void setProjectileDelay(frames_t frames) {
    projectile_delay_ = frames;
  }

  void setNumSkills(size_t num_skills) {
    num_skills_ = num_skills;
  }

private:
  ActionStat::Reader getComboStat(size_t i);
  ActionStat::Reader getSkillStat(size_t i);
  size_t getNumSkills();
  uint32_t afterActionSp(AfterAction after);
  double afterActionDmg(AfterAction after);
  frames_t hitDelay(AfterAction after);

  AdventurerName adventurerName() { return config_->getAdventurer().getName(); }

  AdventurerState applyHit(AdventurerState, Action, double* dmg_out);

  frames_t prevRecoveryFrames(AfterAction prev, Action a);
  frames_t afterStartupFrames(AfterAction prev, Action a, AfterAction after);

  kj::Own<Config::Reader> config_;

  std::optional<size_t> num_skills_;
  frames_t ui_hidden_frames_ = 114;
  frames_t projectile_delay_ = 50;  // default to precharge computation
};
