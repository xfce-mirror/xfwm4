/* X-SPDX-License-Identifier: GPL-2.0+ */

#include <gdk/gdk.h>

#include <common/list.h>
#include <common/xfconf-util.h>

#include "screen.h"
#include "fences.h"
#include "wireframe.h"

#define XFCONF_KEY_FENCES "/fences"

#define XFCONF_FENCE_PATH(path, ident, key) \
    snprintf(path, sizeof(path), XFCONF_KEY_FENCES "/%s/" key, ident)

static void loadFenceDef(struct list_head *head, XfconfChannel *channel, const char* ident)
{
    FenceInfo *fence;
    char p[PATH_MAX];

    fence = g_new0(FenceInfo, 1);
    fence->ident = g_strdup(ident);

    XFCONF_FENCE_PATH(p, ident, "auto-maximize");
    fence->auto_maximize = xfconf_channel_get_bool(channel, p, FALSE);

    XFCONF_FENCE_PATH(p, ident, "geometry");
    xfwm_xfconf_get_geometry(channel, p, &fence->geometry);

    XFCONF_FENCE_PATH(p, ident, "title");
    fence->title = xfconf_channel_get_string(channel, p, "");

    XFCONF_FENCE_PATH(p, ident, "border-color");
    xfwm_xfconf_get_rgba (channel, p, &fence->border_color);

    XFCONF_FENCE_PATH(p, ident, "background-color");
    xfwm_xfconf_get_rgba (channel, p, &fence->background_color);

    list_add(&fence->list, head);
}

/*
 * load fences definition from xfconf
 *
 * note that the /fences/ entries MUST be of type array,
 * otherwise they wont be found by iterating over all properties.
 *
 * xfconf_channel_get_properties() is quite inrobust against
 * tiny errors (eg. missing type attribute)
 */
void fencesLoad(struct list_head *head, XfconfChannel *channel)
{
    GHashTable *ht = NULL;

    if ((ht = xfconf_channel_get_properties(channel, XFCONF_KEY_FENCES))) {
        GHashTableIter iter;
        gpointer value;
        char *key;

        g_hash_table_iter_init(&iter, ht);
        while (g_hash_table_iter_next(&iter, (gpointer)&key, &value))
        {
            if (strlen(key) > strlen(XFCONF_KEY_FENCES)+1) {
                key += strlen(XFCONF_KEY_FENCES)+1;
                if (!strchr(key, '/'))
                    loadFenceDef(head, channel, key);
            }
        }
        g_hash_table_unref(ht);
    }
}

void fencesFree(struct list_head *head)
{
    FenceInfo *walk, *save;
    list_for_each_entry_safe(walk, save, head, list)
    {
        list_del(&walk->list);
        if (walk->ident)
            g_free(walk->ident);
        if (walk->title)
            g_free(walk->title);
        g_free(walk);
    }
}

void fenceShow(FenceInfo *fence, ScreenInfo *screen_info)
{
    if (!fence)
        return;

    if (!fence->wireframe)
        fence->wireframe = wireframeCreate(screen_info, fence->geometry,
                                           fence->border_color, fence->background_color);

    wireframeRedraw(fence->wireframe);
}

void fenceHide(FenceInfo *fence)
{
    if (!fence)
        return;

    if (!fence->wireframe)
        return;

    wireframeDelete(fence->wireframe);
    fence->wireframe = NULL;
}

FenceInfo *fencesFindBest(struct list_head *fences, GdkRectangle geometry)
{
    FenceInfo *walk, *found = NULL;
    int max_sz = 0;

    list_for_each_entry(walk, fences, list)
    {
        int sz;
        GdkRectangle r;
        gdk_rectangle_intersect(&walk->geometry, &geometry, &r);
        sz = r.width * r.height;
        if (sz > max_sz)
        {
            max_sz = sz;
            found = walk;
        }
    }

    return found;
}

FenceInfo* fencesLookup(struct list_head *fences, const gchar *ident)
{
    FenceInfo *fence;

    list_for_each_entry(fence, fences, list)
    {
        if (fence->ident && (strcmp(ident, fence->ident)==0))
            return fence;
    }

    return NULL;
}
