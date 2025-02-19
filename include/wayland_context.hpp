#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

struct ToplevelInfo {
    std::string app_id;
    std::string title;
    bool active = false;
    pid_t pid = 0;
    std::string icon_path;
    struct zwlr_foreign_toplevel_handle_v1 *handle = nullptr;
    struct wl_output *output = nullptr;
    struct zwlr_foreign_toplevel_handle_v1 *parent = nullptr;
    bool closed = false;
};

struct WaylandContext {
    struct wl_display *display = nullptr;
    struct wl_registry *registry = nullptr;
    struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager = nullptr;
    std::vector<ToplevelInfo*> toplevels;
    std::vector<ToplevelInfo*> pending_deletions;
    std::unordered_map<std::string, std::string> icon_cache;
    bool follow_mode = false;
    bool compact_mode = false;
};

extern WaylandContext g_ctx;