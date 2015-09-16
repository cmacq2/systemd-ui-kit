
# systemd-ui-kit

## What systemd-ui-kit is

 * A library to expose selected systemd and related API in a nice way for KDE applications.
 * A grab bag of components that any KDE frontend to systemd would have to reinvent anyway.
 * A means to separate UI cleanly from functionality that any such frontend would offer.

## What systemd-ui-kit is not

`systemd-ui-kit` is not:
 * A KDE/Qt systemd bindings project, aiming for full systemd and related API coverage.
 * A generic Qt only library. Integration with KDE desktop is not considered optional (at this point).
 
## Requirements

See systemd-kcm project.
 
## Basic todo list

 * Settle on an appropriate licence for the project: LGPL v2+ or GPL v2+? 
  - also what about borrowed code/cmake magic from other KDE projects?
 * Investigate behaviour of Qt with broken/invalid UTF-8 encoded files. (We'd like to recover gracefully instead of erroring out.)
 * Unit parser that actually tokenises things
 * Unit file model that permits annotations to items (like errors, suggestions, help links)
 * Type support for various configuration options (e.g. `si_size`, `iec_size`, enums/bitmasks)
 * Support for relating various unit types to each other (e.g. socket <-> service <-> bus)
 * Investigate possibility of a Qt item model with custom widgets depending on type, changing from row to row.
 * Investigate remote support, to talk to systemd systems over SSH:
  - proxying dbus, can Qt dbus API be configured with raw sockets?
  - side channel to execute commands?
  - see: systemd-run command, which has a --host switch to do basically this.
 * Investigate support for machined containers
  - again: obtaining dbus connection, side channel?
  - see: various -M switches supported by systemd utilities