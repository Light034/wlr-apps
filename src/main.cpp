#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include "wayland_context.hpp"
#include "helpers.hpp"
#include "callbacks.hpp"
#include <gtkmm/application.h>
#include <gtkmm/icontheme.h>

// Define the global context.
WaylandContext g_ctx;

int main(int argc, char *argv[]) {
    // Signal handler for graceful exit.
    signal(SIGINT, [](int) {
        std::cerr << "Exiting gracefully..." << std::endl;
        if (g_ctx.toplevel_manager) {
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(g_ctx.toplevel_manager));
        }
        if (g_ctx.registry) {
            wl_registry_destroy(g_ctx.registry);
        }
        if (g_ctx.display) {
            wl_display_disconnect(g_ctx.display);
        }
        exit(0);
    });

    // Initialize GTK before connecting to Wayland.
    auto app = Gtk::Application::create(argc, argv, "org.gtkmm.example");

    // Parse command-line arguments.
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--follow") {
            g_ctx.follow_mode = true;
        } else if (arg == "--compact") {
            g_ctx.compact_mode = true;
        }
    }

    // Connect to Wayland display.
    g_ctx.display = wl_display_connect(nullptr);
    if (!g_ctx.display) {
        std::cerr << "Failed to connect to Wayland display." << std::endl;
        return 1;
    }

    // Get registry and add listener
    g_ctx.registry = wl_display_get_registry(g_ctx.display);
    wl_registry_add_listener(g_ctx.registry, &registry_listener, nullptr);
    wl_display_roundtrip(g_ctx.display); // Wait for roundtrip.

    if (!g_ctx.toplevel_manager) {
        std::cerr << "Compositor does not support wlr-foreign-toplevel-management." << std::endl;
        return 1;
    }

    // Initial toplevel events.
    wl_display_roundtrip(g_ctx.display);
    print_toplevel_info();

    if (g_ctx.follow_mode) {
        while (wl_display_dispatch(g_ctx.display) != -1) {
            flush_pending_deletions();
            print_toplevel_info();
        }
    }

    return 0;
}