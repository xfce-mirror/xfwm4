/* X-SPDX-License-Identifier: GPL-2.0+ */

#ifndef __XFWM4_FENCES_H
#define __XFWM4_FENCES_H

#include <gdk/gdk.h>
#include <xfconf/xfconf.h>

#include <common/list.h>

typedef struct _ScreenInfo ScreenInfo;
typedef struct _FenceInfo FenceInfo;
typedef struct _WireFrame WireFrame;

struct _FenceInfo {
    struct list_head list;

    GdkRectangle geometry;
    gchar *ident;
    gchar *title;
    GdkRGBA border_color;
    GdkRGBA background_color;
    WireFrame *wireframe;
    gboolean auto_maximize;
};

#ifdef ENABLE_WINDOW_FENCES

void       fencesFree(struct list_head *fences) NONNULL(1);
void       fencesLoad(struct list_head *fences, XfconfChannel *channel) NONNULL(1,2);
FenceInfo* fencesLookup(struct list_head *fences, const gchar *ident) NONNULL(1);

FenceInfo* fencesFindBest(struct list_head *fences, GdkRectangle geometry) NONNULL(1);

void       fenceShow(FenceInfo *fence, ScreenInfo *screen_info) NONNULL(2);
void       fenceHide(FenceInfo *fence);

#else /* ENABLE_WINDOW_FENCES */

#define fencesFree(a)
#define fencesLoad(a,b)
#define fencesLookup(a,b) (NULL)
#define fencesFindBest(a,b) (NULL)
#define fenceShow(a,b)
#define fenceHide(a)

#endif /* ENABLE_WINDOW_FENCES */

#endif /* __XFWM4_FENCES_H */
