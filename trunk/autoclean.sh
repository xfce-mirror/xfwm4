#!/bin/sh
(type xdt-autogen) >/dev/null 2>&1 || {
  cat >&2 <<EOF
autoclean.sh: You don't seem to have the Xfce development tools installed on
              your system, which are required to build this software.
              Please install the xfce4-dev-tools package first, it is 
	      available from http://www.xfce.org/.
EOF
  exit 1
}

exec xdt-autogen clean
