#include <algorithm>
#include <regex>
#include <csignal>
#include <iostream>
#include <json/json.h>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <gtkmm/application.h>
#include <gtkmm/icontheme.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

struct ToplevelInfo {
    std::string app_id;
    std::string title;
    bool active = false;
    pid_t pid = 0;
    std::string icon_path;
    struct zwlr_foreign_toplevel_handle_v1 *handle = nullptr;
    struct wl_output *output = nullptr; // Store the output.
    struct zwlr_foreign_toplevel_handle_v1 *parent = nullptr;
    bool closed = false;
};

struct WaylandContext {
    struct wl_display *display = nullptr;
    struct wl_registry *registry = nullptr;
    struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager = nullptr;
    std::vector<ToplevelInfo*> toplevels;
    std::vector<ToplevelInfo*> pending_deletions;
    std::unordered_map<std::string, std::string> icon_cache; // Cache icon paths.
    bool follow_mode = false;
    bool compact_mode = false;
};

static WaylandContext g_ctx;

// --- Helper Functions ---

void flush_pending_deletions() {
  for (ToplevelInfo* info : g_ctx.pending_deletions) {
      delete info;
  }
  g_ctx.pending_deletions.clear();
}

void print_toplevel_info() {
  Json::Value root(Json::arrayValue);
  for (const auto &info : g_ctx.toplevels) {
    if (!info || info->closed) continue; // Safety check

    Json::Value toplevel_json;
    toplevel_json["app_id"] = info->app_id;
    toplevel_json["title"] = info->title;
    toplevel_json["active"] = info->active;
    //toplevel_json["pid"] = static_cast<Json::Int>(info->pid);
    toplevel_json["icon_path"] = info->icon_path;
    if (info->parent) {
        toplevel_json["parent_app_id"] = "unknown";
        for(const auto& other_info : g_ctx.toplevels){
          if (other_info && other_info->handle == info->parent) {
            toplevel_json["parent_app_id"] = other_info->app_id;
            break;
            }
        }
    }
    root.append(toplevel_json);
  }

  Json::StreamWriterBuilder writer;
  writer["indentation"] = g_ctx.compact_mode ? "" : "  ";
  std::cout << Json::writeString(writer, root) << std::endl;
}

std::string get_icon_path(const std::string &app_id) {
  // If the app_id is empty we return an empty string.
  if (app_id.empty()) {
    return "";
  }

  // Check cache first
  if (g_ctx.icon_cache.count(app_id)) {
    return g_ctx.icon_cache[app_id];
  }

  // Get icon path using Gtk::IconTheme
  auto theme = Gtk::IconTheme::get_default();
  const int icon_size = 48;
  const int flags = Gtk::ICON_LOOKUP_USE_BUILTIN | Gtk::ICON_LOOKUP_FORCE_SIZE;

  try {
    // Try an exact match first
    auto icon_info = theme->lookup_icon(app_id, icon_size, flags);

    if (icon_info) {
      std::string path = icon_info.get_filename();
      g_ctx.icon_cache[app_id] = path; // Store in cache
      return path;
    }

    // If no exact match, try a case-insensitive regex match
    std::regex pattern(".*" + app_id + ".*", std::regex_constants::icase);

    // Retrieve list of all available icons and try to find partial matches
    std::vector<Glib::ustring> icon_names = theme->list_icons();

    // Iterate over the list of icons until we find one that matches the regex
    for (const auto &name : icon_names) {
      std::string name_str = name.raw();
      if (std::regex_match(name_str, pattern)) {
        auto icon_info = theme->lookup_icon(name_str, icon_size, flags);
        if (icon_info) {
          std::string path = icon_info.get_filename();
          if (!path.empty()) {
          g_ctx.icon_cache[app_id] = path; // Store in cache
          }
          return path;
        }
      }
    }
  } catch (const Glib::Error &ex) {
    std::cerr << "Error: getting icon for " << app_id << ": " << ex.what()
              << '\n';
  }
  return ""; // Return empty string on failure
}

// --- Update Functions ---

void update_title(ToplevelInfo *info, const std::string &title) {
  if (!info || info->closed) return; // Safety check
  info->title = title;
}

void update_app_id(ToplevelInfo *info, const std::string &app_id) {
  if (!info || info->closed) return;
  info->app_id = app_id;
  info->icon_path = get_icon_path(info->app_id);
  /*
   if (!g_ctx.icon_cache.count(info->app_id)) {
        info->icon_path = get_icon_path(info->app_id);
   } else {
      info->icon_path = get_icon_path(info->app_id);
   }*/
}

// --- wlr-foreign-toplevel-maganement Callbacks ---


static void handle_toplevel_handle_title(void *data,
  struct zwlr_foreign_toplevel_handle_v1 *handle,
  const char *title) {
// Find the ToplevelInfo associated with this handle
ToplevelInfo *info = static_cast<ToplevelInfo*>(data); // Get from data
if (!info || info->closed) return; // Safety Check
if(title){ //Check for null
  update_title(info, title);
}
}

static void handle_toplevel_handle_app_id(void *data,
   struct zwlr_foreign_toplevel_handle_v1 *handle,
   const char *app_id) {
ToplevelInfo *info = static_cast<ToplevelInfo*>(data); // Get from data
if (!info || info->closed) return;
if(app_id && std::strlen(app_id) > 0){
  update_app_id(info, app_id);
} else {
  update_app_id(info, "");
}
}

static void handle_toplevel_handle_output_enter(
void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
struct wl_output *output) {}

static void handle_toplevel_handle_output_leave(
void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
struct wl_output *output) {}


static void handle_toplevel_handle_done(
void *data,
struct zwlr_foreign_toplevel_handle_v1 *handle) {}

