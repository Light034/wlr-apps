#pragma once
#include <string>
#include <vector>
#include "wayland_context.hpp"
#include <wayland-client.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include <unistd.h>

// Callback declarations for toplevel handle events.
void handle_toplevel_handle_title(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle,
    const char *title
);

void handle_toplevel_handle_app_id(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle,
    const char *title
);

void handle_toplevel_handle_output_enter(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle,
    struct wl_output *output
);

void handle_toplevel_handle_output_leave(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle,
    struct wl_output *output
);

void handle_toplevel_handle_done(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle
);

void handle_toplevel_handle_closed(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle
);

void handle_toplevel_handle_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    wl_array *state
);

void handle_toplevel_handle_parent(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle,
    struct zwlr_foreign_toplevel_handle_v1 *parent
);

// Global listener for toplevel handle events.
extern const zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_listener;

// Registry callbacks.
void registry_handle_global(void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char*interface,
    uint32_t version
);
void registry_handle_global_remove(void *data,
    struct wl_registry *registry,
    uint32_t name
);
extern const wl_registry_listener registry_listener;