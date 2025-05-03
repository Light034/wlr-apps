# Changelog

All changes will be documented here.

## 0.3 (02.05.2025)

### Changes
Added the option to sort the output based on id or app_id, this fixes the behaviour where some apps would randomly change place when some toplevels are removed.

## 0.2 (29.04.2025)

### Changes
With this release the entire code has been rewritten, I based the program from the wlr-clients repository, specifically with foreign_toplevel.c
which provides the basic functionality that this first program created. The way the program works has been fundamentally changed and now it uses a different
approach to managing and printing information about toplevels.
- Switched from C++ to C since the upstream code is written in C and I felt no need to translate it back to C++.
- Continous mode now works as a daemon, able to receive events to perform actions from a UNIX socket.
- Introduced client mode, which only sends the event to the already open socket rather than doing all the logic to connect to wayland.
- Removed compact json mode and now the program when run with json enabled will print compact json by default. To make it pretty pipe the output to jq.

### Internal changes.
- Code rewritten in C.
- Completely removed any logic that prints gtk icon path, eww already provides this option and it adds a significant performance hit.
- Made the code a single file, the previous split was to test out how it works, but it made it cumbersome to edit functions. Since the behaviour of this program is fairly simple I decided to revert back to a single file.

## 0.1 (21.02.2025)

### BREAKING CHANGES
- Removed logic to retrieve Gtk icon path, as I just found out you can search for icons with eww's included css.

### Changes
- Remove gtk icon path lookup and dependencies.
- Add example config looking up icons with eww's gtk css.

### Internal changes
- Comented out references to `get_icon_path` function and removed dependency from meson.build. If needed uncomment the references to this function in `helpers.cpp`, `helpers.hpp`, and `main.cpp`.
