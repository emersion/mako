# mako

A lightweight notification daemon for Wayland. Works on Sway.

<p align="center">
  <img src="https://sr.ht/meoc.png" alt="mako screenshot">
</p>

mako implements the [FreeDesktop Notifications Specification][spec].

Feel free to join the IRC channel: #emersion on irc.libera.chat.

## Running

`mako` requires an active session bus to function properly. 

If you are using SystemD `mako` will run automatically when a notification is emitted.
This happens via D-Bus activation, so you don't really need to explicitly 
start it up (this also allows delaying its startup time and speed up system startup).

If you are not using systemd, you might need to manually start a dbus user session
with the compositor of your choice from the display manager. The command run should
look something like this: `dbus-launch --exit-with-session COMPOSITOR`.

### Multiple notification daemons
If you have several notification daemons installed though, you might want to
explicitly start this one.
This is usually done by putting `exec mako` in your compositor's configuration file.

## Configuration

`mako` can be extensively configured and customized - feel free to read more
using the command `man 5 mako`

For control of mako during runtime, `makoctl` can be used; see `man makoctl`

## Building

Install dependencies:

* meson (build-time dependency)
* wayland
* pango
* cairo
* systemd, elogind or [basu] (for the sd-bus library)
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

[spec]: https://specifications.freedesktop.org/notification-spec/notification-spec-latest.html
[basu]: https://github.com/emersion/basu
