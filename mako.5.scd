mako(5)

# NAME

mako - configuration file

# DESCRIPTION

The mako configuration file is located at *~/.config/mako/config* or at
*$XDG\_CONFIG\_HOME/mako/config*. Each line of the form:

	key=value

Is equivalent to passing *--key=value* to mako from the command line. See
*mako*(1) for a list of command line options. Note that any quotes used within
your shell are unnecessary and also invalid in the config file.

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

The following fields are available in criteria:

- _app-name_ (string)
- _app-icon_ (string)
- _summary_ (string)
	- An exact match on the summary of the notification.
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

# CRITERIA-ONLY STYLE OPTIONS

Some style options are not useful in the global context and therefore have no
associated command-line option.

*invisible* 0|1
	Whether this notification should be invisible even if it is above the
	_max-visible_ cutoff. This is used primarily for hiding members of groups.
	If you want to make more than the first group member visible, turn this
	option off within a _group-index_ criteria.

	Default: 0


# COLORS

Colors can be specified as _#RRGGBB_ or _#RRGGBBAA_.

# FORMAT SPECIFIERS

Format specification works similarly to *printf*(3), but with a different set of
specifiers.

See *mako*(1) for more information.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about mako development, see
https://github.com/emersion/mako.

# SEE ALSO

*mako*(1) *makoctl*(1)