static void handle_toplevel_handle_closed(
  void *data, struct zwlr_foreign_toplevel_handle_v1 *handle) {
    ToplevelInfo *info = static_cast<ToplevelInfo*>(data);
    if (!info)
        return;
    
    // If already closed, ignore subsequent events.
    if (info->closed) {
        zwlr_foreign_toplevel_handle_v1_destroy(handle);
        return;
    }
    
    // Mark the object as closed.
    info->closed = true;
    
    // Remove from the global list.
    auto &toplevels = g_ctx.toplevels;
    auto it = std::find_if(toplevels.begin(), toplevels.end(),
                          [handle](const ToplevelInfo* i) {
                              return i->handle == handle;
                          });
    if (it != toplevels.end()) {
        toplevels.erase(it);
    }
    
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
    
    // Instead of deleting immediately, add to the pending deletion list.
    g_ctx.pending_deletions.push_back(info);
}

static void handle_toplevel_handle_state(
  void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, wl_array *state){
    ToplevelInfo *info = static_cast<ToplevelInfo *>(data);
    if (!info || info->closed) return;
  
// Reset to false, then update based on the event.
  info->active = false;

  bool maximized = false, minimized = false, activated = false, fullscreen = false;
  uint32_t *buffer = new uint32_t[state->size / sizeof(uint32_t)];
  std::memcpy(buffer, state->data, state->size);
  uint32_t *state_element = buffer;

  if (!state_element) {
    std::cerr << "state_element is nullptr!" << std::endl;
    return;
  }

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
  
static void handle_toplevel_handle_parent(void *data,
  struct zwlr_foreign_toplevel_handle_v1 *handle,
  struct zwlr_foreign_toplevel_handle_v1 *parent){
    ToplevelInfo *info = static_cast<ToplevelInfo*>(data); // Get from data
    if (!info || info->closed) return;
    info->parent = parent;
  }
  
  static const zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_listener = {
    .title = handle_toplevel_handle_title,
    .app_id = handle_toplevel_handle_app_id,
    .output_enter = handle_toplevel_handle_output_enter,
    .output_leave = handle_toplevel_handle_output_leave,
    .state = handle_toplevel_handle_state,
    .done = handle_toplevel_handle_done,
    .closed = handle_toplevel_handle_closed,
    .parent = handle_toplevel_handle_parent,
  };

  static void handle_toplevel(
    [[maybe_unused]] void *data,
    struct zwlr_foreign_toplevel_manager_v1 *manager,
    struct zwlr_foreign_toplevel_handle_v1 *handle) {
    // Create and add toplevel
    ToplevelInfo *info = new ToplevelInfo;
    info->handle = handle;
    g_ctx.toplevels.push_back(info);
    zwlr_foreign_toplevel_handle_v1_add_listener(handle,
      &toplevel_handle_listener, info); // Pass the ToplevelInfo*
    wl_display_roundtrip(g_ctx.display); // Wait for handle events
}

static void handle_finished(
    [[maybe_unused]] void *data,
    [[maybe_unused]] struct zwlr_foreign_toplevel_manager_v1 *manager) {
  std::cerr << "Error: wlr_foreign_toplevel_manager_v1 finished/not supported."
            << std::endl;
  exit(1);
}


static const struct zwlr_foreign_toplevel_manager_v1_listener
    toplevel_manager_listener = {
        .toplevel = handle_toplevel,
        .finished = handle_finished,
};

// -- Wayland Registry Callbacks --

static void registry_handle_global(
  [[maybe_unused]] void *data, struct wl_registry *registry, uint32_t name,
  const char *interface, [[maybe_unused]] uint32_t version) {
if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
  g_ctx.toplevel_manager = static_cast<zwlr_foreign_toplevel_manager_v1 *>(
      wl_registry_bind(registry, name,
                       &zwlr_foreign_toplevel_manager_v1_interface,
                       3)); // Bind to version 3!
  zwlr_foreign_toplevel_manager_v1_add_listener(
      g_ctx.toplevel_manager, &toplevel_manager_listener, nullptr);
}
}

static void registry_handle_global_remove(
  [[maybe_unused]] void *data, [[maybe_unused]] struct wl_registry *registry,
  [[maybe_unused]] uint32_t name) {}


static const struct wl_registry_listener registry_listener = {
  .global = registry_handle_global,
  .global_remove = registry_handle_global_remove,
};

// --- Main ---

int main(int argc, char *argv[]) {
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
    // Initialize GTK *before* connecting to Wayland
    auto app = Gtk::Application::create(argc, argv, "org.gtkmm.example");

    for (int i = 1; i < argc; i++) {
      std::string arg(argv[i]);
      if (arg == "--follow") {
          g_ctx.follow_mode = true;
      } else if (arg == "--compact") {
          g_ctx.compact_mode = true;
      }
    }

    g_ctx.display = wl_display_connect(nullptr);
    if (!g_ctx.display) {
        std::cerr << "Failed to connect to Wayland display." << std::endl;
        return 1;
    }

    g_ctx.registry = wl_display_get_registry(g_ctx.display);
    wl_registry_add_listener(g_ctx.registry, &registry_listener, nullptr);
    wl_display_roundtrip(g_ctx.display);  // Wait for registry events

    if (!g_ctx.toplevel_manager) {
        std::cerr
            << "Compositor does not support wlr-foreign-toplevel-management."
            << std::endl;
        return 1;
    }

    // Initial list of windows.
    wl_display_roundtrip(g_ctx.display); // Wait for toplevel events
    print_toplevel_info();

    if (g_ctx.follow_mode) {
        while (wl_display_dispatch(g_ctx.display) != -1) {
          flush_pending_deletions();
          // Wayland event loop
          print_toplevel_info();
        }

    return 0;
  }
}
