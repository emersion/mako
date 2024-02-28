mako(1)

# NAME

mako - notification daemon for Wayland

# SYNOPSIS

*mako* [options...]

# DESCRIPTION

mako is a graphical notification daemon for Wayland compositors which support
the layer-shell protocol. Notifications received over D-Bus are displayed until
dismissed with a click or via *makoctl*(1).

# OPTIONS

*-h, --help*
	Show help message and quit.

*-c, --config*
	Custom path to the config file.

Additionally, global configuration options can be specified. Passing
*--key=value* is equivalent to a *key=value* line in the configuration file.
See *mako*(5) for a list of options.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other
open-source contributors. For more information about mako development, see
https://github.com/emersion/mako.

# SEE ALSO

*mako*(5) *makoctl*(1)
