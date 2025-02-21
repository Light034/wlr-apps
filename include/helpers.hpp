#pragma once
#include <string>
#include "wayland_context.hpp"

// Flushes ToplevelInfo objects marked for pending deletion.
void flush_pending_deletions();

// Prints the current Toplevel information as JSON.
void print_toplevel_info();

// Returns the icon path for the given app_id.
// std::string get_icon_path(const std::string &app_id);

// Update functions.
void update_title(ToplevelInfo *info, const std::string &title);
void update_app_id(ToplevelInfo *info, const std::string &app_id);