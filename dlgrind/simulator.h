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

  frames_t computeFrames(AdventurerState prev, Action a) {
    frames_t r;
    auto ok = applyAction(prev, a, &r);
    KJ_ASSERT(!!ok);
    return r;
  }

  void setConfig(kj::Own<Config::Reader> config) {
    config_ = std::move(config);
  }

private:
  ActionStat::Reader getComboStat(size_t i);
  ActionStat::Reader getSkillStat(size_t i);
  uint32_t afterActionSp(AfterAction after);
  double afterActionDmg(AfterAction after);

  frames_t prevRecoveryFrames(AfterAction prev, Action a);
  frames_t afterStartupFrames(AfterAction prev, Action a, AfterAction after);

  kj::Own<Config::Reader> config_;

  size_t num_skills_ = 2;  // can toggle to two
  frames_t ui_hidden_frames_ = 114;
};
