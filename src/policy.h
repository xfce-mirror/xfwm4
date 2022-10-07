#ifndef __XFWM4_POLICY_H
#define __XFWM4_POLICY_H

#include <glib.h>
#include <gdk/gdk.h>
#include <xfconf/xfconf.h>

#include <common/core-c.h>

gchar * policy_get_string(XfconfChannel *channel,
                          const gchar *property,
                          const gchar *res_class,
                          const gchar *res_name,
                          const gchar *wm_name,
                          const gchar *window_type) NONNULL(1,2);

int policy_get_geometry(XfconfChannel *channel,
                        const gchar *property,
                        const gchar *res_class,
                        const gchar *res_name,
                        const gchar *wm_name,
                        const gchar *window_type,
                        GdkRectangle *geometry) NONNULL(1,2,7);

#endif /* __XFWM4_POLICY_H */
