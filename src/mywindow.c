/*      $Id$
 
        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2, or (at your option)
        any later version.
 
        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.
 
        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <libxfce4util/libxfce4util.h> 
#include "mypixmap.h"
#include "mywindow.h"

#ifdef HAVE_RENDER
#include <X11/extensions/Xrender.h>
#endif

static void
xfwmWindowSetVisual (xfwmWindow * win, Visual *visual, gint depth)
{
    if (visual)
    {
        win->visual = visual;
    }
    else
    {
        win->visual = DefaultVisual (win->dpy, win->screen);
    }
    
    if (depth)
    {
        win->depth = depth;
    }
    else
    {
        win->depth = DefaultDepth (win->dpy, win->screen);
    }
}

void
xfwmWindowInit (xfwmWindow * win)
{
    win->window = None;
    win->map = FALSE;
    win->dpy = NULL;
    win->depth = 0;
    win->x = 0;
    win->y = 0;
    win->width = 1;
    win->height = 1;
}

void
xfwmWindowCreate (Display * dpy, gint screen, Visual *visual, gint depth, Window parent,  
                  xfwmWindow * win, Cursor cursor)
{
    TRACE ("entering xfwmWindowCreate");

    win->window = XCreateSimpleWindow (dpy, parent, 0, 0, 1, 1, 0, 0, 0);
    TRACE ("Created XID 0x%lx", win->window);
    if (cursor != None)
    {
        XDefineCursor (dpy, win->window, cursor);
    }
    win->map = FALSE;
    win->dpy = dpy;
    win->depth = depth;
    win->screen = screen;
    win->x = 0;
    win->y = 0;
    win->width = 1;
    win->height = 1;
    xfwmWindowSetVisual (win, visual, depth);
}

void
xfwmWindowDelete (xfwmWindow * win)
{
    TRACE ("entering xfwmWindowDelete");

    if (win->window != None)
    {
        XDestroyWindow (win->dpy, win->window);
        win->window = None;
    }
    win->map = FALSE;
}

void
xfwmWindowShow (xfwmWindow * win, int x, int y, int width, int height,
    gboolean refresh)
{
    TRACE ("entering xfwmWindowShow");

    if (!(win->window))
    {
        return;
    }
    if ((width < 1) || (height < 1))
    {
        xfwmWindowHide (win);
        return;
    }
    if (!(win->map))
    {
        XMapWindow (win->dpy, win->window);
        win->map = TRUE;
    }
    TRACE ("Showing XID 0x%lx", win->window);
    if (((x != win->x) || (y != win->y)) && ((width != win->width)
            || (height != win->height)))
    {
        XMoveResizeWindow (win->dpy, win->window, x, y, (unsigned int) width,
            (unsigned int) height);
        win->x = x;
        win->y = y;
        win->width = width;
        win->height = height;
    }
    else if ((x != win->x) || (y != win->y))
    {
        XMoveWindow (win->dpy, win->window, x, y);
        if (refresh)
        {
            XClearWindow (win->dpy, win->window);
        }
        win->x = x;
        win->y = y;
    }
    else if ((width != win->width) || (height != win->height))
    {
        XResizeWindow (win->dpy, win->window, (unsigned int) width,
            (unsigned int) height);
        win->width = width;
        win->height = height;
    }
    else if (refresh)
    {
        XClearWindow (win->dpy, win->window);
    }
}

void
xfwmWindowHide (xfwmWindow * win)
{
    TRACE ("entering xfwmWindowHide");

    if (win->map)
    {
        g_assert (win->window);
        XUnmapWindow (win->dpy, win->window);
        win->map = FALSE;
    }
}

gboolean
xfwmWindowVisible (xfwmWindow *win)
{
    g_return_val_if_fail (win, FALSE);
    
    return win->map;
}        

gboolean
xfwmWindowDeleted (xfwmWindow *win)
{
    g_return_val_if_fail (win, TRUE);

    return (win->window == None);
}        

void 
xfwmWindowTemp (Display * dpy, gint screen, Visual *visual, gint depth, Window parent,
                xfwmWindow * win, int x, int y, 
                int width, int height, long eventmask)
{
    XSetWindowAttributes attributes;

    attributes.event_mask = eventmask;
    attributes.override_redirect = TRUE;
    win->window = XCreateWindow (dpy, parent, x, y, width, height, 0, 0, 
                         InputOnly, CopyFromParent,
                         CWEventMask | CWOverrideRedirect, &attributes);
    XMapRaised (dpy, win->window);
    XFlush (dpy);

    win->map = TRUE;
    win->dpy = dpy;
    win->screen = screen;
    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    xfwmWindowSetVisual (win, visual, depth);
}

void
xfwmWindowSetBG (xfwmWindow * win, xfwmPixmap * pix)
{
    if ((win->width < 1) || (win->height < 1) || (pix->width < 1) || (pix->height < 1))
    {
        return;
    }

    if ((win->visual == DefaultVisual (win->dpy, win->screen))
        && (win->depth == DefaultDepth (win->dpy, win->screen)))
    {
        XSetWindowBackgroundPixmap (win->dpy, win->window, pix->pixmap);
    }
#ifdef HAVE_RENDER
    else
    {
        Picture psrc, pdst;
        XRenderPictFormat *format_src, *format_dst;
        Pixmap temp;

        format_src = XRenderFindVisualFormat (win->dpy, DefaultVisual (win->dpy, win->screen));
        format_dst = XRenderFindVisualFormat (win->dpy, win->visual);

        if (!format_src || !format_dst)
        {
            g_warning ("Cannot get XRender picture format");
            return;
        }

        temp = XCreatePixmap (win->dpy, win->window, pix->width, pix->height, win->depth);

        psrc = XRenderCreatePicture (win->dpy, pix->pixmap, format_src, 0, NULL);
        pdst = XRenderCreatePicture (win->dpy, temp, format_dst, 0, NULL);

        XRenderComposite (win->dpy, PictOpSrc, psrc, None, pdst, 0, 0, 0, 0, 0, 0, pix->width, pix->height);

        XRenderFreePicture (win->dpy, psrc);
        XRenderFreePicture (win->dpy, pdst);

        XSetWindowBackgroundPixmap (win->dpy, win->window, temp);

        XFreePixmap (win->dpy, temp);
    }
#endif
}
