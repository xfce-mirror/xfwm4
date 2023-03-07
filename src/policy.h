#ifndef __XFWM4_POLICY_H
#define __XFWM4_POLICY_H

#include <glib.h>
#include <gdk/gdk.h>
#include <xfconf/xfconf.h>

#include <common/core-c.h>

typedef struct _Client ClientInfo;

#ifdef ENABLE_WINDOW_POLICIES

/* generic policy getters */

gchar *policy_get_string(XfconfChannel *channel,
                          const gchar *property,
                          const gchar *res_name,
                          const gchar *res_class,
                          const gchar *wm_name,
                          const gchar *window_type) NONNULL(1,2);

gboolean policy_get_bool(XfconfChannel *channel,
                         const gchar *property,
                         const gchar *res_name,
                         const gchar *res_class,
                         const gchar *wm_name,
                         const gchar *window_type,
                         gboolean def) NONNULL(1,2);

int policy_get_geometry(XfconfChannel *channel,
                        const gchar *property,
                        const gchar *res_name,
                        const gchar *res_class,
                        const gchar *wm_name,
                        const gchar *window_type,
                        GdkRectangle *geometry) NONNULL(1,2,7);

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

#define policy_get_string(a,b,c,d,e,f)        (NULL)
#define policy_get_bool(a,b,c,d,e,f,g)        (FALSE)
#define policy_get_geometry(a,b,c,d,e,f,g)    (0)

#define clientPolicyGetString(a,b)            (NULL)
#define clientPolicyGetBool(a,b,c)            (c)
#define clientPolicyGetGeometry(a,b,c)        (0)

#endif /* ENABLE_WINDOW_POLICIES */

#endif /* __XFWM4_POLICY_H */
