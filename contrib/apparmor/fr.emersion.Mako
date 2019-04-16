#include <tunables/global>

profile fr.emersion.Mako /usr/bin/mako {
  #include <abstractions/base>
  #include <abstractions/fonts>
  #include <abstractions/wayland>

  #include <abstractions/dbus-strict>
  dbus bind
       bus=session
       name=org.freedesktop.Notifications,
  dbus receive
       bus=session
       path=/fr/emersion/Mako
       interface=fr.emersion.Mako,

  /{run,dev}/shm/mako-* rw,
  owner @{HOME}/.config/mako/config r,

  # Site-specific additions and overrides. See local/README for details.
  #include if exists <local/fr.emersion.Mako>
}
