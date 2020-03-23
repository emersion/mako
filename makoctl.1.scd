makoctl(1)

# NAME

makoctl - controls the *mako*(1) daemon

# SYNOPSIS

makoctl [cmd] [options...]

# DESCRIPTION

Sends IPC commands to the running mako daemon via dbus.

# COMMANDS

*dismiss*
	Dismisses the first notification.

	Options:

	*-a, --all*
		Dismiss all notifications.

	*-g, --group*
		Dismiss the first notification group.

*restore*
	Restores the most recently expired notification from the history buffer.

*invoke* [action]
	Invokes an action on the first notification. If _action_ is not specified,
	invokes the default action.

*menu* [program] [argument...]
	Use a program to select an action on the first notification. The list of
	actions are joined on newlines and passed to _program_. The program should
	write the selected action to stdout. If an action is given, this action
	will be invoked.

	If no action is found, or no action is selected, _makoctl_ will return non-zero.

	Examples:

		```
		makoctl menu dmenu -p 'Select Action: '
		makoctl menu wofi -d -p 'Choose Action: '
		```

*list*
	Retrieve a list of current notifications.

*reload*
	Reloads the configuration file.

*set* <key>=<value>
	Set a global configuration option.

	Reloading the config will discard options set like this.

*help, -h, --help*
	Show help message and quit.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about mako development, see
https://github.com/emersion/mako.

# SEE ALSO

*mako*(1)
