
#include <stdio.h>
#include <glib.h>
#include <xfconf/xfconf.h>

#include "common/xfconf-util.h"
#include "policy.h"
#include "client.h"

struct winspec_iter {
    const char *res_class;
    const char *res_name;
    const char *wm_name;
    const char *window_type;
    const char *prefix;
    const char *suffix;
    int pos;
    char current[PATH_MAX];
};

#define SAFE_STR(s) (s ? s : "")

#define ITER_MASK_NUM 16

static const struct _iter_mask {
    char res_class;
    char res_name;
    char wm_name;
    char window_type;
} iter_masks[ITER_MASK_NUM] = {
    { 1, 1, 1, 1 },
    { 1, 1, 1, 0 },
    { 1, 1, 0, 1 },
    { 1, 1, 0, 0 },
    { 1, 0, 1, 1 },
    { 1, 0, 1, 0 },
    { 1, 0, 0, 1 },
    { 1, 0, 0, 0 },
    { 0, 1, 1, 1 },
    { 0, 1, 1, 0 },
    { 0, 1, 0, 1 },
    { 0, 1, 0, 0 },
    { 0, 0, 1, 1 },
    { 0, 0, 1, 0 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 0 },
};


static void winspec_iter_init(struct winspec_iter *iter,
                              const gchar *suffix, const gchar *res_class,
                              const gchar *res_name, const gchar *wm_name,
                              const gchar *window_type)
{
    iter->res_class = SAFE_STR(res_class);
    iter->res_name = SAFE_STR(res_name);
    iter->wm_name = SAFE_STR(wm_name);
    iter->window_type = SAFE_STR(window_type);
    iter->suffix = suffix;
    iter->prefix = "policies";
    iter->pos = 0;
    iter->current[0] = 0;
}

static int winspec_iter_next(struct winspec_iter *iter)
{
    struct _iter_mask mask;

    if (iter->pos >= ITER_MASK_NUM)
        return 0;

    mask = iter_masks[iter->pos];

    snprintf (iter->current, sizeof(iter->current), "/%s%s%s.%s.%s.%s%s%s",
             (iter->prefix     ? iter->prefix : ""),
             (iter->prefix     ? "/" : ""),
             (mask.res_name    ? iter->res_name : "*"),
             (mask.res_class   ? iter->res_class : "*"),
             (mask.wm_name     ? iter->wm_name : "*"),
             (mask.window_type ? iter->window_type : "*"),
             (iter->suffix     ? "/" : ""),
             (iter->suffix     ? iter->suffix : ""));

    iter->pos++;

    return 1;
}

gchar *policy_get_string (XfconfChannel *channel,
                          const gchar *property,
                          const gchar *res_name,
                          const gchar *res_class,
                          const gchar *wm_name,
                          const gchar *window_type)
{
    struct winspec_iter iter;

    winspec_iter_init (&iter, property, res_class, res_name,
                       wm_name, window_type);
    while (winspec_iter_next (&iter))
    {
        gchar *r = xfconf_channel_get_string (channel, iter.current, NULL);
        if (r)
            return r;
    }

    return NULL;
}

gboolean policy_get_bool (XfconfChannel *channel,
                          const gchar *property,
                          const gchar *res_name,
                          const gchar *res_class,
                          const gchar *wm_name,
                          const gchar *window_type,
                          gboolean def)
{
    struct winspec_iter iter;

    winspec_iter_init (&iter, property, res_class, res_name,
                       wm_name, window_type);
    while (winspec_iter_next (&iter))
    {
        GValue value = { 0 };
        if (xfconf_channel_get_property (channel, iter.current, &value))
        {
            if (G_VALUE_HOLDS_BOOLEAN(&value))
            {
                def = g_value_get_boolean(&value);
                return def;
            }
        }
    }

    return def;
}

int policy_get_geometry (XfconfChannel *channel,
                         const gchar *property,
                         const gchar *res_name,
                         const gchar *res_class,
                         const gchar *wm_name,
                         const gchar *window_type,
                         GdkRectangle *geometry)
{
    struct winspec_iter iter;

    winspec_iter_init (&iter, property, res_class, res_name,
                       wm_name, window_type);
    while (winspec_iter_next (&iter))
    {
        int ret = xfwm_xfconf_get_geometry(channel, iter.current, geometry);
        if (ret)
            return ret;
    }

    return 0;
}

/* ClientInfo based helpers */

gchar *clientPolicyGetString (ClientInfo *client, const gchar *property)
{
    return policy_get_string (client->screen_info->xfwm4_channel,
                              property,
                              client->class.res_class,
                              client->class.res_name,
                              client->name,
                              client->type_name);
}

int clientPolicyGetGeometry (ClientInfo *client, const gchar *property,
                             GdkRectangle *ret_geometry)
{
    return policy_get_geometry (client->screen_info->xfwm4_channel,
                                property,
                                client->class.res_name,
                                client->class.res_class,
                                client->name,
                                client->type_name,
                                ret_geometry);
}

gboolean clientPolicyGetBool (ClientInfo *client, const gchar *property,
                              gboolean def)
{
    return policy_get_bool (client->screen_info->xfwm4_channel,
                            property,
                            client->class.res_name,
                            client->class.res_class,
                            client->name,
                            client->type_name,
                            def);
}
