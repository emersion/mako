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

*--layer* _layer_
	Arrange mako at the specified layer, relative to normal windows. Supported
	values are _background_, _bottom_, _top_, and _overlay_. Using _overlay_
	will cause notifications to be displayed above fullscreen windows, though
	this may also occur at _top_ depending on your compositor.

	Default: top

*--anchor* _position_
	Show notifications at the specified position on the output. Supported values
	are _top-right_, _bottom-right_, _bottom-center_, _bottom-left_, _top-left_
	and _top-center_.

	Default: top-right

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

*--padding* _directional_
	Set padding on each side to the size specified by _directional_. See
	*DIRECTIONAL VALUES* for more information.

	Default: 5

*--border-size* _px_
	Set popup border size to _px_ pixels.

	Default: 1

*--border-color* _color_
	Set popup border color to _color_. See *COLORS* for more information.

	Default: #4C7899FF

*--progress-color* [over|source] _color_
	Set popup progress indicator color to _color_. See *COLOR* for more
	information. To draw the progess indicator on top of the background
	color, use the *over* attribute. To replace the background color, use
	the *source* attribute (this can be useful when the notification is
	semi-transparent).

	Default: over #5588AAFF

*--max-icon-size* _px_
	Set maximum icon size to _px_ pixels.

	Default: 64

*--markup* 0|1
	If 1, enable Pango markup. If 0, disable Pango markup. If enabled, Pango
	markup will be interpreted in your format specifier and in the body of
	notifications.

	Default: 1

*--actions* 0|1
	Applications may request an action to be associated with activating a
	notification. Disabling this will cause mako to ignore these requests.

	Default: 1

*--format* _format_
	Set notification format string to _format_. See *FORMAT SPECIFIERS* for
	more information. To change this for grouped notifications, set it within
	a _grouped_ criteria.

	Default: <b>%s</b>\\n%b
	Default when grouped: (%g) <b>%s</b>\\n%b

*--default-timeout* _timeout_
	Set the default timeout to _timeout_ in milliseconds. To disable the
	timeout, set it to zero.

	Default: 0

*--ignore-timeout* 0|1
	If set, mako will ignore the expire timeout sent by notifications and use
	the one provided by _default-timeout_ instead.

	Default: 0

*--group-by* _field[,field,...]_
	A comma-separated list of criteria fields that will be compared to other
	visible notifications to determine if this one should form a group with
	them. All listed criteria must be exactly equal for two notifications to
	group.

	Default: none

# CRITERIA-ONLY STYLE OPTIONS

Some style options are not useful in the global context and therefore have no
associated command-line option.

*invisible* 0|1
	Whether this notification should be invisible even if it is above the
	_max-visible_ cutoff. This is used primarily for hiding members of groups.
	If you want to make more than the first group member visible, turn this
	option off within a _group-index_ criteria.

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
- _grouped_ (boolean)
	- Whether the notification is grouped with any others (its group-index is
	  not -1).
- _group-index_ (int)
	- The notification's index within its group, or -1 if it is not grouped.
- _hidden_ (boolean)
	- _hidden_ is special, it defines the style for the placeholder shown when
	  the number of notifications or groups exceeds _max-visible_.

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

There are three criteria always present at the front of the list:
- An empty criteria which matches all notifications and contains the defaults
  for all style options, overwritten with any configured in the global section.
- \[grouped\], which sets the default *format* for grouped notifications and
  sets them *invisible*.
- \[group-index=0\], which makes the first member of each group visible again.

These options can be overridden by simply defining the criteria yourself and
overriding them.

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

*\\\\*	Literal "\\"

*\\n*	New Line

## For notifications

*%a*	Application name

*%s*	Notification summary

*%b*	Notification body

*%g*	Number of notifications in the current group

## For the hidden notifications placeholder

*%h*	Number of hidden notifications

*%t*	Total number of notifications

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about mako development, see
https://github.com/emersion/mako.

# SEE ALSO

*makoctl*(1)
