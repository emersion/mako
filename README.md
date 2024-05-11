# mako

A lightweight notification daemon for Wayland. Works on Sway.

<p align="center">
  <img src="https://sr.ht/meoc.png" alt="mako screenshot">
</p>

mako implements the [FreeDesktop Notifications Specification][spec].

Feel free to join the IRC channel: #emersion on irc.libera.chat.

## Running


`mako` will run automatically when a notification is emitted. This happens via
D-Bus activation, so you don't really need to explicitly start it up (this also
allows delaying its startup time and speed up system startup).

If you have several notification daemons installed though, you might want to
explicitly start this one. Some ways of achieving this is:

- If you're using Sway you can start mako on launch by putting `exec mako` in
  your configuration file.

- If you are not using systemd, you might need to manually start a dbus user
  session: `dbus-daemon --session --address=unix:path=$XDG_RUNTIME_DIR/bus`

## Configuration

`mako` can be extensively configured and customized - feel free to read more
using the command `man 5 mako`

For control of mako during runtime, `makoctl` can be used; see `man makoctl`
<details>
<summary>Variable Explanation</summary>
<br>

| Variable            | Explanation                                                                                                 | Example Value            |
|---------------------|-------------------------------------------------------------------------------------------------------------|--------------------------|
| `max-history`       | Maximum number of notifications to keep in the history buffer.                                              | `5`                      |
| `sort`              | Arrangement of notifications based on time or priority (`+` for ascending, `-` for descending).             | `+time`                  |
| `on-button-left`    | Action when left button is clicked on a notification.                                                        | `invoke-default-action`  |
| `on-button-middle`  | Action when middle button is clicked on a notification.                                                      | `dismiss-group`          |
| `on-button-right`   | Action when right button is clicked on a notification.                                                       | `dismiss`                |
| `on-touch`          | Action when the notification is touched (e.g., on touchscreen devices).                                      | `invoke-default-action`  |
| `on-notify`         | Command to execute when a notification is displayed.                                                         | `exec mpv /usr/share/sounds/freedesktop/stereo/message.oga` |
| `font`              | Font style and size for notifications.                                                                       | `monospace 10`           |
| `background-color`  | Background color of notifications.                                                                           | `#000000`                |
| `text-color`        | Text color inside notifications.                                                                             | `#FFFFFF`                |
| `width`             | Width of notification popup in pixels.                                                                       | `299`                    |
| `height`            | Maximum height of notifications in pixels.                                                                   | `99`                     |
| `outer-margin`      | Margin around the notification block.                                                                        | `1`                      |
| `margin`            | Margin of each individual notification.                                                                      | `0`                      |
| `padding`           | Padding around the notification text.                                                                        | `10`                     |
| `border-size`       | Size of the notification border in pixels.                                                                   | `1`                      |
| `border-color`      | Color of the notification border.                                                                            | `#FFFFFF`                |
| `border-radius`     | Border radius of each notification in pixels.                                                                | `0`                      |
| `progress-color`    | Color of the progress indicator in notifications.                                                            | `over #0b1c1c`           |
| `icons`             | Show or hide icons in notifications.                                                                         | `1`                      |
| `max-icon-size`     | Maximum size of icons in notifications.                                                                      | `34`                     |
| `icon-location`     | Position of icons relative to text in notifications.                                                         | `left`                   |
| `actions`           | Allow applications to request actions in notifications.                                                      | `1`                      |
| `history`           | Save notifications that have reached their timeout into history buffer.                                      | `1`                      |
| `format`            | Format string for displaying notifications.                                                                   | `<b>%s</b>\n%b`          |
| `text-alignment`    | Alignment of text inside notifications.                                                                      | `center`                 |
| `default-timeout`   | Default timeout for notifications in milliseconds.                                                           | `10000`                  |
| `ignore-timeout`    | Ignore the expiration timeout of notifications.                                                               | `0`                      |
| `max-visible`       | Maximum number of visible notifications.                                                                     | `5`                      |
| `layer`             | Layer position of notifications relative to other windows.                                                    | `top`                    |
| `anchor`            | Position of notifications on the output (e.g., screen corner).                                                | `bottom-right`           |

You can use this table to explain each configuration variable used in your project's Mako notification setup.

</details>

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
