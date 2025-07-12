/* X-SPDX-License-Identifier: GPL-2.0+ */
/* Copyright Enrico Weigelt, metux IT consult <info@metux.net> */

#ifndef __XFWM4_COMMON_XFCONF_UTIL_H
#define __XFWM4_COMMON_XFCONF_UTIL_H

#include <gdk/gdk.h>
#include <xfconf/xfconf.h>

#include <common/core-c.h>

int xfwm_xfconf_get_geometry(XfconfChannel *channel, const gchar *path,
                             GdkRectangle *rect) NONNULL(1,2,3);

#endif /* __XFWM4_COMMON_XFCONF_UTIL_H */
