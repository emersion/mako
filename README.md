# mako

A lightweight notification daemon for Wayland. Works on Sway.

<p align="center">
  <img src="https://sr.ht/meoc.png" alt="mako screenshot">
</p>

## Running

If you're using Sway you can start mako on launch by putting `exec mako` in
your configuration file.

If you are using elogind, you might need to manually start a dbus user session:
`dbus-daemon --session --address=unix:path=$XDG_RUNTIME_DIR/bus`

## Building

Install dependencies:

* meson (build-time dependency)
* wayland
* pango
* cairo
* systemd or elogind (for the sd-bus library)
* dbus (with user-session support)

Then run:

```shell
meson build
ninja -C build
build/mako
```

<p align="center">
  <img src="https://sr.ht/frOL.jpg" alt="mako">
</p>

## License

MIT
