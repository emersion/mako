makoctl(1)

# NAME

makoctl - controls the *mako*(1) daemon

# SYNOPSIS

makoctl [cmd] [options...]

# DESCRIPTION

Sends IPC commands to the running mako daemon via dbus.

# COMMANDS

*dismiss* [-a|--all] [-g|--group] [-n <id>]
	Dismisses a notification.

	Options:

	*-a, --all*
		Dismiss all notifications.

	*-g, --group*
		Dismiss the first notification group.

	*-n* <id>
		Dismiss the notification with the given id. Defaults to the first
		notification.

*restore*
	Restores the most recently expired notification from the history buffer.

*invoke* [-n <id>] [action]
	Invokes an action on a notification. If _action_ is not specified, invokes
	the default action. Action names can be discovered using `makoctl list`.

	Options:

	*-n* <id>
		Invoke the action on the notification with the given id. Defaults to
		the first notification.

*menu* [-n <id>] <program> [argument...]
	Use a program to select an action on a notification. The list of
	actions are joined on newlines and passed to _program_. The program should
	write the selected action to stdout. If an action is given, this action
	will be invoked.

	If no action is found, or no action is selected, _makoctl_ will return
	non-zero.

	Options:

	*-n* <id>
		List the actions of the notification with the given id. Defaults to
		the first notification.

	Examples:

		```
		makoctl menu dmenu -p 'Select Action: '
		makoctl menu -n 12345 wofi -d -p 'Choose Action: '
		```

*list*
	Retrieve a list of current notifications.

*reload*
	Reloads the configuration file.

*mode*
*mode* -s <mode>...
*mode* [-a mode]... [-r mode]...
	When run without any option, retrieves a list of current modes.

	When run with the _-s_ option, replaces the current modes with the provided
	list.

	When run with the _-a_ or _-r_ options, adds or removes the provided mode
	from the current modes.

	See the _MODES_ section in **mako**(5) for more information about modes.

*help, -h, --help*
	Show help message and quit.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about mako development, see
https://github.com/emersion/mako.

# SEE ALSO

*mako*(1)
