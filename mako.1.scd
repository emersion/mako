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

*--margin* _px_
	Set margin to _px_ pixels from the edge of the screen.

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

# COLORS

Colors can be specified as _#RRGGBB_ or _#RRGGBBAA_.

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
