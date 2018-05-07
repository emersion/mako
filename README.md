# mako

A lightweight notification daemon for Wayland.

It currently works on Sway 1.0 alpha.

## Building

Install dependencies:
* meson
* wayland
* pango
* cairo
* systemd or elogind (for the sd-bus library)

Then run:

```shell
meson build
ninja -C build
build/mako
```

## License

MIT
