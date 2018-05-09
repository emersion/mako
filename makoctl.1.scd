makoctl(1)

# NAME

makoctl - controls the *mako*(1) daemon

# DESCRIPTION

makoctl [cmd] [options...]

# SYNOPSIS

Sends IPC commands to the running mako daemon via dbus.

# COMMANDS

*close*
	Dismisses the oldest notification.

	*Options*

	*-a, --all*
		Dismiss all notifications.

*help, -h, --help*
	Show help message and quit.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about mako development, see
https://github.com/emersion/mako.

# SEE ALSO

*mako*(1)
