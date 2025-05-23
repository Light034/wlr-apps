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
  * `-q <type>` Allows you to sort out the output by id (how recent the app was open) and the app_id (grouping multiple windows of the same app together). Allows you to sort by ascending or descending order.
    * `0` Disable sorting
    * `1` Sort by id in ascending order (Oldest to newest).
    * `2` Sort by id in descending order (Newest to oldest).
    * `3` Sort by app_id in ascending order (A -> Z).
    * `4` Sort by app_id in descending order (Z -> A).
  * `-h` Prints the help message and quit.


## To Do: 
- [x] Display more toplevel states beyond just `active`, such as `maximized`, `minimized`, `fullscreen`, etc.
- [x] Sorting: Allow sorting of toplevel output by different criteria.
- [x] Handle errors correctly rather than segfaulting.
- [ ] Testing on other compositors. (Please let me know if it doesnt work for your set-up).
- [x] Implement window management so that you can activate, maximize, minimize, and fullscreen using this program.
- [ ] Check on what output the toplevel is located at and print that information out. (Useful for multi screen set-ups)
- [ ] Add ability to group apps into a single app_id to allow multiple windows of the same app to be grouped together.
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

## Example:
* Launch app in continous mode with json and sorting enabled by id (Oldest to newest).
  *  `wlr-apps -mjq 1`
* Send event to focus toplevel with id 1.
  * `wlr-apps -x "f 1"`
* Send event to switch sorting mode to app_id in descending order.
 * `wlr-apps -x "q 4`

## Known bugs:
  * The app_id returned by wayland depends on compositor, most compositors supporting this protocol return the `.desktop` file name without the `.desktop`. Implementing this in `eww` like in the example provided causes some apps to not have any icon present. This is a bug with how `-gtk-icontheme()` works and not with this program. The solution would be to add gtk support and a function to check if the `app_id` returns an icon, if not then the app would manually search for the icon path. However, this is outside the scope of this program and not planned.
  * When launched in client mode the current logic doesn't allow for `<output_id>` to be parsed, meaning requesting fullscreen doesn't let you select on which output you wish to fullscreen the toplevel. Right now it fullscreens on the active output so you can move the window to the output you wish to focus. Since I don't need this feature on client/server mode I didn't implement the extra parsing of the output_id. If you would like this to be added please let me know through an issue and I'll work to fix it.
