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

        sawfish  - (c) 1999 John Harper
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <pango/pango-font.h>
#include <stdio.h>
#include <string.h>
#include "debug.h"

char *states[] = {
    "normal", "active", "prelight", "selected", "insensitive", NULL
};

char *names[] = {
    "fg", "bg", "text", "base", "light", "dark", "mid", NULL
};

#define GTKSTYLE_FG    0
#define GTKSTYLE_BG    1
#define GTKSTYLE_TEXT  2
#define GTKSTYLE_BASE  3
#define GTKSTYLE_LIGHT 4
#define GTKSTYLE_DARK  5
#define GTKSTYLE_MID   6

static gint state_value(gchar * s)
{
    gchar *t;
    gint u;

    t = states[0];
    u = 0;

    while((states[u]) && (strcmp(states[u], s)))
    {
        u++;
    }
    if(states[u])
    {
        return (u);
    }
    return (0);
}

static gint name_value(gchar * s)
{
    gchar *t;
    gint u;

    t = names[0];
    u = 0;

    while((names[u]) && (strcmp(names[u], s)))
    {
        u++;
    }
    if(names[u])
    {
        return (u);
    }
    return (0);
}

static gchar *print_color(GdkColor * c)
{
    gchar *s;

    s = g_new(gchar, 8);
    g_snprintf(s, 8, "#%02x%02x%02x", c->red / 256, c->green / 256, c->blue / 256);
    return (s);
}

static gchar *print_colors(GdkColor * x, int n)
{
    return (print_color(x + n));
}

static gchar *print_rc_style(gchar * name, gchar * state, GtkStyle * style)
{
    gchar *s;
    gint n, m;
    g_return_val_if_fail(state != NULL, NULL);
    g_return_val_if_fail(name != NULL, NULL);

    n = state_value(state);
    m = name_value(name);

    switch (m)
    {
        case GTKSTYLE_FG:
            s = print_colors(style->fg, n);
            break;
        case GTKSTYLE_BG:
            s = print_colors(style->bg, n);
            break;
        case GTKSTYLE_TEXT:
            s = print_colors(style->text, n);
            break;
        case GTKSTYLE_BASE:
            s = print_colors(style->base, n);
            break;
        case GTKSTYLE_LIGHT:
            s = print_colors(style->light, n);
            break;
        case GTKSTYLE_DARK:
            s = print_colors(style->dark, n);
            break;
        default:
        case GTKSTYLE_MID:
            s = print_colors(style->mid, n);
            break;
    }
    return (s);
}

gchar *get_style(GtkWidget * win, gchar * name, gchar * state)
{
    GtkStyle *style = NULL;
    gchar *s = NULL;

    DBG("entering get_style\n");
    g_return_val_if_fail(GTK_WIDGET_REALIZED(win), NULL);

    style = gtk_rc_get_style(win);
    if(!style)
    {
        style = gtk_widget_get_style(win);
    }
    s = print_rc_style(name, state, style);
    DBG("%s[%s]=%s\n", name, state, s);
    return (s);
}

static GdkGC *_get_style_gc(gchar * name, gchar * state, GtkStyle * style)
{
    GdkGC *gc = NULL;
    gint n, m;
    g_return_val_if_fail(state != NULL, NULL);
    g_return_val_if_fail(name != NULL, NULL);

    n = state_value(state);
    m = name_value(name);

    switch (m)
    {
        case GTKSTYLE_FG:
            gc = style->fg_gc[n];
            break;
        case GTKSTYLE_BG:
            gc = style->bg_gc[n];
            break;
        case GTKSTYLE_TEXT:
            gc = style->text_gc[n];
            break;
        case GTKSTYLE_BASE:
            gc = style->base_gc[n];
            break;
        case GTKSTYLE_LIGHT:
            gc = style->light_gc[n];
            break;
        case GTKSTYLE_DARK:
            gc = style->dark_gc[n];
            break;
        default:
        case GTKSTYLE_MID:
            gc = style->mid_gc[n];
            break;
    }
    return (gc);
}

GdkGC *get_style_gc(GtkWidget * win, gchar * name, gchar * state)
{
    GtkStyle *style = NULL;

    DBG("entering get_style_gc\n");
    g_return_val_if_fail(GTK_WIDGET_REALIZED(win), NULL);

    style = gtk_rc_get_style(win);
    if(!style)
    {
        style = gtk_widget_get_style(win);
    }
    return (_get_style_gc(name, state, style));
}

PangoFontDescription *get_font_desc(GtkWidget * win)
{
    DBG("entering get_font_desc\n");
    g_return_val_if_fail(GTK_WIDGET_REALIZED(win), NULL);

    return (win->style->font_desc);
}

PangoContext *pango_get_context(GtkWidget * win)
{
    DBG("entering pango_get_context\n");
    g_return_val_if_fail(GTK_WIDGET_REALIZED(win), NULL);
    return (gtk_widget_get_pango_context(win));
}
