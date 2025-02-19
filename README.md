# wlr-apps

This program intends to be a simple way to use `wlr-foreign-toplevel-management` to retrieve information directly from the compositor and print it out in json format. Its main purpose is to be used with [`eww`](https://github.com/elkowar/eww) to create a widget that displays the current active toplevels.

Inside the directory `examples` you can find a simple **eww** widget that uses this program and displays the active toplevels. In the example, I used [`wlctrl`](https://git.sr.ht/~brocellous/wlrctl) to activate the widget that was clicked, this is not perfect since multiple apps might use the same `title` and in that case it will activate only the first match.

---
### Disclaimer:

<img src=".github/it-werks-on-my-machine-works-on-my-machine-sticker.png" height="100" align="left"/>

This is my first c++ program intended for everyday use and since I'm not very familiar with coding in general there might be a lot of mistakes that cause bugs, crashes, or some unintended behaviour, I relied heavily on AI to get this code up and running from my original attempt, so you might find some AI generated shenanigans inside functions.
This program was only tested using Arch Linux with Wayfire as compositor. It **should**[^1]  work with any other compositor that implements this protocol, but your mileage may vary.
[^1]:It works on my machine.

---

# **Usage**

  *  If no argument is given it runs only once, displays the json, and exits.
  * `--follow` the program continuously monitors toplevel window changes and updates the JSON output in real-time.
  * `--compact` argument produces minified JSON output without indentation.



## To Do: 
- [ ] Display more toplevel states beyond just `active`, such as `maximized`, `minimized`, `fullscreen`, etc. (Currently partially implemented but I didn't finish it).
- [ ] Sorting: Allow sorting of toplevel output by different criteria.
- [ ] More Robust Error Handling:  Improve error handling, right now its almost non-existent.
- [ ] Testing on Other Compositors.
- [ ] Possibly implement window management so that you can activate, maximize, minimize, and fullscreen using this program.
#

Contributions, bug reports, and pull requests are very welcomed.

## **Dependencies:**

  * Wayland client libraries
  * `wlr-foreign-toplevel-management` protocol client library (typically provided by `wlr-protocols` or similar packages)
  * GTKmm

## Build:
To build this program just clone this repository and run:
```
meson setup build
ninja -C build
```
