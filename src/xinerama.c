/*  gxfce
 *  Copyright (C) 2002 Olivier Fourdan (fourdan@xfce.org)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
#  include <X11/extensions/Xinerama.h>
#endif

#include "xinerama.h"

#ifdef EMULATE_XINERAMA
/* NOTE : Look in xinerama.h to define EMULATE_XINERAMA
 * (do not do it here)
 * 
 * We simulate 2 screens side by side on a 1024x768 screen :
 * Left screen is 512x768 pixels wide
 * Right screen is 512x600 pixels wide with 84 unused pixels 
 *   on top and bottom of screen (this is to simulate the
 *   black holes as show below :
 *
 *  +-----------------+//////////////////
 *  |                 |///BLACK HOLE/////
 *  |                 |//////////////////
 *  |                 |-----------------+
 *  |                 |                 |
 *  |                 |                 |
 *  |      First      |      Second     |
 *  |     Screen      |      Screen     |
 *  |                 |                 |
 *  |                 |                 |
 *  |                 |-----------------+
 *  |                 |//////////////////
 *  |                 |///BLACK HOLE/////
 *  +-----------------+//////////////////
 *
 */

static XineramaScreenInfo emulate_infos[2] = {
    {0, 0, 0, 512, 768},
    {0, 512, 84, 512, 600}
};
static int emulate_heads = 2;
#endif

#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
static XineramaScreenInfo *xinerama_infos;
static int xinerama_heads;
static gboolean enable_xinerama;
#endif

#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H)
int xinerama_major, xinerama_minor;
#endif

gboolean xineramaInit(Display * dpy)
{
#ifdef EMULATE_XINERAMA
    xinerama_infos = emulate_infos;
    xinerama_heads = emulate_heads;
    enable_xinerama = TRUE;
    return enable_xinerama;
#else
#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
    if(!XineramaQueryExtension(dpy, &xinerama_major, &xinerama_minor))
    {
        g_message("Xinerama extension disabled");
        xinerama_heads = 0;
        enable_xinerama = FALSE;
        return enable_xinerama;
    }
    else
    {
        xinerama_infos = XineramaQueryScreens(dpy, &xinerama_heads);
        enable_xinerama = TRUE;
        return enable_xinerama;
    }
#else
    return FALSE;
#endif
#endif
}

void xineramaFree(void)
{
#ifndef EMULATE_XINERAMA
#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
    XFree(xinerama_infos);
#endif
#endif
}

inline int xineramaGetHeads(void)
{
#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
    return xinerama_heads;
#else
    return 0;
#endif
}

/* Xinerama handling routines */

#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
static int findhead(int x, int y)
{
    extern XineramaScreenInfo *xinerama_infos;
    extern int xinerama_heads;
    extern gboolean enable_xinerama;
    static int cache_x;
    static int cache_y;
    static int cache_head;
    static gboolean cache_init = FALSE;

    int head, closest_head;
    int dx, dy;
    int center_x, center_y;
    int distsquare, min_distsquare;

    /* Cache system */
    if((cache_init) && (x == cache_x) && (y == cache_y))
        return (cache_head);

    /* Okay, now we consider the cache has been initialized */
    cache_init = TRUE;
    cache_x = x;
    cache_y = y;

    if((xinerama_heads == 0) || (xinerama_infos == NULL) || (!enable_xinerama))
        /* Xinerama extensions are disabled */
    {
        cache_head = 0;
        return (0);
    }

    for(head = 0; head < xinerama_heads; head++)
    {
        if((xinerama_infos[head].x_org <= x) && ((xinerama_infos[head].x_org + xinerama_infos[head].width) > x) && (xinerama_infos[head].y_org <= y) && ((xinerama_infos[head].y_org + xinerama_infos[head].height) > y))
        {
            cache_head = head;
            return (head);
        }
    }
    /* No head has been eligible, use the closest one */

    center_x = xinerama_infos[0].x_org + (xinerama_infos[0].width / 2);
    center_y = xinerama_infos[0].y_org + (xinerama_infos[0].height / 2);

    dx = x - center_x;
    dy = y - center_y;

    min_distsquare = (dx * dx) + (dy * dy);
    closest_head = 0;

    for(head = 1; head < xinerama_heads; head++)
    {
        center_x = xinerama_infos[head].x_org + (xinerama_infos[head].width / 2);
        center_y = xinerama_infos[head].y_org + (xinerama_infos[head].height / 2);

        dx = x - center_x;
        dy = y - center_y;

        distsquare = (dx * dx) + (dy * dy);

        if(distsquare < min_distsquare)
        {
            min_distsquare = distsquare;
            closest_head = head;
        }
    }
    cache_head = closest_head;
    return (closest_head);
}
#endif

