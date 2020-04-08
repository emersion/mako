# mako

A lightweight notification daemon for Wayland. Works on Sway.

<p align="center">
  <img src="https://sr.ht/meoc.png" alt="mako screenshot">
</p>

mako implements the [GNOME Desktop Notifications Specification][gnome-draft].

Feel free to join the IRC channel: ##emersion on irc.freenode.net.

## Running


`mako` will run automatically when a notification is emitted. This happens via
D-Bus activation, so you don't really need to explicitly start it up (this also
allows delaying its startup time and speed up system startup).

If you have several notification daemons installed though, you might want to
explicitly start this one. Some ways of achieving this is:

- If you're using Sway you can start mako on launch by putting `exec mako` in
  your configuration file.

- If you are using elogind, you might need to manually start a dbus user
  session: `dbus-daemon --session --address=unix:path=$XDG_RUNTIME_DIR/bus`

## Building

Install dependencies:

* meson (build-time dependency)
* wayland
* pango
* cairo
* systemd or elogind (for the sd-bus library)
* gdk-pixbuf (optional, for icons support)
* dbus (runtime dependency, user-session support is required)
* scdoc (optional, for man pages)
* jq (optional, runtime dependency)

Then run:

```shell
meson build
ninja -C build
build/mako
```

<p align="center">
  <img src="https://sr.ht/frOL.jpg" alt="mako">
</p>

## I have a question!

See the [faq section in the wiki](https://github.com/emersion/mako/wiki/Frequently-asked-questions).

## License

MIT

[gnome-draft]: https://developer.gnome.org/notification-spec/
