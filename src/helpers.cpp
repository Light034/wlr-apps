#include "helpers.hpp"
#include <regex>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <json/json.h>
//#include <gtkmm/application.h>
//#include <gtkmm/icontheme.h>

// Flush pending deletions.
void flush_pending_deletions() {
    for (ToplevelInfo* info : g_ctx.pending_deletions) {
        delete info;
    }
    g_ctx.pending_deletions.clear();
}

// Prints the information retrieved in json format.
void print_toplevel_info() {
    Json::Value root(Json::arrayValue);
    for (const auto &info : g_ctx.toplevels) {
        if (!info || info->closed) continue;
        Json::Value toplevel_json;

        // This converts the app_id into lowercase, Gtk doesn't seem to handle
        // uppercase app names when looking up icons.
        std::string app_id_lower = info->app_id;
        std::transform(app_id_lower.begin(),
        app_id_lower.end(),
        app_id_lower.begin(),
        ::tolower);
        toplevel_json["app_id"] = app_id_lower;

        toplevel_json["title"] = info->title;
        toplevel_json["active"] = info->active;

        // To be deprecated, eww css uses gtk css which allows looking up icons
        // natively, I will keep the code here in case it's needed but it will no longer be used.
        //toplevel_json["icon_path"] = info->icon_path;

        if (info->parent) {
            toplevel_json["parent_app_id"] = "unknown";
            for(const auto& other_info : g_ctx.toplevels) {
                if (other_info && other_info->handle == info->parent) {
                    toplevel_json["parent_app_id"] = other_info->app_id;
                    break;
                }
            }
        }
        root.append(toplevel_json);
    }
    Json::StreamWriterBuilder writer;
    writer["indentation"] = g_ctx.compact_mode ? "" : " ";
    std::cout << Json::writeString(writer, root) << std::endl;
}

// Gathers the gtk icon using the theme that is set by gtk.
/* Deprecated function, will be removed in a future version
std::string get_icon_path(const std::string &app_id) {
    // If the app_id is empty then it returns an empty string.
    if (app_id.empty()) {
        return "";
    }

    // Check cache for already found entries.
    if (g_ctx.icon_cache.count(app_id)) {
        return g_ctx.icon_cache[app_id];
    }

    // Get the icon path using Gtk
    auto theme = Gtk::IconTheme::get_default();
    const int icon_size=48;
    const int flags = Gtk::ICON_LOOKUP_USE_BUILTIN | Gtk::ICON_LOOKUP_FORCE_SIZE;

    try {
        // Try an exact match first
        auto icon_info = theme->lookup_icon(app_id, icon_size, flags);

        if (icon_info) {
            std::string path = icon_info.get_filename();
            g_ctx.icon_cache[app_id] = path; // This stores the path in cache.
            return path;
        }

        // If no exact match found, try a case-insensitive regex match.
        std::regex pattern(".*" + app_id + ".*", std::regex_constants::icase);

        std::vector<Glib::ustring> icon_names = theme->list_icons();

        for (const auto &name : icon_names) {
            std::string name_str = name.raw();
            if (std::regex_match(name_str, pattern)) {
                auto icon_info = theme->lookup_icon(name_str, icon_size, flags);
                if (icon_info) {
                    std::string path = icon_info.get_filename();
                    if (!path.empty()) {
                        g_ctx.icon_cache[app_id] = path; // Store in cache.
                    }
                    return path;
                }
            }
        }
    } 
    catch (const Glib::Error &ex) {
        std::cerr << "Error: getting icon for " << app_id << ": " << ex.what()
        << '\n';
    }
    return ""; // Return empty string on failure, probably need to add fallback.
} */

// Update the title when it gets updated.
void update_title(ToplevelInfo *info, const std::string &title) {
    if (!info || info->closed) return;
    info->title = title;
}

// Update the app_id if it changes.
void update_app_id(ToplevelInfo *info, const std::string &app_id) {
    if (!info || info->closed) return;
    info->app_id = app_id;

    // Deprecated, uncomment if needed.
    //info->icon_path = get_icon_path(info->app_id);
}