/*
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; You may only use version 2 of the License,
	you have no option to use any other version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <pango/pango.h>
#include "main.h"
#include "client.h"
#include "settings.h"
#include "frame.h"
#include "debug.h"

inline int frameLeft(Client * c)
{
    DBG("entering frameLeft\n");

    if((c->has_border) && !(c->fullscreen))
    {
        return sides[SIDE_LEFT][ACTIVE].width;
    }
    return 0;
}

inline int frameRight(Client * c)
{
    DBG("entering frameRight\n");

    if((c->has_border) && !(c->fullscreen))
    {
        return sides[SIDE_RIGHT][ACTIVE].width;
    }
    return 0;
}

inline int frameTop(Client * c)
{
    DBG("entering frameTop\n");

    if((c->has_border) && !(c->fullscreen))
    {
        return title[TITLE_3][ACTIVE].height;
    }
    return 0;
}

inline int frameBottom(Client * c)
{
    DBG("entering frameBottom\n");

    if((c->has_border) && !(c->fullscreen))
    {
        return sides[SIDE_BOTTOM][ACTIVE].height;
    }
    return 0;
}

inline int frameX(Client * c)
{
    DBG("entering frameX\n");

    if((c->has_border) && !(c->fullscreen))
    {
        return c->x - frameLeft(c);
    }
    return c->x;
}

inline int frameY(Client * c)
{
    DBG("entering frameY\n");

    if((c->has_border) && !(c->fullscreen))
    {
        return c->y - frameTop(c);
    }
    return c->y;
}

inline int frameWidth(Client * c)
{
    DBG("entering frameWidth\n");

    if((c->has_border) && !(c->fullscreen))
    {
        return c->width + frameLeft(c) + frameRight(c);
    }
    return c->width;
}

inline int frameHeight(Client * c)
{
    DBG("entering frameHeight\n");

    if((c->has_border) && (c->shaded) && !(c->fullscreen))
    {
        return frameTop(c) + frameBottom(c);
    }
    else if((c->has_border) && !(c->fullscreen))
    {
        return c->height + frameTop(c) + frameBottom(c);
    }
    return c->height;
}

static inline void fillRectangle(Display * dpy, Drawable d, Pixmap pm, int x, int y, int width, int height)
{
    XGCValues gv;
    GC gc;
    unsigned long mask;

    DBG("entering fillRectangle\n");

    gv.fill_style = FillTiled;
    gv.tile = pm;
    gv.ts_x_origin = x;
    gv.ts_y_origin = y;
    gv.foreground = WhitePixel(dpy, screen);
    if(gv.tile != None)
    {
        mask = GCTile | GCFillStyle | GCTileStipXOrigin;
    }
    else
    {
        mask = GCForeground;
    }
    gc = XCreateGC(dpy, d, mask, &gv);
    XFillRectangle(dpy, d, gc, x, y, width, height);
    XFreeGC(dpy, gc);
}

static void frameCreateTitlePixmap(Client * c, int state, int left, int right, MyPixmap * pm)
{
    int width, x = 0, tp = 0, w1 = 0, w2, w3, w4, w5, temp;
    GdkPixmap *gpixmap = NULL;
    GdkGCValues values;
    GdkGC *gc = NULL;
    PangoLayout *layout = NULL;
    PangoRectangle logical_rect;

    DBG("entering frameCreateTitlePixmap\n");

    if(left > right)
    {
        temp = left;
        left = right;
        right = temp;
    }

    width = frameWidth(c) - corners[CORNER_TOP_LEFT][ACTIVE].width - corners[CORNER_TOP_RIGHT][ACTIVE].width;
    if(left < corners[CORNER_TOP_LEFT][ACTIVE].width)
    {
        left = corners[CORNER_TOP_LEFT][ACTIVE].width;
    }
    if(right > frameWidth(c) - corners[CORNER_TOP_RIGHT][ACTIVE].width)
    {
        right = frameWidth(c) - corners[CORNER_TOP_RIGHT][ACTIVE].width;
    }
    if(right < corners[CORNER_TOP_LEFT][ACTIVE].width)
    {
        right = corners[CORNER_TOP_LEFT][ACTIVE].width;
    }

    left = left - corners[CORNER_TOP_LEFT][ACTIVE].width;
    right = right - corners[CORNER_TOP_LEFT][ACTIVE].width;

    w2 = title[TITLE_2][ACTIVE].width;
    w4 = title[TITLE_4][ACTIVE].width;
    
    layout = gtk_widget_create_pango_layout (getDefaultGtkWidget (), c->name);
    pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
    
    if(full_width_title)
    {
        w1 = left;
        w5 = width - right;
        w3 = width - w1 - w2 - w4 - w5;
        if(w3 < 0)
	{
            w3 = 0;
        }
	switch (title_alignment)
        {
            case ALIGN_RIGHT:
                tp = w3 - logical_rect.width;
                break;
            case ALIGN_CENTER:
                tp = (w3 / 2) - (logical_rect.width / 2);
                break;
        }
        if(tp < 0)
	{
            tp = 0;
	}
    }
    else
    {
        w3 = logical_rect.width;
        w5 = width;
        if(w3 > width - w2 - w4)
	{
            w3 = width - w2 - w4;
	}
        if(w3 < 0)
	{
            w3 = 0;
	}
        switch (title_alignment)
        {
            case ALIGN_LEFT:
                w1 = left;
                break;
            case ALIGN_RIGHT:
                w1 = right - w2 - w3 - w4;
                break;
            case ALIGN_CENTER:
                w1 = left + ((right - left) / 2) - (w3 / 2) - w2;
                break;
        }
        if(w1 < left)
	{
            w1 = left;
	}
    }

    createPixmap (dpy, pm, width, frameTop(c));
    gpixmap = gdk_pixmap_foreign_new (pm->pixmap);
    gdk_drawable_set_colormap(gpixmap, gdk_colormap_get_system());
    gc = gdk_gc_new(gpixmap);
    gdk_gc_get_values (title_colors[state].gc, &values);
    gdk_gc_set_values (gc, &values, GDK_GC_FOREGROUND);
    
    if(w1 > 0)
    {
        fillRectangle(dpy, pm->pixmap, title[TITLE_1][state].pixmap, 0, 0, w1, frameTop(c));
        fillRectangle(dpy, pm->mask,   title[TITLE_1][state].mask,   0, 0, w1, frameTop(c));
        x = x + w1;
    }

    fillRectangle(dpy, pm->pixmap, title[TITLE_2][state].pixmap, x, 0, w2, frameTop(c));
    fillRectangle(dpy, pm->mask,   title[TITLE_2][state].mask,   x, 0, w2, frameTop(c));
    x = x + w2;

    if(w3 > 0)
    {
        fillRectangle(dpy, pm->pixmap, title[TITLE_3][state].pixmap, x, 0, w3, frameTop(c));
        fillRectangle(dpy, pm->mask,   title[TITLE_3][state].mask,   x, 0, w3, frameTop(c));
        gdk_draw_layout(gpixmap, gc, x + tp, (frameTop(c) + title_vertical_offset - logical_rect.height) / 2, layout);
        x = x + w3;
    }

    if(x > right - w4)
    {
        x = right - w4;
    }
    fillRectangle(dpy, pm->pixmap, title[TITLE_4][state].pixmap, x, 0, w4, frameTop(c));
    fillRectangle(dpy, pm->mask,   title[TITLE_4][state].mask,   x, 0, w4, frameTop(c));
    x = x + w4;

    if(w5 > 0)
    {
        fillRectangle(dpy, pm->pixmap, title[TITLE_5][state].pixmap, x, 0, w5, frameTop(c));
        fillRectangle(dpy, pm->mask,   title[TITLE_5][state].mask,   x, 0, w5, frameTop(c));
    }
    g_object_unref (G_OBJECT (gc));
    g_object_unref (G_OBJECT (gpixmap));
    g_object_unref (G_OBJECT (layout));
}

static int getButtonFromLetter(char c)
{
    int b;

    DBG("entering getButtonFromLetter\n");

    switch (c)
    {
        case 'H':
            b = HIDE_BUTTON;
            break;
        case 'C':
            b = CLOSE_BUTTON;
            break;
        case 'M':
            b = MAXIMIZE_BUTTON;
            break;
        case 'S':
            b = SHADE_BUTTON;
            break;
        case 'T':
            b = STICK_BUTTON;
            break;
        case 'O':
            b = MENU_BUTTON;
            break;
        default:
            b = -1;
    }
    return b;
}

static char getLetterFromButton(int b)
{
    char c;

    DBG("entering getLetterFromButton\n");

    switch (b)
    {
        case HIDE_BUTTON:
            c = 'H';
            break;
        case CLOSE_BUTTON:
            c = 'C';
            break;
        case MAXIMIZE_BUTTON:
            c = 'M';
            break;
        case SHADE_BUTTON:
            c = 'S';
            break;
        case STICK_BUTTON:
            c = 'T';
            break;
        case MENU_BUTTON:
            c = 'O';
            break;
        default:
            c = 0;
    }
    return c;
}

static void frameSetShape(Client * c, int state, MyPixmap * title, MyPixmap pm_sides[3], int button_x[BUTTON_COUNT])
{
    Window temp;
    int i;
    XRectangle rect;

    DBG("entering frameSetShape\n");
    DBG("setting shape for client (%#lx)\n", c->window);

    if(!shape)
    {
        return;
    }
    
    temp = XCreateSimpleWindow(dpy, root, 0, 0, frameWidth(c), frameHeight(c), 0, 0, 0);

    if(c->shaded)
    {
        rect.x = 0;
        rect.y = 0;
        rect.width = frameWidth(c);
        rect.height = frameHeight(c);
        XShapeCombineRectangles(dpy, temp, ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
    }
    else
    {
        XShapeCombineShape(dpy, temp, ShapeBounding, frameLeft(c), frameTop(c), c->window, ShapeBounding, ShapeSet);
    }
    if((c->has_border) && !(c->fullscreen))
    {
        XShapeCombineMask(dpy, c->title, ShapeBounding, 0, 0, title->mask, ShapeSet);
        for(i = 0; i < 3; i++)
	{
            XShapeCombineMask(dpy, c->sides[i], ShapeBounding, 0, 0, pm_sides[i].mask, ShapeSet);
	}
        for(i = 0; i < 4; i++)
        {
	    XShapeCombineMask(dpy, c->corners[i], ShapeBounding, 0, 0, corners[i][state].mask, ShapeSet);
        }
	for(i = 0; i < BUTTON_COUNT; i++)
        {
            if(c->button_pressed[i])
	    {
                XShapeCombineMask(dpy, c->buttons[i], ShapeBounding, 0, 0, buttons[i][PRESSED].mask, ShapeSet);
            }
	    else
            {
	        XShapeCombineMask(dpy, c->buttons[i], ShapeBounding, 0, 0, buttons[i][state].mask, ShapeSet);
	    }
        }

        if(corners[CORNER_TOP_LEFT][ACTIVE].height > frameHeight(c) - frameBottom(c) + 1)
        {
            rect.x = 0;
            rect.y = frameHeight(c) - frameBottom(c) + 1;
            rect.width = corners[CORNER_TOP_LEFT][ACTIVE].width;
            rect.height = corners[CORNER_TOP_LEFT][ACTIVE].height - (frameHeight(c) - frameBottom(c) + 1);
            XShapeCombineRectangles(dpy, c->corners[CORNER_TOP_LEFT], ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
        }
        if(corners[CORNER_TOP_RIGHT][ACTIVE].height > frameHeight(c) - frameBottom(c) + 1)
        {
            rect.x = 0;
            rect.y = frameHeight(c) - frameBottom(c) + 1;
            rect.width = corners[CORNER_TOP_RIGHT][ACTIVE].width;
            rect.height = corners[CORNER_TOP_RIGHT][ACTIVE].height - (frameHeight(c) - frameBottom(c) + 1);
            XShapeCombineRectangles(dpy, c->corners[CORNER_TOP_RIGHT], ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
        }
        if(corners[CORNER_BOTTOM_LEFT][ACTIVE].height > frameHeight(c) - frameTop(c) + 1)
        {
            rect.x = 0;
            rect.y = 0;
            rect.width = corners[CORNER_BOTTOM_LEFT][ACTIVE].width;
            rect.height = corners[CORNER_BOTTOM_LEFT][ACTIVE].height - (frameHeight(c) - frameTop(c) + 1);
            XShapeCombineRectangles(dpy, c->corners[CORNER_BOTTOM_LEFT], ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
        }
        if(corners[CORNER_BOTTOM_RIGHT][ACTIVE].height > frameHeight(c) - frameTop(c) + 1)
        {
            rect.x = 0;
            rect.y = 0;
            rect.width = corners[CORNER_BOTTOM_RIGHT][ACTIVE].width;
            rect.height = corners[CORNER_BOTTOM_RIGHT][ACTIVE].height - (frameHeight(c) - frameTop(c) + 1);
            XShapeCombineRectangles(dpy, c->corners[CORNER_BOTTOM_RIGHT], ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
        }

        XShapeCombineShape(dpy, temp, ShapeBounding, 0, frameTop(c), c->sides[SIDE_LEFT], ShapeBounding, ShapeUnion);
        XShapeCombineShape(dpy, temp, ShapeBounding, frameWidth(c) - frameRight(c), frameTop(c), c->sides[SIDE_RIGHT], ShapeBounding, ShapeUnion);
        XShapeCombineShape(dpy, temp, ShapeBounding, corners[CORNER_TOP_LEFT][ACTIVE].width, 0, c->title, ShapeBounding, ShapeUnion);
        XShapeCombineShape(dpy, temp, ShapeBounding, corners[CORNER_BOTTOM_LEFT][ACTIVE].width, frameHeight(c) - frameBottom(c), c->sides[SIDE_BOTTOM], ShapeBounding, ShapeUnion);
        XShapeCombineShape(dpy, temp, ShapeBounding, 0, frameHeight(c) - corners[CORNER_BOTTOM_LEFT][ACTIVE].height, c->corners[CORNER_BOTTOM_LEFT], ShapeBounding, ShapeUnion);
        XShapeCombineShape(dpy, temp, ShapeBounding, frameWidth(c) - corners[CORNER_BOTTOM_RIGHT][ACTIVE].width, frameHeight(c) - corners[CORNER_BOTTOM_RIGHT][ACTIVE].height, c->corners[CORNER_BOTTOM_RIGHT], ShapeBounding, ShapeUnion);
        XShapeCombineShape(dpy, temp, ShapeBounding, 0, 0, c->corners[CORNER_TOP_LEFT], ShapeBounding, ShapeUnion);
        XShapeCombineShape(dpy, temp, ShapeBounding, frameWidth(c) - corners[CORNER_TOP_RIGHT][ACTIVE].width, 0, c->corners[CORNER_TOP_RIGHT], ShapeBounding, ShapeUnion);
        for(i = 0; i < BUTTON_COUNT; i++)
	{
            if(strchr(button_layout, getLetterFromButton(i)))
	    {
                XShapeCombineShape(dpy, temp, ShapeBounding, button_x[i], (frameTop(c) - buttons[i][ACTIVE].height) / 2, c->buttons[i], ShapeBounding, ShapeUnion);
	    }
	}
    }
    XShapeCombineShape(dpy, c->frame, ShapeBounding, 0, 0, temp, ShapeBounding, ShapeSet);
    XDestroyWindow(dpy, temp);
}

void frameDraw(Client * c)
{
    int state = ACTIVE;
    int i;
    int x;
    int button;
    int left;
    int right;
    int top_width;
    int bottom_width;
    int left_height;
    int right_height;
    int button_x[BUTTON_COUNT];
    MyPixmap pm_title, pm_sides[3];

    DBG("entering frameDraw\n");
    DBG("drawing frame for \"%s\" (%#lx)\n", c->name, c->window);

    if(c != clientGetFocus())
    {
        DBG("\"%s\" is not the active window\n", c->name);
        state = INACTIVE;
    }
    if((c->has_border) && !(c->fullscreen))
    {
        XMapWindow(dpy, c->title);
        for(i = 0; i < 3; i++)
	{
            XMapWindow(dpy, c->sides[i]);
        }
	for(i = 0; i < 4; i++)
        {
	    XMapWindow(dpy, c->corners[i]);
        }
	for(i = 0; i < BUTTON_COUNT; i++)
        {
            if(strchr(button_layout, getLetterFromButton(i)))
	    {
                XMapWindow(dpy, c->buttons[i]);
            }
	    else
	    {
                XUnmapWindow(dpy, c->buttons[i]);
	    }
        }

        x = frameLeft(c) + button_offset;
        for(i = 0; i < strlen(button_layout); i++)
        {
            button = getButtonFromLetter(button_layout[i]);
            if(button >= 0)
            {
                XMoveResizeWindow(dpy, c->buttons[button], x, (frameTop(c) - buttons[button][ACTIVE].height) / 2, buttons[button][ACTIVE].width, buttons[button][ACTIVE].height);
                button_x[button] = x;
                x = x + buttons[button][ACTIVE].width + button_spacing;
            }
            else
	    {
                break;
	    }
        }
        left = x - button_spacing;

        x = frameWidth(c) - frameRight(c) + button_spacing - button_offset;
        for(i = strlen(button_layout) - 1; i >= 0; i--)
        {
            button = getButtonFromLetter(button_layout[i]);
            if(button >= 0)
            {
                x = x - buttons[button][ACTIVE].width - button_spacing;
                XMoveResizeWindow(dpy, c->buttons[button], x, (frameTop(c) - buttons[button][ACTIVE].height) / 2, buttons[button][ACTIVE].width, buttons[button][ACTIVE].height);
                button_x[button] = x;
            }
            else
	    {
                break;
            }
	}
        right = x;

        top_width    = frameWidth(c) - corners[CORNER_TOP_LEFT][ACTIVE].width - corners[CORNER_TOP_RIGHT][ACTIVE].width;
        bottom_width = frameWidth(c) - corners[CORNER_BOTTOM_LEFT][ACTIVE].width - corners[CORNER_BOTTOM_RIGHT][ACTIVE].width;
        left_height  = frameHeight(c) - frameTop(c) - corners[CORNER_BOTTOM_LEFT][ACTIVE].height;
        right_height = frameHeight(c) - frameTop(c) - corners[CORNER_BOTTOM_RIGHT][ACTIVE].height;

        frameCreateTitlePixmap(c, state, left, right, &pm_title);

        createPixmap (dpy, &pm_sides[SIDE_LEFT], frameLeft(c), left_height);
        fillRectangle(dpy, pm_sides[SIDE_LEFT].pixmap, sides[SIDE_LEFT][state].pixmap, 0, 0, frameLeft(c), left_height);
        fillRectangle(dpy, pm_sides[SIDE_LEFT].mask,   sides[SIDE_LEFT][state].mask,   0, 0, frameLeft(c), left_height);

        createPixmap (dpy, &pm_sides[SIDE_RIGHT], frameRight(c), right_height);
        fillRectangle(dpy, pm_sides[SIDE_RIGHT].pixmap, sides[SIDE_RIGHT][state].pixmap, 0, 0, frameRight(c), right_height);
        fillRectangle(dpy, pm_sides[SIDE_RIGHT].mask,   sides[SIDE_RIGHT][state].mask,   0, 0, frameRight(c), right_height);

        createPixmap (dpy, &pm_sides[SIDE_BOTTOM], bottom_width, frameBottom(c));
        fillRectangle(dpy, pm_sides[SIDE_BOTTOM].pixmap, sides[SIDE_BOTTOM][state].pixmap, 0, 0, bottom_width, frameBottom(c));
        fillRectangle(dpy, pm_sides[SIDE_BOTTOM].mask,   sides[SIDE_BOTTOM][state].mask,   0, 0, bottom_width, frameBottom(c));

        XSetWindowBackgroundPixmap(dpy, c->title, pm_title.pixmap);
        XSetWindowBackgroundPixmap(dpy, c->sides[SIDE_LEFT], pm_sides[SIDE_LEFT].pixmap);
        XSetWindowBackgroundPixmap(dpy, c->sides[SIDE_RIGHT], pm_sides[SIDE_RIGHT].pixmap);
        XSetWindowBackgroundPixmap(dpy, c->sides[SIDE_BOTTOM], pm_sides[SIDE_BOTTOM].pixmap);
        XSetWindowBackgroundPixmap(dpy, c->corners[CORNER_TOP_LEFT], corners[CORNER_TOP_LEFT][state].pixmap);
        XSetWindowBackgroundPixmap(dpy, c->corners[CORNER_TOP_RIGHT], corners[CORNER_TOP_RIGHT][state].pixmap);
        XSetWindowBackgroundPixmap(dpy, c->corners[CORNER_BOTTOM_LEFT], corners[CORNER_BOTTOM_LEFT][state].pixmap);
        XSetWindowBackgroundPixmap(dpy, c->corners[CORNER_BOTTOM_RIGHT], corners[CORNER_BOTTOM_RIGHT][state].pixmap);
                                       
        for(i = 0; i < BUTTON_COUNT; i++)
        {
            if(c->button_pressed[i])
	    {
                XSetWindowBackgroundPixmap(dpy, c->buttons[i], buttons[i][PRESSED].pixmap);
            }
	    else
	    {
                XSetWindowBackgroundPixmap(dpy, c->buttons[i], buttons[i][state].pixmap);
	    }
        }

        XMoveResizeWindow(dpy, c->title, corners[CORNER_TOP_LEFT][ACTIVE].width, 0, top_width < 1 ? 1 : top_width, frameTop(c));
        XMoveResizeWindow(dpy, c->sides[SIDE_LEFT], 0, frameTop(c), frameLeft(c), left_height < 1 ? 1 : left_height);
        XMoveResizeWindow(dpy, c->sides[SIDE_RIGHT], frameWidth(c) - frameRight(c), frameTop(c), frameRight(c), right_height < 1 ? 1 : right_height);
        XMoveResizeWindow(dpy, c->sides[SIDE_BOTTOM], corners[CORNER_BOTTOM_LEFT][ACTIVE].width, frameHeight(c) - frameBottom(c), bottom_width < 1 ? 1 : bottom_width, frameBottom(c));

        XMoveResizeWindow(dpy, c->corners[CORNER_TOP_LEFT], 0, 0, corners[CORNER_TOP_LEFT][ACTIVE].width, corners[CORNER_TOP_LEFT][ACTIVE].height);
        XMoveResizeWindow(dpy, c->corners[CORNER_TOP_RIGHT], frameWidth(c) - corners[CORNER_TOP_RIGHT][ACTIVE].width, 0, corners[CORNER_TOP_RIGHT][ACTIVE].width, corners[CORNER_TOP_RIGHT][ACTIVE].height);
        XMoveResizeWindow(dpy, c->corners[CORNER_BOTTOM_LEFT], 0, frameHeight(c) - corners[CORNER_BOTTOM_LEFT][ACTIVE].height, corners[CORNER_BOTTOM_LEFT][ACTIVE].width, corners[CORNER_BOTTOM_LEFT][ACTIVE].height);
        XMoveResizeWindow(dpy, c->corners[CORNER_BOTTOM_RIGHT], frameWidth(c) - corners[CORNER_BOTTOM_RIGHT][ACTIVE].width, frameHeight(c) - corners[CORNER_BOTTOM_RIGHT][ACTIVE].height, corners[CORNER_BOTTOM_RIGHT][ACTIVE].width, corners[CORNER_BOTTOM_RIGHT][ACTIVE].height);

        XClearWindow(dpy, c->title);
        XClearWindow(dpy, c->sides[SIDE_LEFT]); 
        XClearWindow(dpy, c->sides[SIDE_RIGHT]); 
        XClearWindow(dpy, c->sides[SIDE_BOTTOM]);
	XClearWindow(dpy, c->corners[CORNER_TOP_LEFT]);
	XClearWindow(dpy, c->corners[CORNER_TOP_RIGHT]);
	XClearWindow(dpy, c->corners[CORNER_BOTTOM_LEFT]);
	XClearWindow(dpy, c->corners[CORNER_BOTTOM_RIGHT]);

	for(i = 0; i < BUTTON_COUNT; i++)
        {
	    XClearWindow(dpy, c->buttons[i]);
        }

	frameSetShape(c, state, &pm_title, pm_sides, button_x);

        freePixmap(dpy, &pm_title);
        freePixmap(dpy, &pm_sides[SIDE_LEFT]); 
        freePixmap(dpy, &pm_sides[SIDE_RIGHT]); 
        freePixmap(dpy, &pm_sides[SIDE_BOTTOM]); 
    }
    else
    {
        XUnmapWindow(dpy, c->title);
        for(i = 0; i < 3; i++)
	{
            XUnmapWindow(dpy, c->sides[i]);
        }
	for(i = 0; i < 4; i++)
	{
            XUnmapWindow(dpy, c->corners[i]);
        }
        for(i = 0; i < BUTTON_COUNT; i++)
	{
            XUnmapWindow(dpy, c->buttons[i]);
        }
        frameSetShape(c, 0, NULL, NULL, NULL);
    }
}
