# mako

A lightweight notification daemon for Wayland. Works on Sway.

<p align="center">
  <img src="https://sr.ht/meoc.png" alt="mako screenshot">
</p>

## Running

If you're using Sway you can start mako on launch by putting `exec mako` in
your configuration file.

## Building

Install dependencies:

* meson (build-time dependency)
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
