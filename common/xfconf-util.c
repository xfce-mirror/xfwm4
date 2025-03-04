/* X-SPDX-License-Identifier: GPL-2.0+ */
/* Copyright Enrico Weigelt, metux IT consult <info@metux.net> */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk/gdk.h>
#include <xfconf/xfconf.h>

#include <common/xfconf-util.h>

int xfwm_xfconf_get_geometry(XfconfChannel *c, const gchar *p, GdkRectangle *r)
{
    int ret = 0, x=0, y=0;
    unsigned int w=0, h=0;
    char* t = xfconf_channel_get_string (c, p, "");

    if (t[0])
        ret = XParseGeometry(t, &x, &y, &w, &h);

    *r = (GdkRectangle) { .x = x, .y = y, .width = w, .height = h };

    g_free(t);
    return ret;
}
