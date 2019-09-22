#include <kj/main.h>

#include <dlgrind/schema.capnp.h>
#include <dlgrind/simulator.h>

class DLGrind {
protected:
  explicit DLGrind(kj::ProcessContext& context)
      : context_(context) {}

  kj::MainBuilder::Validity setConfig(kj::StringPtr config_fn) {
    configFile_ = config_fn;
    return true;
  }

  kj::MainBuilder::Validity setSkillPrep(kj::StringPtr percentage) {
    skill_prep_ = percentage.parseAs<uint8_t>();
    return true;
  }

  kj::MainBuilder::Validity setProjectileDelay(kj::StringPtr frames) {
    sim_.setProjectileDelay(frames.parseAs<frames_t>());
    return true;
  }

  void readConfig() {
    int fd;
    const char* fn;
    if (!configFile_) {
      fd = STDIN_FILENO;
      fn = "<stdin>";
    } else {
      fn = configFile_->cStr();
      fd = open(fn, O_RDONLY);
    }
    auto r = capnp::clone(capnp::StreamFdMessageReader(fd).getRoot<Config>());
    KJ_LOG(INFO, fn, *r);
    sim_.setConfig(std::move(r));
    close(fd);
  }

  Simulator sim_;
  kj::ProcessContext& context_;
  std::optional<kj::StringPtr> configFile_;
  std::optional<uint8_t> skill_prep_;
};
