// main.cc — the whole of the demo's Android bootstrap.
//
// app_shell owns everything that has to happen before an application exists;
// what is left for the app to write is this. Build the host, build the app,
// run. The DemoApp constructed here is the same class gui_main.cc constructs
// on Wayland — nothing under src/app knows which one it is in.
#include <android_native_app_glue.h>

#include <memory>
#include <utility>

#include "android_host.hh"
#include "log.hh"

#include "demo_app.hh"

void android_main(android_app* state) {
    // requestAllFilesAccess = false. The demo reads its fonts and shaders out
    // of its own APK assets and never touches the filesystem, so asking for
    // MANAGE_EXTERNAL_STORAGE would be a Settings screen shown for nothing —
    // and the manifest does not carry the permission, so it would be a toggle
    // that cannot even be pressed.
    auto host = std::make_unique<AndroidHost>(state,
                                              /*launchExtraKey=*/nullptr,
                                              /*fallback=*/nullptr,
                                              /*requestAllFilesAccess=*/false);
    DemoApp app(std::move(host));
    if (!app.create()) {
        VCE_LOGE("ui_demo", "create() failed; nothing to run");
        return;
    }
    app.run();
}
