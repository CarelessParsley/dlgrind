#include <kj/main.h>

#include <dlgrind/schema.capnp.h>
#include <dlgrind/hopcroft.h>
#include <dlgrind/simulator.h>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include <magic_enum.h>

#include <iostream>
#include <unordered_map>
#include <vector>
#include <optional>

class DLGrindRotation {
public:
  explicit DLGrindRotation(kj::ProcessContext& context)
      : context(context) {}
  kj::MainFunc getMain() {
    return kj::MainBuilder(context, "dlgrind-rotation",
        "Simulate a fixed rotation, computing frame count")
      .expectOneOrMoreArgs("<rotation>", KJ_BIND_METHOD(*this, processInput))
      .callAfterParsing(KJ_BIND_METHOD(*this, run))
      .build();
  }

  kj::MainBuilder::Validity processInput(kj::StringPtr action) {
    if (action == "fs") {
      rotation_.emplace_back(Action::FS);
    } else if (action == "x") {
      rotation_.emplace_back(Action::X);
    } else if (action == "s1") {
      rotation_.emplace_back(Action::S1);
    } else if (action == "s2") {
      rotation_.emplace_back(Action::S2);
    } else if (action == "s3") {
      rotation_.emplace_back(Action::S3);
    } else if (action.startsWith("c")) {
      bool fs;
      uint8_t count;
      if (action.endsWith("fs")) {
        fs = true;
        count = heapString(action.slice(1, action.size()-2)).parseAs<uint8_t>();
      } else {
        fs = false;
        count = action.slice(1).parseAs<uint8_t>();
      }
      for (uint8_t i = 0; i < count; i++) {
        rotation_.emplace_back(Action::X);
      }
      if (fs) {
        rotation_.emplace_back(Action::FS);
      }
    }
    return true;
  }

  kj::MainBuilder::Validity run() {
    sim_.setWeaponClass(read<WeaponClass>("dat/axe.bin"));
    sim_.setWeapon(read<Weapon>("dat/axe5b1.bin"));
    sim_.setAdventurer(read<Adventurer>("dat/Erik.bin"));

    frames_t frames = 0;
    AdventurerState st;
    for (auto a : rotation_) {
      frames_t step_frames;
      auto mb_st = sim_.applyAction(st, a, &step_frames);
      frames += step_frames;
      KJ_ASSERT(!!mb_st);
      st = *mb_st;
      float time = static_cast<float>(frames) / 60;
      KJ_LOG(INFO, a, frames, time, st);
    }

    std::cout << static_cast<int>(frames) << "\n";
    std::cout << (static_cast<float>(frames)/60) << "\n";

    return true;
  }

private:
  std::vector<Action> rotation_;
  Simulator sim_;
  kj::ProcessContext& context;
};

KJ_MAIN(DLGrindRotation);