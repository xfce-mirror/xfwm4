/*
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
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <pango/pango.h>
#include <libxfce4util/libxfce4util.h> 

#include "screen.h"
#include "client.h"
#include "settings.h"
#include "mywindow.h"
#include "focus.h"
#include "frame.h"

int
frameLeft (Client * c)
{
    TRACE ("entering frameLeft");

    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER,
            CLIENT_FLAG_FULLSCREEN))
    {
        return c->screen_info->sides[SIDE_LEFT][ACTIVE].width;
    }
    return 0;
}

int
frameRight (Client * c)
{
    TRACE ("entering frameRight");

    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER,
            CLIENT_FLAG_FULLSCREEN))
    {
        return c->screen_info->sides[SIDE_RIGHT][ACTIVE].width;
    }
    return 0;
}

int
frameTop (Client * c)
{
    TRACE ("entering frameTop");

    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER,
            CLIENT_FLAG_FULLSCREEN))
    {
        return c->screen_info->title[TITLE_3][ACTIVE].height;
    }
    return 0;
}

int
frameBottom (Client * c)
{
    TRACE ("entering frameBottom");

    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER,
            CLIENT_FLAG_FULLSCREEN))
    {
        return c->screen_info->sides[SIDE_BOTTOM][ACTIVE].height;
    }
    return 0;
}

int
frameX (Client * c)
{
    TRACE ("entering frameX");

    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER,
            CLIENT_FLAG_FULLSCREEN))
    {
        return c->x - frameLeft (c);
    }
    return c->x;
}

int
frameY (Client * c)
{
    TRACE ("entering frameY");

    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER,
            CLIENT_FLAG_FULLSCREEN))
    {
        return c->y - frameTop (c);
    }
    return c->y;
}

int
frameWidth (Client * c)
{
    TRACE ("entering frameWidth");

    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER,
            CLIENT_FLAG_FULLSCREEN))
    {
        return c->width + frameLeft (c) + frameRight (c);
    }
    return c->width;
}

int
frameHeight (Client * c)
{
    TRACE ("entering frameHeight");

    if (FLAG_TEST_AND_NOT (c->flags,
            CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_SHADED,
            CLIENT_FLAG_FULLSCREEN))
    {
        return frameTop (c) + frameBottom (c);
    }
    else if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER,
            CLIENT_FLAG_FULLSCREEN))
    {
        return c->height + frameTop (c) + frameBottom (c);
    }
    return c->height;
}

static void
fillRectangle (Display *dpy, int screen, Drawable d, Pixmap pm, int x, int y, int width, int height)
{
    XGCValues gv;
    GC gc;
    unsigned long mask;

    TRACE ("entering fillRectangle");

    if ((width < 1) || (height < 1))
    {
        return;
    }
    gv.fill_style = FillTiled;
    gv.tile = pm;
    gv.ts_x_origin = x;
    gv.ts_y_origin = y;
    gv.foreground = WhitePixel (dpy, screen);
    if (gv.tile != None)
    {
        mask = GCTile | GCFillStyle | GCTileStipXOrigin;
    }
    else
    {
        mask = GCForeground;
    }
    gc = XCreateGC (dpy, d, mask, &gv);
    XFillRectangle (dpy, d, gc, x, y, width, height);
    XFreeGC (dpy, gc);
}

static void
frameCreateTitlePixmap (Client * c, int state, int left, int right, XfwmPixmap * pm)
{
    ScreenInfo *screen_info = c->screen_info;
    DisplayInfo *display_info = screen_info->display_info;
    GdkPixmap *gpixmap = NULL;
    GdkGCValues values;
    GdkGC *gc = NULL;
    PangoLayout *layout = NULL;
    PangoRectangle logical_rect;
    int width, x = 0, tp = 0, w1 = 0, w2, w3, w4, w5, temp;
    int voffset = 0;

    TRACE ("entering frameCreateTitlePixmap");

    if (left > right)
    {
        temp = left;
        left = right;
        right = temp;
    }

    width =
        frameWidth (c) - screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width -
        screen_info->corners[CORNER_TOP_RIGHT][ACTIVE].width;
    if (width < 1)
    {
        pm->pixmap = None;
        pm->mask = None;
        pm->width = 0;
        pm->height = 0;
        return;
    }

    if (left < screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width)
    {
        left = screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width;
    }
    if (right >
        frameWidth (c) - screen_info->corners[CORNER_TOP_RIGHT][ACTIVE].width)
    {
        right =
            frameWidth (c) - screen_info->corners[CORNER_TOP_RIGHT][ACTIVE].width;
    }
    if (right < screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width)
    {
        right = screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width;
    }

    left = left - screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width;
    right = right - screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width;

    w2 = screen_info->title[TITLE_2][ACTIVE].width;
    w4 = screen_info->title[TITLE_4][ACTIVE].width;

    layout = gtk_widget_create_pango_layout (myScreenGetGtkWidget (screen_info), c->name);
    pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

    if (screen_info->params->full_width_title)
    {
        w1 = left;
        w5 = width - right;
        w3 = width - w1 - w2 - w4 - w5;
        if (w3 < 0)
        {
            w3 = 0;
        }
        switch (screen_info->params->title_alignment)
        {
            case ALIGN_LEFT:
                tp = screen_info->params->title_horizontal_offset;
                break;
            case ALIGN_RIGHT:
                tp = w3 - logical_rect.width - screen_info->params->title_horizontal_offset;
                break;
            case ALIGN_CENTER:
                tp = (w3 >> 1) - (logical_rect.width >> 1);
                break;
        }
        if (tp < screen_info->params->title_horizontal_offset)
        {
            tp = screen_info->params->title_horizontal_offset;
        }
    }
    else
    {
        w3 = logical_rect.width;
        w5 = width;
        if (w3 > width - w2 - w4)
        {
            w3 = width - w2 - w4;
        }
        if (w3 < 0)
        {
            w3 = 0;
        }
        switch (screen_info->params->title_alignment)
        {
            case ALIGN_LEFT:
                w1 = left + screen_info->params->title_horizontal_offset;
                break;
            case ALIGN_RIGHT:
                w1 = right - w2 - w3 - w4 - screen_info->params->title_horizontal_offset;
                break;
            case ALIGN_CENTER:
                w1 = left + ((right - left) / 2) - (w3 >> 1) - w2;
                break;
        }
        if (w1 < left)
        {
            w1 = left;
        }
    }

    xfwmPixmapCreate (screen_info, pm, width, frameTop (c));
    gpixmap = gdk_pixmap_foreign_new (pm->pixmap);
    gdk_drawable_set_colormap (gpixmap, gdk_colormap_get_system ());
    gc = gdk_gc_new (gpixmap);

    if (w1 > 0)
    {
        fillRectangle (clientGetXDisplay (c), screen_info->screen, pm->pixmap, screen_info->title[TITLE_1][state].pixmap,
            0, 0, w1, frameTop (c));
        fillRectangle (clientGetXDisplay (c), screen_info->screen, pm->mask, screen_info->title[TITLE_1][state].mask, 0, 0,
            w1, frameTop (c));
        x = x + w1;
    }

    fillRectangle (clientGetXDisplay (c), screen_info->screen, pm->pixmap, screen_info->title[TITLE_2][state].pixmap, x, 0,
        w2, frameTop (c));
    fillRectangle (clientGetXDisplay (c), screen_info->screen, pm->mask, screen_info->title[TITLE_2][state].mask, x, 0, w2,
        frameTop (c));
    x = x + w2;

    if (w3 > 0)
    {
        if (state == ACTIVE)
        {
            voffset = screen_info->params->title_vertical_offset_active;
        }
        else
        {
            voffset = screen_info->params->title_vertical_offset_inactive;
        }
        fillRectangle (clientGetXDisplay (c), screen_info->screen, pm->pixmap, screen_info->title[TITLE_3][state].pixmap,
            x, 0, w3, frameTop (c));
        fillRectangle (clientGetXDisplay (c), screen_info->screen, pm->mask, screen_info->title[TITLE_3][state].mask, x, 0,
            w3, frameTop (c));
        if (screen_info->params->title_shadow[state])
        {
            gdk_gc_get_values (screen_info->black_gc, &values);
            gdk_gc_set_values (gc, &values, GDK_GC_FOREGROUND);
            gdk_draw_layout (gpixmap, gc, x + tp + 1,
                (frameTop (c) + voffset - logical_rect.height) / 2 + 1,
                layout);
        }
        gdk_gc_get_values (screen_info->title_colors[state].gc, &values);
        gdk_gc_set_values (gc, &values, GDK_GC_FOREGROUND);
        gdk_draw_layout (gpixmap, gc, x + tp,
            (frameTop (c) + voffset - logical_rect.height) / 2, layout);
        x = x + w3;
    }

    if (x > right - w4)
    {
        x = right - w4;
    }
    fillRectangle (clientGetXDisplay (c), screen_info->screen, pm->pixmap, screen_info->title[TITLE_4][state].pixmap, x, 0,
        w4, frameTop (c));
    fillRectangle (clientGetXDisplay (c), screen_info->screen, pm->mask, screen_info->title[TITLE_4][state].mask, x, 0, w4,
        frameTop (c));
    x = x + w4;

    if (w5 > 0)
    {
        fillRectangle (clientGetXDisplay (c), screen_info->screen, pm->pixmap, screen_info->title[TITLE_5][state].pixmap,
            x, 0, w5, frameTop (c));
        fillRectangle (clientGetXDisplay (c), screen_info->screen, pm->mask, screen_info->title[TITLE_5][state].mask, x, 0,
            w5, frameTop (c));
    }
    g_object_unref (G_OBJECT (gc));
    g_object_unref (G_OBJECT (gpixmap));
    g_object_unref (G_OBJECT (layout));
}

static int
getButtonFromLetter (char chr, Client * c)
{
    int b = -1;

    TRACE ("entering getButtonFromLetter");

    switch (chr)
    {
        case 'H':
            if (CLIENT_CAN_HIDE_WINDOW (c))
            {
                b = HIDE_BUTTON;
            }
            break;
        case 'C':
            if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_CLOSE))
            {
                b = CLOSE_BUTTON;
            }
            break;
        case 'M':
            if (CLIENT_CAN_MAXIMIZE_WINDOW (c))
            {
                b = MAXIMIZE_BUTTON;
            }
            break;
        case 'S':
            b = SHADE_BUTTON;
            break;
        case 'T':
            if (FLAG_TEST_ALL (c->flags,
                    CLIENT_FLAG_HAS_STICK | CLIENT_FLAG_HAS_MENU))
            {
                b = STICK_BUTTON;
            }
            break;
        case 'O':
            if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MENU))
            {
                b = MENU_BUTTON;
            }
            break;
        case '|':
            b = TITLE_SEPARATOR;
            break;
        default:
            b = -1;
    }
    return b;
}

static char
getLetterFromButton (int i, Client * c)
{
    char chr = 0;

    TRACE ("entering getLetterFromButton");

    switch (i)
    {
        case HIDE_BUTTON:
            if (CLIENT_CAN_HIDE_WINDOW (c))
            {
                chr = 'H';
            }
            break;
        case CLOSE_BUTTON:
            if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_CLOSE))
            {
                chr = 'C';
            }
            break;
        case MAXIMIZE_BUTTON:
            if (CLIENT_CAN_MAXIMIZE_WINDOW (c))
            {
                chr = 'M';
            }
            break;
        case SHADE_BUTTON:
            chr = 'S';
            break;
        case STICK_BUTTON:
            if (FLAG_TEST_ALL (c->flags,
                    CLIENT_FLAG_HAS_STICK | CLIENT_FLAG_HAS_MENU))
            {
                chr = 'T';
            }
            break;
        case MENU_BUTTON:
            if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MENU))
            {
                chr = 'O';
            }
            break;
        default:
            chr = 0;
    }
    return chr;
}

static XfwmPixmap *
frameGetPixmap (Client * c, int button, int state)
{
    ScreenInfo *screen_info = c->screen_info;

    switch (button)
    {
        case SHADE_BUTTON:
            if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
                && screen_info->buttons[SHADE_BUTTON][state + 3].pixmap)
            {
                return &screen_info->buttons[SHADE_BUTTON][state + 3];
            }
            return &screen_info->buttons[SHADE_BUTTON][state];
            break;
        case STICK_BUTTON:
            if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY)
                && screen_info->buttons[STICK_BUTTON][state + 3].pixmap)
            {
                return &screen_info->buttons[STICK_BUTTON][state + 3];
            }
            return &screen_info->buttons[STICK_BUTTON][state];
            break;
        case MAXIMIZE_BUTTON:
            if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED)
                && screen_info->buttons[MAXIMIZE_BUTTON][state + 3].pixmap)
            {
                return &screen_info->buttons[MAXIMIZE_BUTTON][state + 3];
            }
            return &screen_info->buttons[MAXIMIZE_BUTTON][state];
            break;
        default:
            break;
    }
    return &screen_info->buttons[button][state];
}


static void
frameSetShape (Client * c, int state, ClientPixmapCache * pm_cache, int button_x[BUTTON_COUNT])
{
    ScreenInfo *screen_info = c->screen_info;
    DisplayInfo *display_info = screen_info->display_info;
    Window temp;
    XRectangle rect;
    XfwmPixmap *my_pixmap;
    int i;
   
    TRACE ("entering frameSetShape");
    TRACE ("setting shape for client (0x%lx)", c->window);

    if (!display_info->shape)
    {
        return;
    }

    temp = XCreateSimpleWindow (display_info->dpy, screen_info->xroot, 0, 0, frameWidth (c), frameHeight (c), 0, 0, 0);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        rect.x = 0;
        rect.y = 0;
        rect.width = frameWidth (c);
        rect.height = frameHeight (c);
        XShapeCombineRectangles (display_info->dpy, temp, ShapeBounding, 0, 0, &rect, 1,
            ShapeSubtract, 0);
    }
    else
    {
        XShapeCombineShape (display_info->dpy, temp, ShapeBounding, frameLeft (c),
            frameTop (c), c->window, ShapeBounding, ShapeSet);
    }
    if (pm_cache)
    {
        XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->title), ShapeBounding, 0,
            0, pm_cache->pm_title[state].mask, ShapeSet);
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->sides[SIDE_LEFT]),
                ShapeBounding, 0, 0,
                pm_cache->pm_sides[SIDE_LEFT][state].mask, ShapeSet);
            XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->sides[SIDE_RIGHT]),
                ShapeBounding, 0, 0,
                pm_cache->pm_sides[SIDE_RIGHT][state].mask, ShapeSet);
        }
        XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->sides[SIDE_BOTTOM]),
            ShapeBounding, 0, 0, pm_cache->pm_sides[SIDE_BOTTOM][state].mask,
            ShapeSet);
        XShapeCombineMask (display_info->dpy,
            MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_LEFT]), ShapeBounding,
            0, 0, screen_info->corners[CORNER_BOTTOM_LEFT][state].mask, ShapeSet);
        XShapeCombineMask (display_info->dpy,
            MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_RIGHT]), ShapeBounding,
            0, 0, screen_info->corners[CORNER_BOTTOM_RIGHT][state].mask, ShapeSet);
        XShapeCombineMask (display_info->dpy,
            MYWINDOW_XWINDOW (c->corners[CORNER_TOP_LEFT]), ShapeBounding, 0,
            0, screen_info->corners[CORNER_TOP_LEFT][state].mask, ShapeSet);
        XShapeCombineMask (display_info->dpy,
            MYWINDOW_XWINDOW (c->corners[CORNER_TOP_RIGHT]), ShapeBounding, 0,
            0, screen_info->corners[CORNER_TOP_RIGHT][state].mask, ShapeSet);

        for (i = 0; i < BUTTON_COUNT; i++)
        {
            my_pixmap =
                frameGetPixmap (c, i, c->button_pressed[i] ? PRESSED : state);
            XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->buttons[i]),
                ShapeBounding, 0, 0, my_pixmap->mask, ShapeSet);
        }

        if (screen_info->corners[CORNER_TOP_LEFT][ACTIVE].height >
            frameHeight (c) - frameBottom (c) + 1)
        {
            rect.x = 0;
            rect.y = frameHeight (c) - frameBottom (c) + 1;
            rect.width = screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width;
            rect.height =
                screen_info->corners[CORNER_TOP_LEFT][ACTIVE].height -
                (frameHeight (c) - frameBottom (c) + 1);
            XShapeCombineRectangles (display_info->dpy,
                MYWINDOW_XWINDOW (c->corners[CORNER_TOP_LEFT]), ShapeBounding,
                0, 0, &rect, 1, ShapeSubtract, 0);
        }
        if (screen_info->corners[CORNER_TOP_RIGHT][ACTIVE].height >
            frameHeight (c) - frameBottom (c) + 1)
        {
            rect.x = 0;
            rect.y = frameHeight (c) - frameBottom (c) + 1;
            rect.width = screen_info->corners[CORNER_TOP_RIGHT][ACTIVE].width;
            rect.height =
                screen_info->corners[CORNER_TOP_RIGHT][ACTIVE].height -
                (frameHeight (c) - frameBottom (c) + 1);
            XShapeCombineRectangles (display_info->dpy,
                MYWINDOW_XWINDOW (c->corners[CORNER_TOP_RIGHT]),
                ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
        }
        if (screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE].height >
            frameHeight (c) - frameTop (c) + 1)
        {
            rect.x = 0;
            rect.y = 0;
            rect.width = screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE].width;
            rect.height =
                screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE].height -
                (frameHeight (c) - frameTop (c) + 1);
            XShapeCombineRectangles (display_info->dpy,
                MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_LEFT]),
                ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
        }
        if (screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE].height >
            frameHeight (c) - frameTop (c) + 1)
        {
            rect.x = 0;
            rect.y = 0;
            rect.width = screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE].width;
            rect.height =
                screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE].height -
                (frameHeight (c) - frameTop (c) + 1);
            XShapeCombineRectangles (display_info->dpy,
                MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_RIGHT]),
                ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
        }

        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            XShapeCombineShape (display_info->dpy, temp, ShapeBounding, 0, frameTop (c),
                MYWINDOW_XWINDOW (c->sides[SIDE_LEFT]), ShapeBounding,
                ShapeUnion);
            XShapeCombineShape (display_info->dpy, temp, ShapeBounding,
                frameWidth (c) - frameRight (c), frameTop (c),
                MYWINDOW_XWINDOW (c->sides[SIDE_RIGHT]), ShapeBounding,
                ShapeUnion);
        }
        XShapeCombineShape (display_info->dpy, temp, ShapeBounding,
            screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width, 0,
            MYWINDOW_XWINDOW (c->title), ShapeBounding, ShapeUnion);
        XShapeCombineShape (display_info->dpy, temp, ShapeBounding,
            screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE].width,
            frameHeight (c) - frameBottom (c),
            MYWINDOW_XWINDOW (c->sides[SIDE_BOTTOM]), ShapeBounding,
            ShapeUnion);
        XShapeCombineShape (display_info->dpy, temp, ShapeBounding, 0,
            frameHeight (c) -
            screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE].height,
            MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_LEFT]), ShapeBounding,
            ShapeUnion);
        XShapeCombineShape (display_info->dpy, temp, ShapeBounding,
            frameWidth (c) -
            screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE].width,
            frameHeight (c) -
            screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE].height,
            MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_RIGHT]), ShapeBounding,
            ShapeUnion);
        XShapeCombineShape (display_info->dpy, temp, ShapeBounding, 0, 0,
            MYWINDOW_XWINDOW (c->corners[CORNER_TOP_LEFT]), ShapeBounding,
            ShapeUnion);
        XShapeCombineShape (display_info->dpy, temp, ShapeBounding,
            frameWidth (c) - screen_info->corners[CORNER_TOP_RIGHT][ACTIVE].width,
            0, MYWINDOW_XWINDOW (c->corners[CORNER_TOP_RIGHT]), ShapeBounding,
            ShapeUnion);

        for (i = 0; i < BUTTON_COUNT; i++)
        {
            char b = getLetterFromButton (i, c);
            if ((b) && strchr (screen_info->params->button_layout, b))
            {
                XShapeCombineShape (display_info->dpy, temp, ShapeBounding, button_x[i],
                    (frameTop (c) - screen_info->buttons[i][ACTIVE].height) / 2,
                    MYWINDOW_XWINDOW (c->buttons[i]), ShapeBounding,
                    ShapeUnion);
            }
        }
    }
    XShapeCombineShape (display_info->dpy, c->frame, ShapeBounding, 0, 0, temp,
        ShapeBounding, ShapeSet);
    XDestroyWindow (display_info->dpy, temp);
}

void
frameDraw (Client * c, gboolean invalidate_cache, gboolean force_shape_update)
{
    ScreenInfo *screen_info = NULL;
    XfwmPixmap *my_pixmap;
    gboolean requires_clearing = FALSE;
    int state = ACTIVE;
    int i;
    int j;
    int x;
    int button;
    int left;
    int right;
    int top_width;
    int bottom_width;
    int left_height;
    int right_height;
    int button_x[BUTTON_COUNT];

    TRACE ("entering frameDraw");
    TRACE ("drawing frame for \"%s\" (0x%lx)", c->name, c->window);

    g_return_if_fail (c != NULL);

    screen_info = c->screen_info;
    if (c != clientGetFocus ())
    {
        TRACE ("\"%s\" is not the active window", c->name);
        state = INACTIVE;
    }
    if ((state == INACTIVE) && ((c->draw_active) || (c->first_map)))
    {
        requires_clearing = TRUE;
        c->draw_active = FALSE;
    }
    else if ((state == ACTIVE) && (!(c->draw_active) || (c->first_map)))
    {
        requires_clearing = TRUE;
        c->draw_active = TRUE;
    }

    /* Flag clearance */
    if (c->first_map)
    {
        c->first_map = FALSE;
    }

    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER,
            CLIENT_FLAG_FULLSCREEN))
    {
        /* Cache mgmt */
        if (invalidate_cache)
        {
            clientClearPixmapCache (c);
            requires_clearing = TRUE;
        }
        else
        {
            if (c->pm_cache.previous_width != c->width)
            {
                xfwmPixmapFree (&c->pm_cache.pm_title[ACTIVE]);
                xfwmPixmapFree (&c->pm_cache.pm_title[INACTIVE]);
                xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_BOTTOM][ACTIVE]);
                xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_BOTTOM][INACTIVE]);
                c->pm_cache.previous_width = c->width;
                requires_clearing = TRUE;
            }
            if (c->pm_cache.previous_height != c->height)
            {
                xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_LEFT][ACTIVE]);
                xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_LEFT][INACTIVE]);
                xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_RIGHT][ACTIVE]);
                xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_RIGHT][INACTIVE]);
                c->pm_cache.previous_height = c->height;
                requires_clearing = TRUE;
            }
        }

        /* First, hide the buttons that we don't have... */
        for (i = 0; i < BUTTON_COUNT; i++)
        {
            char b = getLetterFromButton (i, c);
            if ((!b) || !strchr (screen_info->params->button_layout, b))
            {
                xfwmWindowHide (&c->buttons[i]);
            }
        }

        /* Then, show the ones that we do have on right... */
        x = frameLeft (c) + screen_info->params->button_offset;
        for (i = 0; i < strlen (screen_info->params->button_layout); i++)
        {
            button = getButtonFromLetter (screen_info->params->button_layout[i], c);
            if (button == TITLE_SEPARATOR)
            {
                break;
            }
            else if (button >= 0)
            {
                my_pixmap =
                    frameGetPixmap (c, button,
                    c->button_pressed[button] ? PRESSED : state);
                if (my_pixmap->pixmap)
                {
                    XSetWindowBackgroundPixmap (clientGetXDisplay (c),
                        MYWINDOW_XWINDOW (c->buttons[button]),
                        my_pixmap->pixmap);
                }
                xfwmWindowShow (&c->buttons[button], x,
                    (frameTop (c) - screen_info->buttons[button][ACTIVE].height) / 2,
                    screen_info->buttons[button][ACTIVE].width,
                    screen_info->buttons[button][ACTIVE].height, TRUE);
                button_x[button] = x;
                x = x + screen_info->buttons[button][ACTIVE].width +
                    screen_info->params->button_spacing;
            }
        }
        left = x - screen_info->params->button_spacing;

        /* and those that we do have on left... */
        x = frameWidth (c) - frameRight (c) + screen_info->params->button_spacing -
            screen_info->params->button_offset;
        for (j = strlen (screen_info->params->button_layout) - 1; j >= i; j--)
        {
            button = getButtonFromLetter (screen_info->params->button_layout[j], c);
            if (button == TITLE_SEPARATOR)
            {
                break;
            }
            else if (button >= 0)
            {
                my_pixmap =
                    frameGetPixmap (c, button,
                    c->button_pressed[button] ? PRESSED : state);
                if (my_pixmap->pixmap)
                {
                    XSetWindowBackgroundPixmap (clientGetXDisplay (c),
                        MYWINDOW_XWINDOW (c->buttons[button]),
                        my_pixmap->pixmap);
                }
                x = x - screen_info->buttons[button][ACTIVE].width -
                    screen_info->params->button_spacing;
                xfwmWindowShow (&c->buttons[button], x,
                    (frameTop (c) - screen_info->buttons[button][ACTIVE].height) / 2,
                    screen_info->buttons[button][ACTIVE].width,
                    screen_info->buttons[button][ACTIVE].height, TRUE);
                button_x[button] = x;
            }
        }
        right = x;

        top_width =
            frameWidth (c) - screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width -
            screen_info->corners[CORNER_TOP_RIGHT][ACTIVE].width;
        bottom_width =
            frameWidth (c) -
            screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE].width -
            screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE].width;
        left_height =
            frameHeight (c) - frameTop (c) -
            screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE].height;
        right_height =
            frameHeight (c) - frameTop (c) -
            screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE].height;

        if (c->pm_cache.pm_title[state].pixmap == None)
        {
            frameCreateTitlePixmap (c, state, left, right,
                &c->pm_cache.pm_title[state]);
            requires_clearing = TRUE;
        }

        if (c->pm_cache.pm_sides[SIDE_LEFT][state].pixmap == None)
        {
            xfwmPixmapCreate (screen_info, &c->pm_cache.pm_sides[SIDE_LEFT][state],
                frameLeft (c), left_height);
            requires_clearing = TRUE;
        }
        fillRectangle (clientGetXDisplay (c), screen_info->screen, c->pm_cache.pm_sides[SIDE_LEFT][state].pixmap,
            screen_info->sides[SIDE_LEFT][state].pixmap, 0, 0, frameLeft (c),
            left_height);
        fillRectangle (clientGetXDisplay (c), screen_info->screen, c->pm_cache.pm_sides[SIDE_LEFT][state].mask,
            screen_info->sides[SIDE_LEFT][state].mask, 0, 0, frameLeft (c),
            left_height);

        if (c->pm_cache.pm_sides[SIDE_RIGHT][state].pixmap == None)
        {
            xfwmPixmapCreate (screen_info, &c->pm_cache.pm_sides[SIDE_RIGHT][state],
                frameRight (c), right_height);
            requires_clearing = TRUE;
        }
        fillRectangle (clientGetXDisplay (c), screen_info->screen, c->pm_cache.pm_sides[SIDE_RIGHT][state].pixmap,
            screen_info->sides[SIDE_RIGHT][state].pixmap, 0, 0, frameRight (c),
            right_height);
        fillRectangle (clientGetXDisplay (c), screen_info->screen, c->pm_cache.pm_sides[SIDE_RIGHT][state].mask,
            screen_info->sides[SIDE_RIGHT][state].mask, 0, 0, frameRight (c),
            right_height);

        if (c->pm_cache.pm_sides[SIDE_BOTTOM][state].pixmap == None)
        {
            xfwmPixmapCreate (screen_info, &c->pm_cache.pm_sides[SIDE_BOTTOM][state],
                bottom_width, frameBottom (c));
            requires_clearing = TRUE;
        }
        fillRectangle (clientGetXDisplay (c), screen_info->screen, c->pm_cache.pm_sides[SIDE_BOTTOM][state].pixmap,
            screen_info->sides[SIDE_BOTTOM][state].pixmap, 0, 0, bottom_width,
            frameBottom (c));
        fillRectangle (clientGetXDisplay (c), screen_info->screen, c->pm_cache.pm_sides[SIDE_BOTTOM][state].mask,
            screen_info->sides[SIDE_BOTTOM][state].mask, 0, 0, bottom_width,
            frameBottom (c));

        XSetWindowBackgroundPixmap (clientGetXDisplay (c), MYWINDOW_XWINDOW (c->title),
            c->pm_cache.pm_title[state].pixmap);
        XSetWindowBackgroundPixmap (clientGetXDisplay (c),
            MYWINDOW_XWINDOW (c->sides[SIDE_LEFT]),
            c->pm_cache.pm_sides[SIDE_LEFT][state].pixmap);
        XSetWindowBackgroundPixmap (clientGetXDisplay (c),
            MYWINDOW_XWINDOW (c->sides[SIDE_RIGHT]),
            c->pm_cache.pm_sides[SIDE_RIGHT][state].pixmap);
        XSetWindowBackgroundPixmap (clientGetXDisplay (c),
            MYWINDOW_XWINDOW (c->sides[SIDE_BOTTOM]),
            c->pm_cache.pm_sides[SIDE_BOTTOM][state].pixmap);
        XSetWindowBackgroundPixmap (clientGetXDisplay (c),
            MYWINDOW_XWINDOW (c->corners[CORNER_TOP_LEFT]),
            screen_info->corners[CORNER_TOP_LEFT][state].pixmap);
        XSetWindowBackgroundPixmap (clientGetXDisplay (c),
            MYWINDOW_XWINDOW (c->corners[CORNER_TOP_RIGHT]),
            screen_info->corners[CORNER_TOP_RIGHT][state].pixmap);
        XSetWindowBackgroundPixmap (clientGetXDisplay (c),
            MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_LEFT]),
            screen_info->corners[CORNER_BOTTOM_LEFT][state].pixmap);
        XSetWindowBackgroundPixmap (clientGetXDisplay (c),
            MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_RIGHT]),
            screen_info->corners[CORNER_BOTTOM_RIGHT][state].pixmap);

        if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            xfwmWindowHide (&c->sides[SIDE_LEFT]);
            xfwmWindowHide (&c->sides[SIDE_RIGHT]);
        }
        else
        {
            xfwmWindowShow (&c->sides[SIDE_LEFT], 0, frameTop (c),
                frameLeft (c), left_height, requires_clearing);
            xfwmWindowShow (&c->sides[SIDE_RIGHT],
                frameWidth (c) - frameRight (c), frameTop (c), frameRight (c),
                right_height, requires_clearing);
        }

        xfwmWindowShow (&c->title,
            screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width, 0, top_width,
            frameTop (c), requires_clearing);
        xfwmWindowShow (&c->sides[SIDE_BOTTOM],
            screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE].width,
            frameHeight (c) - frameBottom (c), bottom_width, frameBottom (c),
            requires_clearing);

        xfwmWindowShow (&c->corners[CORNER_TOP_LEFT], 0, 0,
            screen_info->corners[CORNER_TOP_LEFT][ACTIVE].width,
            screen_info->corners[CORNER_TOP_LEFT][ACTIVE].height,
            requires_clearing);
        xfwmWindowShow (&c->corners[CORNER_TOP_RIGHT],
            frameWidth (c) - screen_info->corners[CORNER_TOP_RIGHT][ACTIVE].width,
            0, screen_info->corners[CORNER_TOP_RIGHT][ACTIVE].width,
            screen_info->corners[CORNER_TOP_RIGHT][ACTIVE].height,
            requires_clearing);
        xfwmWindowShow (&c->corners[CORNER_BOTTOM_LEFT], 0,
            frameHeight (c) -
            screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE].height,
            screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE].width,
            screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE].height,
            requires_clearing);
        xfwmWindowShow (&c->corners[CORNER_BOTTOM_RIGHT],
            frameWidth (c) -
            screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE].width,
            frameHeight (c) -
            screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE].height,
            screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE].width,
            screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE].height,
            requires_clearing);

        if (requires_clearing | force_shape_update)
        {
            frameSetShape (c, state, &c->pm_cache, button_x);
        }
    }
    else
    {
        xfwmWindowHide (&c->title);
        for (i = 0; i < 3; i++)
        {
            if (MYWINDOW_XWINDOW (c->sides[i]))
            {
                xfwmWindowHide (&c->sides[i]);
            }
        }
        for (i = 0; i < 4; i++)
        {
            if (MYWINDOW_XWINDOW (c->corners[i]))
            {
                xfwmWindowHide (&c->corners[i]);
            }
        }
        for (i = 0; i < BUTTON_COUNT; i++)
        {
            if (MYWINDOW_XWINDOW (c->buttons[i]))
            {
                xfwmWindowHide (&c->buttons[i]);
            }
        }
        frameSetShape (c, 0, NULL, 0);
    }
}
