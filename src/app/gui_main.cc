// gui_main.cc — the desktop entry point.
//
// app_shell's Wayland bootstrap owns main(): it does the things that have to
// happen before an application exists — the crash handler, the log file, the
// timer resolution — and calls this once they are done. This is the app's half,
// and it is deliberately tiny. Everything that used to be around it (creating a
// window, registering a class, a wnd_proc, a PeekMessage loop) is the host's
// now, which is the whole reason the same DemoApp also runs on a phone.
#include <cstdio>
#include <memory>
#include <utility>

#include "app_main.hh"   // the entry point this file DEFINES
#include "host.hh"
#include "log.hh"

#include "demo_app.hh"

int app_shell_main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    VCE_LOGI("ui_demo", "desktop build");

    auto host = make_host();
    if (!host) {
        std::fprintf(stderr, "ui_demo: failed to create a host\n");
        return 1;
    }

    DemoApp app(std::move(host));
    if (!app.create()) {
        std::fprintf(stderr, "ui_demo: create() failed\n");
        return 1;
    }
    app.run();
    return 0;
}
