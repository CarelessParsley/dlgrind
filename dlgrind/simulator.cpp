#include <dlgrind/simulator.h>

#include <cmath>

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

size_t Simulator::getNumSkills() {
  if (num_skills_) {
    return *num_skills_;
  }
  switch (config_->getWeapon().getWtype()) {
    // State space on these is too large to handle S3
    case WeaponType::BLADE:
    case WeaponType::SWORD:
    case WeaponType::WAND:
      return 2;
    default:
      return 3;
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

static uint8_t getSkillPrep(AdventurerName name) {
  switch (name) {
    case AdventurerName::HEINWALD: return 100;
    case AdventurerName::AMANE: return 75;
    default: return 0;
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

double Simulator::afterActionDmg(AfterAction after) {
  switch (after) {
    case AfterAction::AFTER_FS:
      return config_->getWeaponClass().getFsStat().getDmg();
    case AfterAction::AFTER_S1:
    case AfterAction::AFTER_S2:
    case AfterAction::AFTER_S3:
      return getSkillStat(*afterSkillIndex(after)).getDmg();
    case AfterAction::AFTER_C1:
    case AfterAction::AFTER_C2:
    case AfterAction::AFTER_C3:
    case AfterAction::AFTER_C4:
    case AfterAction::AFTER_C5:
      return getComboStat(*afterComboIndex(after)).getDmg();
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
    AdventurerState prev, Action a, frames_t* frames_out, double* dmg_out) {

  if (dmg_out) *dmg_out = 0;

  auto after = prev;

  frames_t frames = 0;

  // Repeated FS is not allowed
  // TODO: make this toggleable
  if (prev.afterAction_ == AfterAction::AFTER_FS && a == Action::FS) {
    return std::nullopt;
  }

  // Cancelling staff FS is not allowed (you don't get the hits)
  if (prev.afterAction_ == AfterAction::AFTER_FS && skillIndex(a)) {
    return std::nullopt;
  }

  // Apply delayed hits, if applicable

  frames_t hit_delay = hitDelay(prev.afterAction_);
  frames_t prevFrames = prevRecoveryFrames(prev.afterAction_, a);

  if (hit_delay > 0 && hit_delay <= prevFrames) {
    after = applyHit(after, a, dmg_out);
  }

  // Wait for recovery to see if we can legally skill
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

  if (hit_delay > prevFrames) {
    after = applyHit(after, a, dmg_out);
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

  // Not enough memory to handle this
  // KJ_ASSERT(hit_delay < prevFrames + afterFrames, hit_delay, prevFrames, afterFrames);

  if (hitDelay(after.afterAction_) == 0) {
    after = applyHit(after, a, dmg_out);
  }

  // Apply skill effects
  if (config_->getWeapon().getName() == WeaponName::AXE5B1 && a == Action::S3) {
    after.buffFramesLeft_[2] = 20 * 60;
  }
  if (config_->getAdventurer().getName() == AdventurerName::HEINWALD && a == Action::S2) {
    if (after.buffFramesLeft_[1] > 0) {
      after.buffFramesLeft_[0] = after.buffFramesLeft_[1];
    }
    after.buffFramesLeft_[1] = 10 * 60;
  }
  if (config_->getAdventurer().getName() == AdventurerName::AMANE && a == Action::S2) {
    if (after.buffFramesLeft_[1] > 0) {
      after.buffFramesLeft_[0] = after.buffFramesLeft_[1];
    }
    after.buffFramesLeft_[1] = 10 * 60;
  }

  if (frames_out) *frames_out = frames;
  return after;
}

AdventurerState Simulator::applyHit(AdventurerState after, Action a, double* dmg_out) {
  // Apply skill SP change
  float haste = config_->getAdventurer().getModifiers().getSkillHaste();
  // skill haste buffs here:
  // (currently none)
  for (size_t i = 0; i < getNumSkills(); i++) {
    uint16_t new_sp = after.sp_[i] +
      static_cast<uint16_t>(ceil(static_cast<float>(afterActionSp(after.afterAction_)) * (1. + haste)));
    if (new_sp > getSkillStat(i).getSp()) {
      after.sp_[i] = getSkillStat(i).getSp();
    } else {
      after.sp_[i] = new_sp;
    }
  }

  // Compute damage
  //  (NB: some units have to compute damage after skill effects;
  //  e.g., Alfonse S1 and Serena. Be careful!)
  {
    double dmg = 5./3;
    dmg *= config_->getAdventurer().getBaseStrength();
    dmg *= (1. + config_->getAdventurer().getModifiers().getStrength());
    dmg *= (1. + config_->getAdventurer().getCoabilityModifiers().getStrength());
    // strength buffs here:
    if (config_->getAdventurer().getName() == AdventurerName::HEINWALD && after.buffFramesLeft_[1] > 0) {
      dmg *= 1.2;
    }
    if (config_->getAdventurer().getName() == AdventurerName::HEINWALD && after.buffFramesLeft_[0] > 0) {
      dmg *= 1.2;
    }
    if (config_->getAdventurer().getName() == AdventurerName::AMANE && after.buffFramesLeft_[1] > 0) {
      dmg *= 1.15;
    }
    if (config_->getAdventurer().getName() == AdventurerName::AMANE && after.buffFramesLeft_[0] > 0) {
      dmg *= 1.15;
    }
    if (config_->getAdventurer().getName() == AdventurerName::ANNELIE && after.buffFramesLeft_[0] > 0) {
      dmg *= 1.20;
    }
    // modifier
    if (config_->getAdventurer().getName() == AdventurerName::ANNELIE && after.afterAction_ == AfterAction::AFTER_S1) {
      switch (after.skillShift_[0]) {
        case 0:
          dmg *= .1 + 8.14;
          break;
        case 1:
          dmg *= .1 * 2 + 2 * 4.07;
          break;
        case 2:
          dmg *= .1 * 3 + 3 * 3.54;
          break;
        default:
          KJ_ASSERT(0, after.skillShift_[0]);
      }
    } else {
      dmg *= afterActionDmg(after.afterAction_) / 100.;
    }
    if (skillIndex(a)) {
      dmg *= (1. + config_->getAdventurer().getModifiers().getSkillDmg());
      dmg *= (1. + config_->getAdventurer().getCoabilityModifiers().getSkillDmg());
      // skill dmg buffs here:
      // (none)
    } else if (a == Action::FS) {
      dmg *= (1. + config_->getAdventurer().getModifiers().getFsDmg());
    }
    dmg /= 10.;
    double crit_rate = config_->getAdventurer().getModifiers().getCritRate()
      + config_->getAdventurer().getCoabilityModifiers().getCritRate();
    double crit_dmg = config_->getAdventurer().getModifiers().getCritDmg() + 0.7;
    // crit buffs here:
    if (config_->getWeapon().getName() == WeaponName::AXE5B1 &&
        after.buffFramesLeft_[2] > 0) {
      crit_dmg += 0.50;
    }
    // energy
    if (after.energy_ == 5) {
      dmg *= 1.5;
      // don't adjust here, we'll handle below
    }
    dmg *= 1 + crit_rate * crit_dmg;
    dmg *= 1.5;
    /*
    // ODPS
    if (after.afterAction_ == AfterAction::AFTER_FS) {
      dmg *= 4;
    }
    */
    if (dmg_out) *dmg_out += dmg;
  }

  // Apply skill state change
  if (config_->getAdventurer().getName() == AdventurerName::ANNELIE) {
    auto prev_energy = after.energy_;
    if (after.energy_ == 5) {
      after.energy_ = 0;
    } else {
      if (after.afterAction_ == AfterAction::AFTER_S1) {
        if (after.skillShift_[0] == 0) {
          after.energy_ = std::min(after.energy_ + 1, 5);
        } else if (after.skillShift_[0] == 1) {
          after.energy_ = std::min(after.energy_ + 2, 5);
        }
      } else if (after.afterAction_ == AfterAction::AFTER_S2) {
        after.energy_ = std::min(after.energy_ + 2, 5);
      }
    }
    if (after.afterAction_ == AfterAction::AFTER_S1) {
      if (after.skillShift_[0] == 0) {
        after.skillShift_[0] = 1;
      } else if (after.skillShift_[0] == 1) {
        after.skillShift_[0] = 2;
      } else {
        after.skillShift_[0] = 0;
      }
    }
    // Energized: Strength +20% for 15s
    if (after.energy_ == 5 && prev_energy != 5) {
      after.buffFramesLeft_[0] = 15 * 60;
    }
  }

  return after;
}

AdventurerState Simulator::applyPrep(AdventurerState prev, std::optional<uint8_t> mb_prep) {
  AdventurerState after = prev;
  uint8_t prep = mb_prep.value_or(getSkillPrep(config_->getAdventurer().getName()));
  KJ_LOG(INFO, prep, "skill prep");
  for (size_t i = 0; i < getNumSkills(); i++) {
    // NB: Rounds down
    after.sp_[i] = getSkillStat(i).getSp() * prep / 100;
  }
  return after;
}

frames_t Simulator::hitDelay(AfterAction after) {
  switch (after) {
    case AfterAction::AFTER_C1:
    case AfterAction::AFTER_C2:
    case AfterAction::AFTER_C3:
    case AfterAction::AFTER_C4:
    case AfterAction::AFTER_C5:
    case AfterAction::AFTER_FS:
      switch (config_->getWeapon().getWtype()) {
        case WeaponType::STAFF:
        case WeaponType::WAND:
        case WeaponType::BOW:
          // TODO: Handle Bow FS
          return projectile_delay_;
        default:
          return 0;
      }
    // TODO: Some skills have delays
    default:
      return 0;
  }
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
