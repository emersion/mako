# mako

A lightweight notification daemon for Wayland.

It currently works on Sway 1.0.

<p align="center">
  <img src="https://sr.ht/meoc.png" alt="mako screenshot">
</p>

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

<p align="center">
  <img src="https://sr.ht/frOL.jpg" alt="mako">
</p>

## License

MIT
