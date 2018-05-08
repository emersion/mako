# mako

A lightweight notification daemon for Wayland.

It currently works on Sway 1.0 alpha.

<p align="center">
  <img src="https://sr.ht/frOL.jpg" alt="mako"/>
</p>

## Building

Install dependencies:
* meson
* wayland
* pango
* cairo
* systemd or elogind (for the sd-bus library)
* pkg-config

Then run:

```shell
meson build
ninja -C build
build/mako
```

## License

MIT
