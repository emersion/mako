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

	*Options*

	*-a, --all*
		Dismiss all notifications.

*invoke* [action]
	Invokes an action on the first notification. If _action_ is not specified,
	invokes the default action.

*reload*
	Reloads the configuration file.

*help, -h, --help*
	Show help message and quit.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about mako development, see
https://github.com/emersion/mako.

# SEE ALSO

*mako*(1)
