# Changelog

All changes will be documented here.

## 0.1 (21.02.2025)

### BREAKING CHANGES
- Removed logic to retrieve Gtk icon path, as I just found out you can search for icons with eww's included css.

### Changes
- Remove gtk icon path lookup and dependencies.
- Add example config looking up icons with eww's gtk css.

### Internal changes
- Comented out references to `get_icon_path` function and removed dependency from meson.build. If needed uncomment the references to this function in `helpers.cpp`, `helpers.hpp`, and `main.cpp`.
