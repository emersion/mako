mako(5)

# NAME

mako - configuration file

# DESCRIPTION

The config file is located at *~/.config/mako/config* or at
*$XDG\_CONFIG\_HOME/mako/config*. Option lines can be specified to configure
mako like so:

	key=value

Empty lines and lines that begin with # are ignored.

# GLOBAL CONFIGURATION OPTIONS

*max-history*=_n_
	Set maximum number of expired notifications to keep in the history
	buffer to _n_. If the buffer is full, newly expired notifications
	replace the oldest ones. If 0, history is disabled.

	Default: 5

*sort*=_+/-time_ | _+/-priority_
	Sorts incoming notifications by time and/or priority in ascending(+)
	or descending(-) order.

	Default: -time

# BINDING OPTIONS

Bindings allow one to perform an action when an event is triggered. Supported
values are _none_, _dismiss_, _dismiss-all_, _dismiss-group_,
_invoke-default-action_ and _exec <command>_.

*on-button-left*=_action_
	Default: invoke-default-action

*on-button-middle*=_action_
	Default: none

*on-button-right*=_action_
	Default: dismiss

*on-touch*=_action_
	Default: dismiss

*on-notify* = _action_
	Default: none

When _exec_ is used, the command will be executed in a POSIX shell. The
shell variable _id_ will be set to the notification ID. For example, the
following option will display an interactive action menu on middle click:

```
on-button-middle=exec makoctl menu -n "$id" dmenu -p 'Select action: '
```

The following option will play a sound when a new notification is opened:

```
on-notify=exec mpv /usr/share/sounds/freedesktop/stereo/message.oga
```

# STYLE OPTIONS

*font*=_font_
	Set font to _font_, in Pango format.

	Default: monospace 10

*background-color*=_color_
	Set background color to _color_. See *COLORS* for more information.

	Default: #285577FF

*text-color*=_color_
	Set text color to _color_. See *COLORS* for more information.

	Default: #FFFFFFFF

*width*=_px_
	Set width of notification popups.

	Default: 300

*height*=_px_
	Set maximum height of notification popups. Notifications whose text takes
	up less space are shrunk to fit.

	Default: 100

*outer-margin*=_directional_
	Set outer-margin of each edge to the size specified by _directional_. See
	*DIRECTIONAL VALUES* for more information.
	This margin applies to the outside of the whole notification list.

	Default: 0

*margin*=_directional_
	Set margin of each edge to the size specified by _directional_. See
	*DIRECTIONAL VALUES* for more information.
	This margin applies to each individual notification.
	Note that it applies in addition to outer-margin, meaning first and last
	notifications will use the sum of both margins.

	Default: 10

*padding*=_directional_
	Set padding on each side to the size specified by _directional_. See
	*DIRECTIONAL VALUES* for more information.

	Default: 5

*border-size*=_px_
	Set popup border size to _px_ pixels.

	Default: 2

*border-color*=_color_
	Set popup border color to _color_. See *COLORS* for more information.

	Default: #4C7899FF

*border-radius*=_px_
	Set popup corner radius to _px_ pixels.

	Default: 0

*progress-color*=[over|source] _color_
	Set popup progress indicator color to _color_. See *COLOR* for more
	information. To draw the progress indicator on top of the background
	color, use the *over* attribute. To replace the background color, use
	the *source* attribute (this can be useful when the notification is
	semi-transparent).

	Progress can be indicated in a notification by setting a hint, "value"
	to an integer between 0 and 100 inclusive.

	Default: over #5588AAFF

*icons*=0|1
	Show icons in notifications.

	Default: 1

*max-icon-size*=_px_
	Set maximum icon size to _px_ pixels.

	Default: 64

*icon-path*=_path_\[:_path_...\]
	Paths to search for icons when a notification specifies a name instead
	of a full path. Colon-delimited. This approximates the search algorithm
	used by the XDG Icon Theme Specification, but does not support any of
	the theme metadata. Therefore, if you want to search parent themes,
	you'll need to add them to the path manually.

	The path should be the root of the icon theme, the categories and
	resolutions will be searched for the most appropriate match.

	/usr/share/icons/hicolor and /usr/share/pixmaps are always searched.

	Default: ""

*icon-location*=_position_
	Position of the icon relative to the displayed text. Valid options are
	_left_, _right_, _top_ and _bottom_.

	Default: left

*markup*=0|1
	If 1, enable Pango markup. If 0, disable Pango markup. If enabled, Pango
	markup will be interpreted in your format specifier and in the body of
	notifications.

	Default: 1

*actions*=0|1
	Applications may request an action to be associated with activating a
	notification. Disabling this will cause mako to ignore these requests.

	Default: 1

*history*=0|1
	If set, mako will save notifications that have reached their timeout
	into the history buffer instead of immediately deleting them.
	_max-history_ determines the size of the history buffer.

	Default: 1

*format*=_format_
	Set notification format string to _format_. See *FORMAT SPECIFIERS* for
	more information. To change this for grouped notifications, set it within
	a _grouped_ criteria.

	Default: <b>%s</b>\\n%b++
Default when grouped: (%g) <b>%s</b>\\n%b

*text-alignment*=left|center|right
	Set notification text alignment.

	Default: left

*default-timeout*=_timeout_
	Set the default timeout to _timeout_ in milliseconds. To disable the
	timeout, set it to zero.

	Default: 0

*ignore-timeout*=0|1
	If set, mako will ignore the expire timeout sent by notifications and use
	the one provided by _default-timeout_ instead.

	Default: 0