inline int MyDisplayHeight(Display * dpy, int screen, int x, int y)
{
#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
    int head;

    if((xinerama_heads == 0) || (xinerama_infos == NULL) || (!enable_xinerama))
        /* Xinerama extensions are disabled */
        return (XDisplayHeight(dpy, screen));

    head = findhead(x, y);
    return (xinerama_infos[head].height);
#else
    return (XDisplayHeight(dpy, screen));
#endif
}

inline int MyDisplayWidth(Display * dpy, int screen, int x, int y)
{
#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
    int head;

    if((xinerama_heads == 0) || (xinerama_infos == NULL) || (!enable_xinerama))
        /* Xinerama extensions are disabled */
        return (XDisplayWidth(dpy, screen));

    head = findhead(x, y);
    return (xinerama_infos[head].width);
#else
    return (XDisplayWidth(dpy, screen));
#endif
}

inline int MyDisplayX(int x, int y)
{
#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
    int head;

    if((xinerama_heads == 0) || (xinerama_infos == NULL) || (!enable_xinerama))
        /* Xinerama extensions are disabled */
        return (0);

    head = findhead(x, y);
    return (xinerama_infos[head].x_org);
#else
    return (0);
#endif
}


inline int MyDisplayY(int x, int y)
{
#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
    int head;

    if((xinerama_heads == 0) || (xinerama_infos == NULL) || (!enable_xinerama))
        /* Xinerama extensions are disabled */
        return (0);

    head = findhead(x, y);
    return (xinerama_infos[head].y_org);
#else
    return (0);
#endif
}

inline int MyDisplayMaxX(Display * dpy, int screen, int x, int y)
{
#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
    int head;

    if((xinerama_heads == 0) || (xinerama_infos == NULL) || (!enable_xinerama))
        /* Xinerama extensions are disabled */
        return (XDisplayWidth(dpy, screen));

    head = findhead(x, y);
    return (xinerama_infos[head].x_org + xinerama_infos[head].width);
#else
    return (XDisplayWidth(dpy, screen));
#endif
}

inline int MyDisplayMaxY(Display * dpy, int screen, int x, int y)
{
#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
    int head;

    if((xinerama_heads == 0) || (xinerama_infos == NULL) || (!enable_xinerama))
        /* Xinerama extensions are disabled */
        return (XDisplayHeight(dpy, screen));

    head = findhead(x, y);
    return (xinerama_infos[head].y_org + xinerama_infos[head].height);
#else
    return (XDisplayHeight(dpy, screen));
#endif
}

inline gboolean isRightMostHead(Display * dpy, int screen, int x, int y)
{
#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
    return (MyDisplayMaxX(dpy, screen, x, y) == XDisplayWidth(dpy, screen));
#else
    return (TRUE);
#endif
}

inline gboolean isLeftMostHead(Display * dpy, int screen, int x, int y)
{
#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
    return (MyDisplayX(x, y) == 0);
#else
    return (TRUE);
#endif
}

inline gboolean isTopMostHead(Display * dpy, int screen, int x, int y)
{
#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
    return (MyDisplayY(x, y) == 0);
#else
    return (TRUE);
#endif
}

inline gboolean isBottomMostHead(Display * dpy, int screen, int x, int y)
{
#if defined(HAVE_X11_EXTENSIONS_XINERAMA_H) || defined(EMULATE_XINERAMA)
    return (MyDisplayMaxY(dpy, screen, x, y) == XDisplayHeight(dpy, screen));
#else
    return (TRUE);
#endif
}
