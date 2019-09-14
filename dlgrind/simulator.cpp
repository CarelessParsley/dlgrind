#include <dlgrind/simulator.h>

// Indexed stat retrieval

ActionStat::Reader Simulator::getComboStat(size_t i) {
  return config_->getWeaponClass().getXStats()[i];
}

ActionStat::Reader Simulator::getSkillStat(size_t i) {
  switch (i) {
    case 0:
      return config_->getAdventurer().getS1Stat();
    case 1:
      return config_->getAdventurer().getS2Stat();
    case 2:
      return config_->getWeapon().getS3Stat();
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

uint32_t Simulator::afterActionSp(AfterAction after) {
  switch (after) {
    case AfterAction::AFTER_FS:
      return config_->getWeaponClass().getFsStat().getSp();
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

/*
 * The most important picture
 *     _____________________________.  when the action actually happens
 *    /
 * [ AP ]   recovery   |   startup   [ AP ]
 *  prev             input            after
 *
 */

std::optional<AdventurerState> Simulator::applyAction(
    AdventurerState prev, Action a, frames_t* frames_out) {

  auto after = prev;

  frames_t frames = 0;

  // Wait for recovery to see if we can legally skill
  frames_t prevFrames = prevRecoveryFrames(prev.afterAction_, a);
  after.advanceFrames(prevFrames);
  frames += prevFrames;

  // Check if we can legally skill, and
  // apply effects of skill if so.
  auto mb_skill_index = skillIndex(a);
  if (mb_skill_index) {
    if (after.uiHiddenFramesLeft_ > 0) {
      // Apply delay from UI
      frames_t delayFrames = after.uiHiddenFramesLeft_;
      after.advanceFrames(delayFrames);
      frames += delayFrames;
    }
    if (after.sp_[*mb_skill_index] < getSkillStat(*mb_skill_index).getSp()) {
      return std::nullopt;
    }
    KJ_ASSERT(after.uiHiddenFramesLeft_ == 0);
    after.uiHiddenFramesLeft_ = ui_hidden_frames_;
    after.sp_[*mb_skill_index] = 0;
  }

  // Apply state machine change
  switch (a) {
    case Action::FS:
      after.afterAction_ = AfterAction::AFTER_FS;
      break;
    case Action::X:
      switch (prev.afterAction_) {
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
  frames_t afterFrames = afterStartupFrames(prev.afterAction_, a, after.afterAction_);
  after.advanceFrames(afterFrames);
  frames += afterFrames;

  // Apply skill effects
  if (config_->getWeapon().getName() == WeaponName::AXE5B1 && a == Action::S3) {
    after.buffFramesLeft_ = 20 * 60;
  }

  // Apply skill SP change
  for (size_t i = 0; i < num_skills_; i++) {
    after.sp_[i] = std::min(
      // SP is combo dependent, that's why we feed it afterAction
      after.sp_[i] + afterActionSp(after.afterAction_),
      getSkillStat(i).getSp()
    );
  }

  if (frames_out) *frames_out = frames;
  return after;
}

// Invariant: action in this state is legal
frames_t Simulator::prevRecoveryFrames(AfterAction prev, Action a) {
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
      if (config_->getWeaponClass().getXfsStartups().size() &&
          a == Action::FS) return 0;
      return getComboStat(*afterComboIndex(prev)).getTiming().getRecovery();
    case AfterAction::AFTER_FS:
      // skills cancel force strikes
      if (skillIndex(a)) return 0;
      // NB: force strikes do not cancel force strikes
      return config_->getWeaponClass().getFsStat().getTiming().getRecovery();
    case AfterAction::AFTER_S1:
    case AfterAction::AFTER_S2:
    case AfterAction::AFTER_S3:
      // nothing cancels skills
      return getSkillStat(*afterSkillIndex(prev)).getTiming().getRecovery();
    case AfterAction::AFTER_NOTHING:
      return 0;
  }
}

frames_t Simulator::afterStartupFrames(AfterAction prev, Action a, AfterAction after) {
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
      if (mb_combo_index && config_->getWeaponClass().getXfsStartups().size()) {
        return config_->getWeaponClass().getXfsStartups()[*mb_combo_index];
      } else {
        return config_->getWeaponClass().getFsStat().getTiming().getStartup();
      }
  }
}