*group-by*=_field[,field,...]_
	A comma-separated list of criteria fields that will be compared to other
	visible notifications to determine if this one should form a group with
	them. All listed criteria must be exactly equal for two notifications to
	group.

	Default: none

*max-visible*=_n_
	Set maximum number of visible notifications to _n_. Older notifications will
	be hidden. If -1, all notifications are visible.

	Default: 5

*output*=_name_
	Show notifications on the specified output. If empty, notifications will
	appear on the focused output.

	Requires the compositor to support the Wayland protocol
	xdg-output-unstable-v1 version 2.

	Default: ""

*layer*=_layer_
	Arrange mako at the specified layer, relative to normal windows. Supported
	values are _background_, _bottom_, _top_, and _overlay_. Using _overlay_
	will cause notifications to be displayed above fullscreen windows, though
	this may also occur at _top_ depending on your compositor.

	Default: top

*anchor*=_position_
	Show notifications at the specified position on the output. Supported values
	are _top-right_, _top-center_, _top-left_, _bottom-right_, _bottom-center_,
	_bottom-left_, _center-right_, _center-left_ and _center_.

	Default: top-right

# CRITERIA

In addition to the set of options at the top of the file, the config file may
contain zero or more sections, each containing any combination of the
*BINDING OPTIONS* and *STYLE OPTIONS*. The sections, called criteria, are
defined with an INI-like square bracket syntax. The brackets may contain any
number of fields, like so:

	\[field=value field2=value2 ...\]

When a notification is received, it will be compared to the fields defined in
each criteria. If all of the fields match, the style options within will be
applied to the notification. Fields not included in the criteria are not
considered during the match. A notification may match any number of criteria.
This matching occurs in the order the criteria are defined in the config file,
meaning that if multiple criteria match a notification, the last occurrence of
any given style option will "win".

The following fields are available in criteria:

- _app-name_ (string)
- _app-icon_ (string)
- _summary_ (string)
	- An exact match on the summary of the notification.
	- This field conflicts with _summary~_.
- _summary~_ (string)
	- A POSIX extended regular expression match on the summary of the
	  notification.
	- This field conflicts with _summary_.
- _body_ (string)
	- An exact match on the body of the notification.
	- This field conflicts with _body~_.
- _body~_ (string)
	- A POSIX extended regular expression match on the body of the
	notification.
	- This field conflicts with _body_.
- _urgency_ (one of "low", "normal", "critical")
- _category_ (string)
- _desktop-entry_ (string)
- _actionable_ (boolean)
- _expiring_ (boolean)
- _mode_ (string)
	- Only apply style options in this section if the provided mode is
	  currently enabled. For more information about modes, see the _MODES_
	  section.

The following fields are also available to match on a second pass based on
where previous style options decided to place each notification:

- _grouped_ (boolean)
	- Whether the notification is grouped with any others (its group-index is
	  not -1).
- _group-index_ (int)
	- The notification's index within its group, or -1 if it is not grouped.
- _hidden_ (boolean)
	- _hidden_ is special, it defines the style for the placeholder shown when
	  the number of notifications or groups exceeds _max-visible_.
- _output_ (string)
    - Which output the notification was sorted onto. See the output style
	  option for possible values.
- _anchor_ (string)
	- Which position on the output the notification was assigned to. See the
      anchor style option for possible values.

There are only two passes performed on each notification, so the second-pass
criteria are not allowed to reposition the notification.

If a field's value contains special characters, they may be escaped with a
backslash, or quoted:

	\[app-name="Google Chrome"\]

	\[app-name=Google\\ Chrome\]

Quotes within quotes may also be escaped, and a literal backslash may be
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

There are some rules restricting what can be configured depending on what is
being matched by a given criteria:

- Criteria matching _grouped_ or _group-index_ are not allowed to change the
  _anchor_, _output_, or _group-by_, as this would invalidate the grouping.
  Grouping is only performed once rather than recursively, to avoid the
  potential for infinite loops.

# CRITERIA-ONLY STYLE OPTIONS

Some style options are not useful in the global context and therefore have no
associated command-line option.

*invisible*=0|1
	Whether this notification should be invisible even if it is above the
	_max-visible_ cutoff. This is used primarily for hiding members of groups.
	If you want to make more than the first group member visible, turn this
	option off within a _group-index_ criteria.

	Default: 0

# COLORS

Colors can be specified as _#RRGGBB_ or _#RRGGBBAA_.

# DIRECTIONAL VALUES

Some options set values that affect all four edges of a notification. These
options can be specified in several different ways, depending on how much
control over each edge is desired:

- A single value will apply to all four edges.
- Two values will set vertical and horizontal edges separately.
- Three will set top, horizontal, and bottom edges separately.
- Four will set top, right, bottom, and left edges separately.

When specifying multiple values, they should be comma-separated. For example,
this would set the top margin to 10, left and right to 20, and bottom to five:

```
margin = 10,20,5
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

*%i*	Notification id

## For the hidden notifications placeholder

*%h*	Number of hidden notifications

*%t*	Total number of notifications

# MODES

mako supports applying style options conditionally via modes. A configuration
section with a _mode_ criteria will only be applied if the current mode
matches. **makoctl**(1) can be used to change the current mode.

The initial list of modes contains a single mode called "default". This is
deprecated, in a future version the initial list of modes will be empty.

For example, to hide all notifications if the mode "do-not-disturb" is
enabled:

```
[mode=do-not-disturb]
invisible=1
```

_makoctl mode -a do-not-disturb_ will hide all notifications,
_makoctl mode -r do-not-disturb_ will show them again.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about mako development, see
https://github.com/emersion/mako.

# SEE ALSO

*mako*(1) *makoctl*(1)
