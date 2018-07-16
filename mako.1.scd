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

# GLOBAL CONFIGURATION OPTIONS

*--max-visible* _n_
	Set maximum number of visible notifications to _n_. Older notifications will
	be hidden. If -1, all notifications are visible.

	Default: 5

*--sort* _+/-time_ | _+/-priority_
	Sorts incoming notifications by time and/or priority in ascending(+)
	or descending(-) order.

	Default: -time

*--output* _name_
	Show notifications on the specified output. If empty, notifications will
	appear on the focused output.

	Requires the compositor to support the Wayland protocol
	xdg-output-unstable-v1 version 2.

	Default: ""

*--anchor* _corner_
	Show notifications in the specified corner of the output. Supported values
	are _top-right_, _bottom-right_, _bottom-left_ and _top-left_.

	Default: _top-right_

# STYLE OPTIONS

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

*--default-timeout* _timeout_
	Set the default timeout to _timeout_ in milliseconds. To disable the
	timeout, set it to zero.

	Default: 0

*--ignore-timeout*
	If set, mako will ignore the expire timeout sent by notifications and use
	the one provided by _default-timeout_ instead.

	Default: 0

# CONFIG FILE

The config file is located at *~/.config/mako/config* or at
*$XDG\_CONFIG\_HOME/mako/config*. Each line of the form:

	key=value

Is equivalent to passing *--key=value* to mako from the command line.

Empty lines and lines that begin with # are ignored.

# CRITERIA

In addition to the set of options at the top of the file, the config file may
contain zero or more sections, each containing any combination of the
*STYLE OPTIONS*. The sections, called criteria, are defined with an INI-like
square bracket syntax. The brackets may contain any number of fields, like so:

	\[field=value field2=value2 ...\]

When a notification is received, it will be compared to the fields defined in
each criteria. If all of the fields match, the style options within will be
applied to the notification. Fields not included in the criteria are not
considered during the match. A notification may match any number of criteria.
This matching occurs in the order the criteria are defined in the config file,
meaning that if multiple criteria match a notification, the last occurrence of
any given style option will "win".

The following fields are available in critiera:

- _app-name_ (string)
- _app-icon_ (string)
- _urgency_ (one of "low", "normal", "high")
- _category_ (string)
- _desktop-entry_ (string)
- _actionable_ (boolean)
- _expiring_ (boolean)
- _hidden_ (boolean)
	- _hidden_ is special, it defines the style for the placeholder shown when
	  the number of notifications exceeds _max-visible_.

If a field's value contains special characters, they may be escaped with a
backslash, or quoted:

	\[app-name="Google Chrome"\]

	\[app-name=Google\\ Chrome\]

Quotes within quotes may also be escaped, and a literal backslack may be
specified as \\\\. No spaces are allowed around the equal sign. Escaping equal
signs within values is unnecessary.

Additionally, boolean values may be specified using any of true/false, 0/1, or
as bare words:

	\[actionable=true\] \[actionable=1\] \[actionable\]

	\[actionable=false\] \[actionable=0\] \[!actionable\]

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

## For notifications

*%a*	Application name

*%s*	Notification summary

*%b*	Notification body

## For the hidden notifications placeholder

*%h*	Number of hidden notifications

*%t*	Total number of notifications

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about mako development, see
https://github.com/emersion/mako.

# SEE ALSO

*makoctl*(1)
