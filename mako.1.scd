mako(1)

# NAME

mako - notification daemon for Wayland

# DESCRIPTION

*mako* [options...]

# SYNOPSIS

mako is a graphical notification daemon for Wayland compositors which support
the layer-shell protocol. Notifications received over dbus are displayed until
dismissed with a click or via *makoctl*(1).

# OPTIONS

*-h, --help*
	Show help message and quit.

*--font* _font_
	Set font to _font_, in Pango format.

	Default: monospace 10

*--background-color* _color_
	Set background color to _color_. See *COLORS* for more information.

	Default: #285577FF

*--text-color* _color_
	Set text color to _color_. See *COLORS* for more information.

	Default: #FFFFFFFF

*--width* _px_
	Set width of notification popups.

	Default: 300

*--height* _px_
	Set maximium height of notification popups. Notifications whose text takes
	up less space are shrunk to fit.

	Default: 100

*--margin* _directional_
	Set margin of each edge to the size specified by _directional_. See
	*DIRECTIONAL VALUES* for more information.

	Default: 10

*--padding* _px_
	Set padding to _px_ pixels from the edge of the popup.

	Default: 5

*--border-size* _px_
	Set popup border size to _px_ pixels.

	Default: 1

*--border-color* _color_
	Set popup border color to _color_. See *COLORS* for more information.

	Default: #4C7899FF

*--markup* 0|1
	If 1, enable Pango markup. If 0, disable Pango markup. If enabled, Pango
	markup will be interpreted in your format specifier and in the body of
	notifications.

	Default: 1

*--format* _format_
	Set notification format string to _format_. See *FORMAT SPECIFIERS* for
	more information.

	Default: <b>%s</b>\\n%b

*--max-visible* _n_
	Set maximum number of visible notifications to _n_. Older notifications will
	be hidden. If -1, all notifications are visible.

	Default: 5

*--default-timeout* _timeout_
	Set the default timeout to _timeout_ in milliseconds. To disable the
	timeout, set it to zero.

	Default: 0

*--output* _name_
	Show notifications on the specified output. If empty, notifications will
	appear on the focused output.

	Requires the compositor to support the Wayland protocol
	xdg-output-unstable-v1 version 2.

	Default: ""

# CONFIG FILE

The config file is located at *~/.config/mako/config* or at
*$XDG\_CONFIG\_HOME/mako/config*. Each line of the form:

	key=value

Is equivalent to passing *--key=value* to mako from the command line.

Empty lines and lines that begin with # are ignored.

# COLORS

Colors can be specified as _#RRGGBB_ or _#RRGGBBAA_.

# DIRECTIONAL VALUES

Some options set values that affect all four edges of a notification. These
options can be specified in several different ways, depending on how much
control over each edge is desired:

- A single value will apply to all four edges.
- Two values will set vertical and horizontal edges separately.
- Three will set top, horizontal, and bottom edges separately.
- Four will give each edge a separate value.

When specifying multiple values, they should be comma-separated. For example,
this would set the top margin to 10, left and right to 20, and bottom to five:

```
--margin 10,20,5
```

# FORMAT SPECIFIERS

Format specification works similarly to *printf*(3), but with a different set of
specifiers.

*%%*	Literal "%"

*%a*	Application name

*%s*	Notification summary

*%b*	Notification body

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about mako development, see
https://github.com/emersion/mako.

# SEE ALSO

*makoctl*(1)
