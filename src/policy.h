#ifndef __XFWM4_POLICY_H
#define __XFWM4_POLICY_H

#include <glib.h>
#include <gdk/gdk.h>
#include <xfconf/xfconf.h>

#include <common/core-c.h>

typedef struct _Client ClientInfo;

#ifdef ENABLE_WINDOW_POLICIES

/* ClientInfo based policy getters */
gchar *clientPolicyGetString(ClientInfo *client,
                             const gchar *property) NONNULL(1,2);

gboolean clientPolicyGetBool(ClientInfo *client,
                             const gchar *property,
                             gboolean def) NONNULL(1,2);

int clientPolicyGetGeometry(ClientInfo *client,
                            const gchar *property,
                            GdkRectangle *ret_geometry) NONNULL(1,2,3);

#else /* ENABLE_WINDOW_POLICIES */

#define clientPolicyGetString(a,b)            (NULL)
#define clientPolicyGetBool(a,b,c)            (c)
#define clientPolicyGetGeometry(a,b,c)        (0)

#endif /* ENABLE_WINDOW_POLICIES */

#endif /* __XFWM4_POLICY_H */
