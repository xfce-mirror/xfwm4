[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/xfce/xfwm4/COPYING)

xfwm4
====================

xfwm is the window manager for Xfce 

----

### Homepage

[xfwm4 documentation](https://docs.xfce.org/xfce/xfwm4/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/xfce/xfwm4/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Source Code Repository

[xfwm4 source code](https://gitlab.xfce.org/xfce/xfwm4)

### Download A Release Tarball

[xfwm4 archive](https://archive.xfce.org/src/xfce/xfwm4)
    or
[xfwm4 tags](https://gitlab.xfce.org/xfce/xfwm4/-/tags)
### Installation

From source: 

    % cd xfwm4
    % meson setup build
    % meson compile -C build
    % meson install -C build

From release tarball:

    % tar xf xfwm4-<version>.tar.xz
    % cd xfwm4-<version>
    % meson setup build
    % meson compile -C build
    % meson install -C build

### Uninstallation

    % ninja uninstall -C build

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/xfce/xfwm4/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.

