#include "callbacks.hpp"
#include "helpers.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>

// --- wlr-foreing-toplevel Callbacks ---

void handle_toplevel_handle_title(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle,
    const char *title) {
        // Find the ToplevelInfo associated with this handle
        ToplevelInfo *info = static_cast<ToplevelInfo*>(data); // Get from data
        if (!info || info->closed) return; // Safety Check
        if(title){ //Check for null
            update_title(info, title);
        }
    }

void handle_toplevel_handle_app_id(void *data,
   struct zwlr_foreign_toplevel_handle_v1 *handle,
   const char *app_id) {
    ToplevelInfo *info = static_cast<ToplevelInfo*>(data); // Get from data
    if (!info || info->closed) return;
    
    update_app_id(info, app_id ? app_id : "");
}

void handle_toplevel_handle_output_enter(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    struct wl_output *output) {}
    
void handle_toplevel_handle_output_leave(
    void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
    struct wl_output *output) {}
    
    
void handle_toplevel_handle_done(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle
) {}
    
void handle_toplevel_handle_closed(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle
) {
    ToplevelInfo *info = static_cast<ToplevelInfo*>(data);
    if (!info) return;
    
    // If already closed, ignore subsequent events.
    if (info->closed) {
        //zwlr_foreign_toplevel_handle_v1_destroy(handle);
        return;
    }
    
    // Mark the object as closed.
    info->closed = true;
    
    // Remove from the global list.
    auto &toplevels = g_ctx.toplevels;
    auto it = std::find_if(toplevels.begin(), toplevels.end(), [handle](const ToplevelInfo* i) {
            return i->handle == handle;
        });
    if (it != toplevels.end()) {
        toplevels.erase(it);
    }
    
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
    
    // Instead of deleting immediately, add to the pending deletion list.
    g_ctx.pending_deletions.push_back(info);
}

void handle_toplevel_handle_state(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle,
    wl_array *state
){
    ToplevelInfo *info = static_cast<ToplevelInfo *>(data);
    if (!info || info->closed) return;
    
    // Reset to false, then update based on the event.
    info->active = false;

    bool maximized = false,
    minimized = false,
    activated = false,
    fullscreen = false;
    
    uint32_t *buffer = new uint32_t[state->size / sizeof(uint32_t)];
    std::memcpy(buffer, state->data, state->size);
    uint32_t *state_element = buffer;

    for (size_t i = 0; i < state->size / sizeof(uint32_t); ++i) {
        state_element = &buffer[i];
        switch (*state_element) {
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED:
                maximized = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED:
                minimized = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED:
                activated = true;
                info->active = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN:
                fullscreen = true;
                break;
            }
        }
    delete[] buffer;
    buffer = nullptr;
}
    
void handle_toplevel_handle_parent(void *data,
    struct zwlr_foreign_toplevel_handle_v1 *handle,
    struct zwlr_foreign_toplevel_handle_v1 *parent
){
    ToplevelInfo *info = static_cast<ToplevelInfo*>(data); // Get from data
    if (!info || info->closed) return;
    info->parent = parent;
}
    
const zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_listener = {
    .title = handle_toplevel_handle_title,
    .app_id = handle_toplevel_handle_app_id,
    .output_enter = handle_toplevel_handle_output_enter,
    .output_leave = handle_toplevel_handle_output_leave,
    .state = handle_toplevel_handle_state,
    .done = handle_toplevel_handle_done,
    .closed = handle_toplevel_handle_closed,
    .parent = handle_toplevel_handle_parent,
};

void handle_toplevel(
    [[maybe_unused]] void *data,
    struct zwlr_foreign_toplevel_manager_v1 *manager,
    struct zwlr_foreign_toplevel_handle_v1 *handle
) {
    // Create and add toplevel
    ToplevelInfo *info = new ToplevelInfo;
    info->handle = handle;
    g_ctx.toplevels.push_back(info);
    zwlr_foreign_toplevel_handle_v1_add_listener(handle,
        &toplevel_handle_listener, info); // Pass the ToplevelInfo*
    wl_display_roundtrip(g_ctx.display); // Wait for handle events
}

void handle_finished(
    [[maybe_unused]] void *data,
    [[maybe_unused]] struct zwlr_foreign_toplevel_manager_v1 *manager) {
    std::cerr << "Error: wlr_foreign_toplevel_manager_v1 finished/not supported."
            << std::endl;
    exit(1);
}


const struct zwlr_foreign_toplevel_manager_v1_listener
    toplevel_manager_listener = {
        .toplevel = handle_toplevel,
        .finished = handle_finished,
};

// -- Wayland Registry Callbacks --

void registry_handle_global(
    [[maybe_unused]] void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    [[maybe_unused]] uint32_t version
) {
    if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        g_ctx.toplevel_manager = static_cast<zwlr_foreign_toplevel_manager_v1 *>(
            wl_registry_bind(registry,
                name,
                &zwlr_foreign_toplevel_manager_v1_interface,
                3)
            ); // Bind to version 3!
        zwlr_foreign_toplevel_manager_v1_add_listener(
            g_ctx.toplevel_manager,
            &toplevel_manager_listener,
            nullptr
        );
    }
}

void registry_handle_global_remove(
    [[maybe_unused]] void *data,
    [[maybe_unused]] struct wl_registry *registry,
    [[maybe_unused]] uint32_t name
) {}


const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};