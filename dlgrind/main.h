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

  Config::Reader config_;
  Simulator sim_;
  kj::ProcessContext& context_;
  std::optional<kj::StringPtr> configFile_;
};
