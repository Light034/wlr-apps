# wlr-apps

This program intends to be a simple way to use `wlr-foreign-toplevel-management` to retrieve information directly from the compositor and print it out in json format. Its main purpose is to be used with [`eww`](https://github.com/elkowar/eww) to create a widget that displays the current active toplevels.

This code has been modified from [`wlr-clients/foreign_toplevel.c`](https://gitlab.freedesktop.org/wlroots/wlr-clients/) to add extra functionality and allow it to work with `eww` or any other program where getting a constant update of the toplevels is important.

Inside the directory `examples` you can find a simple **eww** widget that uses this program and displays the active toplevels.

---

# **Usage**

  * `wlr-apps [OPTIONS] [ARGUMENT]...`
  *  If no argument is given it runs only once, displays the toplevel information, and exits.
  * `-m` Continously monitors for changes and outputs the toplevels that got updated with the new information.
  * `-j` Prints the output in json format in compact form. Use it along `m` to get continous output in json.
  * `-f <id>` Requests the focus of the specified id. Run the program without argument to get a list of id's.
  * `-s <id>` Requests the specified toplevel to become fullscreen.
  * `-o <output_id>` Select the output for fullscreen toplevel to appear on. Use this with `-s`. View available outputs with wayland-info.
  * `-S <id>` Requests the toplevel to unfullscreen.
  * `-a <id>` Requests toplevel to maximize.
  * `-u <id>` Requests toplevel to unmaximize.
  * `-i <id>` Requests toplevel to minimize.
  * `-r <id>` Requests toplevel to restore(unminimize).
  * `-c <id>` Requests toplevel to close.
  * `-x "<opt> <id>"` Launches the program in client mode and sends an event to the main instance for it to perform an action. The `<opt>` follows the same convention as the normal `[OPTIONS]` but without the `-`, it needs to be only 1 letter and the id. Make sure to surround the option and id in double qoutes for the server to detect it.
  * `-h` Prints the help message and quit.


## To Do: 
- [x] Display more toplevel states beyond just `active`, such as `maximized`, `minimized`, `fullscreen`, etc.
- [ ] Sorting: Allow sorting of toplevel output by different criteria.
- [x] More Robust Error Handling:  Improve error handling, right now its almost non-existent.
- [ ] Testing on other compositors. (Please let me know if it doesnt work for your set-up).
- [x] Implement window management so that you can activate, maximize, minimize, and fullscreen using this program.
#

Contributions, bug reports, and pull requests are very welcomed.

## **Dependencies:**

  * Wayland client libraries
  * `wlr-foreign-toplevel-management` protocol client library (typically provided by `wlr-protocols`)

## Build:
To build this program just clone this repository and run:
```
meson setup build
ninja -C build
```

## Known bugs:
  * The app_id returned by wayland depends on compositor, most compositors supporting this protocol return the `.desktop` file name without the `.desktop`. Implementing this in `eww` like in the example provided causes some apps to not have any icon present. This is a bug with how `-gtk-icontheme()` works and not with this program. The solution would be to add gtk support and a function to check if the `app_id` returns an icon, if not then the app would manually search for the icon path. However, this is outside the scope of this program and not planned.
  * When launched in client mode the current logic doesn't allow for `<output_id>` to be parsed, meaning requesting fullscreen doesn't work when working in server/client mode. Since I have no real need to request fullscreen of a toplevel through this program (my compositor already has a shortcut for this) I decided to release it like this. If anyone needs this feature please open an issue and I'll modify the logic to allow this, contributions for this feature are welcome as well.
