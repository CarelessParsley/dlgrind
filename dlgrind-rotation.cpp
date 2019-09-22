#include <dlgrind/main.h>
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

class DLGrindRotation : DLGrind {
public:
  explicit DLGrindRotation(kj::ProcessContext& context)
      : DLGrind(context) {}
  kj::MainFunc getMain() {
    return kj::MainBuilder(context_, "dlgrind-rotation",
        "Simulate a fixed rotation, computing frame count")
      .addOptionWithArg({'c', "config"}, KJ_BIND_METHOD(*this, setConfig),
          "<filename>", "Read config from <filename>.")
      .addOptionWithArg({"skill-prep"}, KJ_BIND_METHOD(*this, setSkillPrep),
          "<percent>", "Skill prep percentage (e.g., 75).")
      .addOptionWithArg({"projectile-delay"}, KJ_BIND_METHOD(*this, setProjectileDelay),
          "<frames>", "Frames of delay behind projectile cast and hit (enables precharge).")
      .expectOneOrMoreArgs("<rotation>", KJ_BIND_METHOD(*this, setRotation))
      .callAfterParsing(KJ_BIND_METHOD(*this, run))
      .build();
  }

  kj::MainBuilder::Validity setRotation(kj::StringPtr action) {
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
    readConfig();

    frames_t frames = 0;
    double dmg = 0;
    AdventurerState st;
    st = sim_.applyPrep(st, skill_prep_);
    for (auto a : rotation_) {
      frames_t step_frames;
      double step_dmg;
      auto mb_st = sim_.applyAction(st, a, &step_frames, &step_dmg);
      frames += step_frames;
      dmg += step_dmg;
      KJ_ASSERT(!!mb_st);
      st = *mb_st;
      float time = static_cast<float>(frames) / 60;
      // KJ_LOG(INFO, time, a, step_dmg, st);
      std::cerr << time << " " << kj::str(a).cStr() << " " << step_dmg << " " << kj::str(st).cStr() << "\n";
    }

    std::cout << dmg << "\n";

    return true;
  }

private:
  std::vector<Action> rotation_;
};

KJ_MAIN(DLGrindRotation);
