#pragma once

#include <dlgrind/schema.capnp.h>

#include <magic_enum.h>

#include <memory>
#include <optional>
#include <unordered_map>

using frames_t = uint32_t;

inline frames_t sub_floor_zero(frames_t a, frames_t b) {
  if (b > a) return 0;
  return a - b;
}

struct AdventurerState {
  AfterAction afterAction_ = AfterAction::AFTER_NOTHING;
  uint8_t uiHiddenFramesLeft_ = 0;
  uint16_t sp_[3] = {0, 0, 0};
  // Convention: usually, skills trigger buffs, so you put
  // the buff for a particular skill in that slot
  uint16_t buffFramesLeft_[3] = {0, 0, 0};

  void advanceFrames(frames_t frames) {
    uiHiddenFramesLeft_ = sub_floor_zero(uiHiddenFramesLeft_, frames);
    for (size_t i = 0; i < 3; i++) {
      buffFramesLeft_[i] = sub_floor_zero(buffFramesLeft_[i], frames);
    }
  }

  bool operator==(const AdventurerState& other) const {
    return afterAction_ == other.afterAction_ &&
           uiHiddenFramesLeft_ == other.uiHiddenFramesLeft_ &&
           sp_[0] == other.sp_[0] &&
           sp_[1] == other.sp_[1] &&
           sp_[2] == other.sp_[2] &&
           buffFramesLeft_[0] == other.buffFramesLeft_[0] &&
           buffFramesLeft_[1] == other.buffFramesLeft_[1] &&
           buffFramesLeft_[2] == other.buffFramesLeft_[2];
  }
};

inline uint KJ_HASHCODE(const AdventurerState& st) {
  return kj::hashCode(
      static_cast<uint>(st.afterAction_),
      st.uiHiddenFramesLeft_,
      st.sp_[0],
      st.sp_[1],
      st.sp_[2],
      st.buffFramesLeft_[0],
      st.buffFramesLeft_[1],
      st.buffFramesLeft_[2]
      );
}

inline kj::String KJ_STRINGIFY(const AdventurerState& st) {
  return kj::str("[sp=", st.sp_[0], ",", st.sp_[1], ",", st.sp_[2], "; c=", std::string(magic_enum::enum_name(st.afterAction_)), "; b=", st.buffFramesLeft_[0], ",", st.buffFramesLeft_[1], ",", st.buffFramesLeft_[2], "; ui=", st.uiHiddenFramesLeft_, "]");
}

struct AdventurerStateHasher {
  std::size_t operator()(const AdventurerState& st) const { return kj::hashCode(st); }
};

template <typename T>
using AdventurerStateMap = std::unordered_map<AdventurerState, T, AdventurerStateHasher>;

