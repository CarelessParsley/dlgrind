#include <kj/main.h>

class DLGrindMain {
public:
  explicit DLGrindMain(kj::ProcessContext& context)
      : context(context) {}
  kj::MainFunc getMain() {
    return kj::MainBuilder(context, "dlgrind",
        "Compute optimal rotations for characters in Dragalia Lost")
      .callAfterParsing(KJ_BIND_METHOD(*this, run))
      .build();
  }

  kj::MainBuilder::Validity run() {
    return true;
  }

private:
  kj::ProcessContext& context;
};

KJ_MAIN(DLGrindMain);
