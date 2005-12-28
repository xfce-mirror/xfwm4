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
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2005 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif
#include <libxfce4util/libxfce4util.h> 
#include <libxfcegui4/libxfcegui4.h>
#include <string.h>

#include "misc.h"
#include "workspaces.h"
#include "settings.h"
#include "mywindow.h"
#include "frame.h"
#include "client.h"
#include "stacking.h"
#include "transients.h"
#include "focus.h"
#include "netwm.h"
#include "menu.h"
#include "hints.h"
#include "startup_notification.h"
#include "compositor.h"
#include "events.h"

#ifndef CHECK_BUTTON_TIME
#define CHECK_BUTTON_TIME 0
#endif

#define WIN_IS_BUTTON(win)      ((win == MYWINDOW_XWINDOW(c->buttons[HIDE_BUTTON])) || \
                                 (win == MYWINDOW_XWINDOW(c->buttons[CLOSE_BUTTON])) || \
                                 (win == MYWINDOW_XWINDOW(c->buttons[MAXIMIZE_BUTTON])) || \
                                 (win == MYWINDOW_XWINDOW(c->buttons[SHADE_BUTTON])) || \
                                 (win == MYWINDOW_XWINDOW(c->buttons[STICK_BUTTON])))

#define DBL_CLICK_GRAB          (ButtonMotionMask | \
                                 PointerMotionMask | \
                                 ButtonPressMask | \
                                 ButtonReleaseMask)
                                 
#define MODIFIER_MASK           (ShiftMask | \
                                 ControlMask | \
                                 AltMask | \
                                 MetaMask | \
                                 SuperMask | \
                                 HyperMask)

extern gboolean xfwm4_quit;
extern gboolean xfwm4_reload;

static guint raise_timeout = 0;
static GdkAtom atom_rcfiles = GDK_NONE;
static xfwmWindow menu_event_window;
static int edge_scroll_x = 0;
static int edge_scroll_y = 0;

static void handleEvent (DisplayInfo *display_info, XEvent * ev);
static void menu_callback (Menu * menu, MenuOp op, Window xid,
    gpointer menu_data, gpointer item_data);
static gboolean show_popup_cb (GtkWidget * widget, GdkEventButton * ev,
    gpointer data);
static gboolean client_event_cb (GtkWidget * widget, GdkEventClient * ev, gpointer data);

typedef enum
{
    XFWM_BUTTON_UNDEFINED = 0,
    XFWM_BUTTON_DRAG = 1,
    XFWM_BUTTON_CLICK = 2,
    XFWM_BUTTON_CLICK_AND_DRAG = 3,
    XFWM_BUTTON_DOUBLE_CLICK = 4
}
XfwmButtonClickType;

typedef struct _XfwmButtonClickData XfwmButtonClickData;
struct _XfwmButtonClickData
{
    DisplayInfo *display_info;
    Window w;
    guint button;
    gboolean allow_double_click;
    guint clicks;
    gint x;
    gint y;
    gint xcurrent;
    gint ycurrent;
    guint timeout;
};

static gboolean
typeOfClick_break (gpointer data)
{
    XfwmButtonClickData *passdata = (XfwmButtonClickData *) data;
    if (passdata->timeout)
    {
        g_source_remove (passdata->timeout);
        passdata->timeout = 0;
    }

    gtk_main_quit ();

    return (TRUE);
}

static XfceFilterStatus
typeOfClick_event_filter (XEvent * xevent, gpointer data)
{
    gboolean keep_going = TRUE;
    XfceFilterStatus status = XEV_FILTER_STOP;
    XfwmButtonClickData *passdata = (XfwmButtonClickData *) data;
    
    /* Update the display time */
    myDisplayUpdateCurentTime (passdata->display_info, xevent);

    if ((xevent->type == ButtonRelease) || (xevent->type == ButtonPress))
    {
        if (xevent->xbutton.button == passdata->button)
        {
            passdata->clicks++;
        }
        if (((XfwmButtonClickType) passdata->clicks == XFWM_BUTTON_DOUBLE_CLICK)
            || (!(passdata->allow_double_click) && 
                 (XfwmButtonClickType) passdata->clicks == XFWM_BUTTON_CLICK))
        {
            keep_going = FALSE;
        }
    }
    else if (xevent->type == MotionNotify)
    {
        passdata->xcurrent = xevent->xmotion.x_root;
        passdata->ycurrent = xevent->xmotion.y_root;
    }
    else if ((xevent->type == DestroyNotify) || (xevent->type == UnmapNotify))
    {
        if (xevent->xany.window == passdata->w)
        {
            /* Discard, mark the click as undefined */
            passdata->clicks = (guint) XFWM_BUTTON_UNDEFINED;
            keep_going = FALSE;
        }
        status = XEV_FILTER_CONTINUE;
    }
    else
    {
        status = XEV_FILTER_CONTINUE;
    }

    if ((ABS (passdata->x - passdata->xcurrent) > 1) || 
        (ABS (passdata->y - passdata->ycurrent) > 1) ||
        (!keep_going))
    {
        TRACE ("event loop now finished");
        typeOfClick_break (data);
    }

    return status;
}

static XfwmButtonClickType
typeOfClick (ScreenInfo *screen_info, Window w, XEvent * ev, gboolean allow_double_click)
{
    DisplayInfo *display_info = NULL;
    XfwmButtonClickData passdata;
    gboolean g;

    g_return_val_if_fail (screen_info != NULL, XFWM_BUTTON_UNDEFINED);
    g_return_val_if_fail (ev != NULL, XFWM_BUTTON_UNDEFINED);
    g_return_val_if_fail (w != None, XFWM_BUTTON_UNDEFINED);
    
    display_info = screen_info->display_info;    
    XFlush (display_info->dpy);
    g = myScreenGrabPointer (screen_info, DBL_CLICK_GRAB, None, ev->xbutton.time);

    if (!g)
    {
        TRACE ("grab failed in typeOfClick");
        gdk_beep ();
        myScreenUngrabPointer (screen_info, ev->xbutton.time);
        return XFWM_BUTTON_UNDEFINED;
    }

    passdata.display_info = display_info;
    passdata.button = ev->xbutton.button;
    passdata.w = w;
    passdata.x = ev->xbutton.x_root;
    passdata.y = ev->xbutton.y_root;
    passdata.xcurrent = passdata.x;
    passdata.ycurrent = passdata.y;
    passdata.clicks = 1;
    passdata.allow_double_click = allow_double_click;
    passdata.timeout = g_timeout_add_full (0, display_info->dbl_click_time, 
                                              (GtkFunction) typeOfClick_break, 
                                              (gpointer) &passdata, NULL);

    TRACE ("entering typeOfClick loop");
    xfce_push_event_filter (display_info->xfilter, typeOfClick_event_filter, &passdata);
    gtk_main ();
    xfce_pop_event_filter (display_info->xfilter);
    TRACE ("leaving typeOfClick loop");

    myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));
    XFlush (display_info->dpy);
    return (XfwmButtonClickType) passdata.clicks;
}

#if CHECK_BUTTON_TIME
static gboolean
check_button_time (XButtonEvent *ev)
{
    static Time last_button_time = (Time) 0;
    
    if (last_button_time > ev->time)
    {
        return FALSE;
    }

    last_button_time = ev->time;
    return TRUE;
}
 #endif
 
static void
clear_timeout (void)
{
    if (raise_timeout)
    {
        g_source_remove (raise_timeout);
        raise_timeout = 0;
    }
}

static gboolean
raise_cb (gpointer data)
{
    Client *c = NULL;
    TRACE ("entering raise_cb");

    clear_timeout ();
    c = clientGetFocus ();
    
    if (c)
    {
        clientRaise (c, None);
    }
    return (TRUE);
}

static void
reset_timeout (ScreenInfo *screen_info)
{
    if (raise_timeout)
    {
        g_source_remove (raise_timeout);
    }
    raise_timeout = g_timeout_add_full (0, screen_info->params->raise_delay, (GtkFunction) raise_cb, NULL, NULL);
}

static void
moveRequest (Client * c, XEvent * ev)
{
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MOVE)
        && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        clientMove (c, ev);
    }
}

static void
resizeRequest (Client * c, int corner, XEvent * ev)
{
    if (FLAG_TEST_ALL (c->xfwm_flags,
            XFWM_FLAG_HAS_RESIZE | XFWM_FLAG_IS_RESIZABLE))
    {
        clientResize (c, corner, ev);
    }
    else if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MOVE)
        && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        clientMove (c, ev);
    }
}

static void
toggle_show_desktop (ScreenInfo *screen_info)
{
    long visible = 0;

    getHint (screen_info->display_info, screen_info->xroot, 
             NET_SHOWING_DESKTOP, &visible);
    sendRootMessage (screen_info, NET_SHOWING_DESKTOP, !visible, 
                     myDisplayGetCurrentTime (screen_info->display_info));
}

static void
handleMotionNotify (DisplayInfo *display_info, XMotionEvent * ev)
{
    TRACE ("entering handleMotionNotify");
}

static int
getKeyPressed (ScreenInfo *screen_info, XKeyEvent * ev)
{
    int key, state;
    
    state = ev->state & MODIFIER_MASK;
    for (key = 0; key < KEY_COUNT; key++)
    {
        if ((screen_info->params->keys[key].keycode == ev->keycode)
            && (screen_info->params->keys[key].modifier == state))
        {
            break;
        }
    }
    
    return key;
}

static void
handleKeyPress (DisplayInfo *display_info, XKeyEvent * ev)
{
    ScreenInfo *screen_info = NULL;
    Client *c = NULL;
    int key;

    TRACE ("entering handleKeyEvent");

    c = clientGetFocus ();
    if (c)
    {
        screen_info = c->screen_info;
        key = getKeyPressed (screen_info, ev);
        c->user_time = myDisplayGetCurrentTime (display_info);

        switch (key)
        {
            case KEY_MOVE_UP:
            case KEY_MOVE_DOWN:
            case KEY_MOVE_LEFT:
            case KEY_MOVE_RIGHT:
                moveRequest (c, (XEvent *) ev);
                break;
            case KEY_RESIZE_UP:
            case KEY_RESIZE_DOWN:
            case KEY_RESIZE_LEFT:
            case KEY_RESIZE_RIGHT:
                if (FLAG_TEST_ALL (c->xfwm_flags,
                        XFWM_FLAG_HAS_RESIZE | XFWM_FLAG_IS_RESIZABLE))
                {
                    clientResize (c, CORNER_BOTTOM_RIGHT, (XEvent *) ev);
                }
                break;
            case KEY_CYCLE_WINDOWS:
                clientCycle (c, (XEvent *) ev);
                break;
            case KEY_CLOSE_WINDOW:
                clientClose (c);
                break;
            case KEY_HIDE_WINDOW:
                if (CLIENT_CAN_HIDE_WINDOW (c))
                {
                    clientHide (c, c->win_workspace, TRUE);
                }
                break;
            case KEY_MAXIMIZE_WINDOW:
                clientToggleMaximized (c, WIN_STATE_MAXIMIZED, TRUE);
                break;
            case KEY_MAXIMIZE_VERT:
                clientToggleMaximized (c, WIN_STATE_MAXIMIZED_VERT, TRUE);
                break;
            case KEY_MAXIMIZE_HORIZ:
                clientToggleMaximized (c, WIN_STATE_MAXIMIZED_HORIZ, TRUE);
                break;
            case KEY_SHADE_WINDOW:
                clientToggleShaded (c);
                break;
            case KEY_STICK_WINDOW:
                if (CLIENT_CAN_STICK_WINDOW(c))
                {
                    clientToggleSticky (c, TRUE);
                    frameDraw (c, FALSE, FALSE);
                }
                break;
            case KEY_RAISE_WINDOW:
                clientRaise (c, None);
                break;
            case KEY_LOWER_WINDOW:
                clientLower (c, None);
                break;
            case KEY_TOGGLE_FULLSCREEN:
                clientToggleFullscreen (c);
                break;
            case KEY_MOVE_NEXT_WORKSPACE:
                workspaceSwitch (screen_info, screen_info->current_ws + 1, c, TRUE);
                break;
            case KEY_MOVE_PREV_WORKSPACE:
                workspaceSwitch (screen_info, screen_info->current_ws - 1, c, TRUE);
                break;
            case KEY_MOVE_UP_WORKSPACE:
                workspaceMove (screen_info, -1, 0, c);
                break;
            case KEY_MOVE_DOWN_WORKSPACE:
                workspaceMove (screen_info, 1, 0, c);
                break;
            case KEY_MOVE_LEFT_WORKSPACE:
                workspaceMove (screen_info, 0, -1, c);
                break;
            case KEY_MOVE_RIGHT_WORKSPACE:
                workspaceMove (screen_info, 0, 1, c);
                break;
            case KEY_MOVE_WORKSPACE_1:
            case KEY_MOVE_WORKSPACE_2:
            case KEY_MOVE_WORKSPACE_3:
            case KEY_MOVE_WORKSPACE_4:
            case KEY_MOVE_WORKSPACE_5:
            case KEY_MOVE_WORKSPACE_6:
            case KEY_MOVE_WORKSPACE_7:
            case KEY_MOVE_WORKSPACE_8:
            case KEY_MOVE_WORKSPACE_9:
            case KEY_MOVE_WORKSPACE_10:
            case KEY_MOVE_WORKSPACE_11:
            case KEY_MOVE_WORKSPACE_12:
                if (key - KEY_MOVE_WORKSPACE_1 < screen_info->workspace_count)
                {
                    clientRaise (c, None);
                    workspaceSwitch (screen_info, key - KEY_MOVE_WORKSPACE_1, c, TRUE);
                }
                break;
            default:
                break;
        }
    }
    else
    {
        screen_info = myDisplayGetScreenFromRoot (display_info, ev->root);
        if (!screen_info)
        {
            return;
        }
        
        key = getKeyPressed (screen_info, ev);
        switch (key)
        {
            case KEY_CYCLE_WINDOWS:
                if (screen_info->clients)
                {
                    clientCycle (screen_info->clients->prev, (XEvent *) ev);
                }
                break;
            default:
                break;
        }
    }
    /* 
       Here we know that "screen_info" is defined, otherwise, we would 
       already have returned...
     */
    switch (key)
    {
        case KEY_NEXT_WORKSPACE:
            workspaceSwitch (screen_info, screen_info->current_ws + 1, NULL, TRUE);
            break;
        case KEY_PREV_WORKSPACE:
            workspaceSwitch (screen_info, screen_info->current_ws - 1, NULL, TRUE);
            break;
        case KEY_UP_WORKSPACE:
            workspaceMove(screen_info, -1, 0, NULL);
            break;
        case KEY_DOWN_WORKSPACE:
            workspaceMove(screen_info, 1, 0, NULL);
            break;
        case KEY_LEFT_WORKSPACE:
            workspaceMove(screen_info, 0, -1, NULL);
            break;
        case KEY_RIGHT_WORKSPACE:
            workspaceMove(screen_info, 0, 1, NULL);
            break;
        case KEY_ADD_WORKSPACE:
            workspaceSetCount (screen_info, screen_info->workspace_count + 1);
            break;
        case KEY_DEL_WORKSPACE:
            workspaceSetCount (screen_info, screen_info->workspace_count - 1);
            break;
        case KEY_WORKSPACE_1:
        case KEY_WORKSPACE_2:
        case KEY_WORKSPACE_3:
        case KEY_WORKSPACE_4:
        case KEY_WORKSPACE_5:
        case KEY_WORKSPACE_6:
        case KEY_WORKSPACE_7:
        case KEY_WORKSPACE_8:
        case KEY_WORKSPACE_9:
        case KEY_WORKSPACE_10:
        case KEY_WORKSPACE_11:
        case KEY_WORKSPACE_12:
            if (key - KEY_WORKSPACE_1 < screen_info->workspace_count)
            {
                workspaceSwitch (screen_info, key - KEY_WORKSPACE_1, NULL, TRUE);
            }
            break;
        case KEY_SHOW_DESKTOP:
            toggle_show_desktop (screen_info);
            break;
        default:
            break;
    }
}

/* User has clicked on an edge or corner.
 * Button 1 : Raise and resize
 * Button 2 : Move
 * Button 3 : Resize
 */
static void
edgeButton (Client * c, int part, XButtonEvent * ev)
{
    if (ev->button == Button2)
    {
        XfwmButtonClickType tclick;
        ScreenInfo *screen_info = c->screen_info;

        tclick = typeOfClick (screen_info, c->window, (XEvent *) ev, FALSE);
        if (tclick == XFWM_BUTTON_CLICK)
        {
            clientLower (c, None);
        }
        else if (tclick != XFWM_BUTTON_UNDEFINED)
        {
            moveRequest (c, (XEvent *) ev);
        }
    }
    else
    {
        if (ev->button == Button1)
        {
            if (!(c->type & WINDOW_TYPE_DONT_FOCUS))
            {
                clientSetFocus (c->screen_info, c, ev->time, NO_FOCUS_FLAG);
            }
            clientRaise (c, None);
        }
        if ((ev->button == Button1) || (ev->button == Button3))
        {
            resizeRequest (c, part, (XEvent *) ev);
        }
    }
}

static void
button1Action (Client * c, XButtonEvent * ev)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;
    XEvent copy_event;
    XfwmButtonClickType tclick;

    g_return_if_fail (c != NULL);
    g_return_if_fail (ev != NULL);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (!(c->type & WINDOW_TYPE_DONT_FOCUS))
    {
        clientSetFocus (screen_info, c, ev->time, NO_FOCUS_FLAG);
    }
    clientRaise (c, None);

    memcpy(&copy_event, ev, sizeof(XEvent));
    tclick = typeOfClick (screen_info, c->window, &copy_event, TRUE);

    if ((tclick == XFWM_BUTTON_DRAG)
        || (tclick == XFWM_BUTTON_CLICK_AND_DRAG))
    {
        moveRequest (c, (XEvent *) ev);
    }
    else if (tclick == XFWM_BUTTON_DOUBLE_CLICK)
    {
        switch (screen_info->params->double_click_action)
        {
            case ACTION_MAXIMIZE:
                clientToggleMaximized (c, WIN_STATE_MAXIMIZED, TRUE);
                break;
            case ACTION_SHADE:
                clientToggleShaded (c);
                break;
            case ACTION_HIDE:
                if (CLIENT_CAN_HIDE_WINDOW (c))
                {
                    clientHide (c, c->win_workspace, TRUE);
                }
                break;
        }
    }
}

static void
titleButton (Client * c, int state, XButtonEvent * ev)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;

    g_return_if_fail (c != NULL);
    g_return_if_fail (ev != NULL);

    /* Get Screen data from the client itself */
    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (ev->button == Button1)
    {
        button1Action (c, ev);
    }
    else if (ev->button == Button2)
    {
        clientLower (c, None);
    }
    else if (ev->button == Button3)
    {
        /*
           We need to copy the event to keep the original event untouched
           for gtk to handle it (in case we open up the menu)
         */

        XEvent copy_event;
        XfwmButtonClickType tclick;

        memcpy(&copy_event, ev, sizeof(XEvent));
        tclick = typeOfClick (screen_info, c->window, &copy_event, FALSE);

        if (tclick == XFWM_BUTTON_DRAG)
        {
            moveRequest (c, (XEvent *) ev);
        }
        else if (tclick != XFWM_BUTTON_UNDEFINED)
        {
            if (!(c->type & WINDOW_TYPE_DONT_FOCUS))
            {
                clientSetFocus (screen_info, c, ev->time, NO_FOCUS_FLAG);
            }
            if (screen_info->params->raise_on_click)
            {
                clientRaise (c, None);
            }
            ev->window = ev->root;
            if (screen_info->button_handler_id)
            {
                g_signal_handler_disconnect (GTK_OBJECT (myScreenGetGtkWidget (screen_info)), screen_info->button_handler_id);
            }
            screen_info->button_handler_id = g_signal_connect (GTK_OBJECT (myScreenGetGtkWidget (screen_info)),
                                                      "button_press_event", GTK_SIGNAL_FUNC (show_popup_cb), (gpointer) c);
            /* Let GTK handle this for us. */
        }
    }
    else if (ev->button == Button4)
    {

        /* Mouse wheel scroll up */

        if (state == AltMask)
        {
            clientIncOpacity(c);
        }
        else if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            clientShade (c);
        }
    }
    else if (ev->button == Button5)
    {
        /* Mouse wheel scroll down */

        if (state == AltMask)
        {
            clientDecOpacity(c);
        }
        else if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            clientUnshade (c);
        }
    }
}

static void
rootScrollButton (DisplayInfo *display_info, XButtonEvent * ev)
{
    static Time lastscroll = (Time) 0;
    ScreenInfo *screen_info = NULL;

    if ((ev->time - lastscroll) < 25)  /* ms */
    {
        /* Too many events in too little time, drop this event... */
        return;
    }
    lastscroll = ev->time;
    
    /* Get the screen structure from the root of the event */
    screen_info = myDisplayGetScreenFromRoot (display_info, ev->root);
    if (!screen_info)
    {
        return;
    }

    if (ev->button == Button4)
    {
        workspaceSwitch (screen_info, screen_info->current_ws - 1, NULL, TRUE);
    }
    else if (ev->button == Button5)
    {
        workspaceSwitch (screen_info, screen_info->current_ws + 1, NULL, TRUE);
    }
}


static void
handleButtonPress (DisplayInfo *display_info, XButtonEvent * ev)
{
    ScreenInfo *screen_info = NULL;
    Client *c = NULL;
    Window win;
    int state, replay = FALSE;

    TRACE ("entering handleButtonPress");

#if CHECK_BUTTON_TIME
    /* Avoid treating the same event twice */
    if (!check_button_time (ev))
    {
        TRACE ("ignoring ButtonPress event because it has been already handled");
        return;
    }
#endif
     
    c = myDisplayGetClientFromWindow (display_info, ev->window, ANY);
    if (c)
    {
        state = ev->state & MODIFIER_MASK;
        win = ev->subwindow;
        screen_info = c->screen_info;
        c->user_time = myDisplayGetCurrentTime (display_info);

        if ((ev->button == Button1) && (state == AltMask) && (screen_info->params->easy_click))
        {
            button1Action (c, ev);
        }
        else if ((ev->button == Button2) && (state == AltMask) && (screen_info->params->easy_click))
        {
            clientLower (c, None);
        }
        else if ((ev->button == Button3) && (state == AltMask) && (screen_info->params->easy_click))
        {
            int part, x_corner_pixels, y_corner_pixels, x_distance, y_distance;

            /* Corner is 1/3 of the side */
            x_corner_pixels = MAX(c->width / 3, 50);
            y_corner_pixels = MAX(c->height / 3, 50);

            /* Distance from event to edge of client window */
            x_distance = c->width / 2 - abs(c->width / 2 - ev->x);
            y_distance = c->height / 2 - abs(c->height / 2 - ev->y);

            if (x_distance < x_corner_pixels && y_distance < y_corner_pixels)
            {
                /* In a corner */
                if (ev->x < c->width / 2)
                {
                    if (ev->y < c->height / 2)
                    {
                        part = CORNER_TOP_LEFT;
                    }
                    else
                    {
                        part = CORNER_BOTTOM_LEFT;
                    }
                }
                else
                {
                    if (ev->y < c->height / 2)
                    {
                        part = CORNER_TOP_RIGHT;
                    }
                    else
                    {
                        part = CORNER_BOTTOM_RIGHT;
                    }
                }
            }
            else
            {
                /* Not a corner - some side */
                if (x_distance / x_corner_pixels < y_distance / y_corner_pixels)
                {
                    /* Left or right side */
                    if (ev->x < c->width / 2)
                    {
                        part = 4 + SIDE_LEFT;
                    }
                    else
                    {
                        part = 4 + SIDE_RIGHT;
                    }
                }
                else
                {
                    /* Top or bottom side */
                    if (ev->y < c->height / 2)
                    {
                        part = 4 + SIDE_TOP;
                    }
                    else
                    {
                        part = 4 + SIDE_BOTTOM;
                    }
                }
            }

            edgeButton (c, part, ev);
        }
        else if (WIN_IS_BUTTON (win))
        {
            if (ev->button <= Button3)
            {
                if (!(c->type & WINDOW_TYPE_DONT_FOCUS))
                {
                    clientSetFocus (screen_info, c, ev->time, NO_FOCUS_FLAG);
                }
                if (screen_info->params->raise_on_click)
                {
                    /* Clear timeout */
                    clear_timeout ();
                    clientRaise (c, None);
                }
                clientButtonPress (c, win, ev);
            }
        }
        else if (win == MYWINDOW_XWINDOW (c->title))
        {
            titleButton (c, state, ev);
        }
        else if (win == MYWINDOW_XWINDOW (c->buttons[MENU_BUTTON]))
        {
            if (ev->button == Button1)
            {
                /*
                   We need to copy the event to keep the original event untouched
                   for gtk to handle it (in case we open up the menu)
                 */

                XEvent copy_event;
                XfwmButtonClickType tclick;

                memcpy(&copy_event, ev, sizeof(XEvent));
                tclick = typeOfClick (screen_info, c->window, &copy_event, TRUE);

                if (tclick == XFWM_BUTTON_DOUBLE_CLICK)
                {
                    clientClose (c);
                }
                else if (tclick != XFWM_BUTTON_UNDEFINED)
                {
                    if (!(c->type & WINDOW_TYPE_DONT_FOCUS))
                    {
                        clientSetFocus (screen_info, c, ev->time, NO_FOCUS_FLAG);
                    }
                    if (screen_info->params->raise_on_click)
                    {
                        /* Clear timeout */
                        clear_timeout ();
                        clientRaise (c, None);
                    }
                    ev->window = ev->root;
                    if (screen_info->button_handler_id)
                    {
                        g_signal_handler_disconnect (GTK_OBJECT (myScreenGetGtkWidget (screen_info)), screen_info->button_handler_id);
                    }
                    screen_info->button_handler_id = g_signal_connect (GTK_OBJECT (myScreenGetGtkWidget (screen_info)),
                                                              "button_press_event", GTK_SIGNAL_FUNC (show_popup_cb), (gpointer) c);
                    /* Let GTK handle this for us. */
                }
            }
        }
        else if ((win == MYWINDOW_XWINDOW (c->corners[CORNER_TOP_LEFT]))
            && (state == 0))
        {
            edgeButton (c, CORNER_TOP_LEFT, ev);
        }
        else if ((win == MYWINDOW_XWINDOW (c->corners[CORNER_TOP_RIGHT]))
            && (state == 0))
        {
            edgeButton (c, CORNER_TOP_RIGHT, ev);
        }
        else if ((win == MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_LEFT]))
            && (state == 0))
        {
            edgeButton (c, CORNER_BOTTOM_LEFT, ev);
        }
        else if ((win == MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_RIGHT]))
            && (state == 0))
        {
            edgeButton (c, CORNER_BOTTOM_RIGHT, ev);
        }
        else if ((win == MYWINDOW_XWINDOW (c->sides[SIDE_BOTTOM]))
            && (state == 0))
        {
            edgeButton (c, 4 + SIDE_BOTTOM, ev);
        }
        else if ((win == MYWINDOW_XWINDOW (c->sides[SIDE_LEFT]))
            && (state == 0))
        {
            edgeButton (c, 4 + SIDE_LEFT, ev);
        }
        else if ((win == MYWINDOW_XWINDOW (c->sides[SIDE_RIGHT]))
            && (state == 0))
        {
            edgeButton (c, 4 + SIDE_RIGHT, ev);
        }
        else if (ev->window == c->window)
        {
            clientPassGrabMouseButton (c);
            if ((screen_info->params->raise_with_any_button) || (ev->button == Button1))
            {
                if (!(c->type & WINDOW_TYPE_DONT_FOCUS))
                {
                    clientSetFocus (screen_info, c, ev->time, NO_FOCUS_FLAG);
                }
                if ((screen_info->params->raise_on_click) || 
                    !FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER))
                {
                    /* Clear timeout */
                    clear_timeout ();
                    clientRaise (c, None);
                }
            }
            replay = TRUE;
        }

        if (replay)
        {
            XAllowEvents (display_info->dpy, ReplayPointer, ev->time);
        }
        else
        {
            XAllowEvents (display_info->dpy, SyncPointer, ev->time);
        }

        return;
    }

    /* 
       The event did not occur in one of our known good client...
       Get the screen structure from the root of the event.
     */
    screen_info = myDisplayGetScreenFromRoot (display_info, ev->root);
    if (!screen_info)
    {
        return;
    }
    
    if ((ev->window == screen_info->xroot) && (screen_info->params->scroll_workspaces)
            && ((ev->button == Button4) || (ev->button == Button5)))
    {
        rootScrollButton (display_info, ev);
    }
    else
    {
        XUngrabPointer (display_info->dpy, CurrentTime);
        XSendEvent (display_info->dpy, screen_info->xfwm4_win, FALSE, SubstructureNotifyMask, (XEvent *) ev);
    }
}

static void
handleButtonRelease (DisplayInfo *display_info, XButtonEvent * ev)
{
    ScreenInfo *screen_info = NULL;
    TRACE ("entering handleButtonRelease");

#if CHECK_BUTTON_TIME
    /* Avoid treating the same event twice */
    if (!check_button_time (ev))
    {
        TRACE ("ignoring ButtonRelease event because it has been already handled");
        return;
    }
#endif

    /* Get the screen structure from the root of the event */
    screen_info = myDisplayGetScreenFromRoot (display_info, ev->root);
    if (!screen_info)
    {
        return;
    }

    XSendEvent (display_info->dpy, screen_info->xfwm4_win, FALSE, SubstructureNotifyMask, (XEvent *) ev);
}

static void
handleDestroyNotify (DisplayInfo *display_info, XDestroyWindowEvent * ev)
{
    Client *c = NULL;
    ScreenInfo *screen_info = NULL;

    TRACE ("entering handleDestroyNotify");
    TRACE ("DestroyNotify on window (0x%lx)", ev->window);

    screen_info = myDisplayGetScreenFromSystray (display_info, ev->window);
    if  (screen_info)
    {
        /* systray window is gone */
        screen_info->systray = None;
        return;
    }
    
    c = myDisplayGetClientFromWindow (display_info, ev->window, WINDOW);
    if (c)
    {
        TRACE ("DestroyNotify for \"%s\" (0x%lx)", c->name, c->window);
        clientPassFocus (c->screen_info, c, c);
        clientUnframe (c, FALSE);
    }
}

static void
handleMapRequest (DisplayInfo *display_info, XMapRequestEvent * ev)
{
    Client *c = NULL;

    TRACE ("entering handleMapRequest");
    TRACE ("MapRequest on window (0x%lx)", ev->window);

    if (ev->window == None)
    {
        TRACE ("Mapping None ???");
        return;
    }

    c = myDisplayGetClientFromWindow (display_info, ev->window, WINDOW);
    if (c)
    {
        ScreenInfo *screen_info = c->screen_info;

        TRACE ("handleMapRequest: clientShow");

        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MAP_PENDING))
        {
            TRACE ("Ignoring MapRequest on window (0x%lx)", ev->window);
            return;
        }
        clientShow (c, TRUE);
        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY) ||
            (c->win_workspace == screen_info->current_ws))
        {
            clientFocusNew(c);
        }
    }
    else
    {
        TRACE ("handleMapRequest: clientFrame");
        clientFrame (display_info, ev->window, FALSE);
    }
}

static void
handleMapNotify (DisplayInfo *display_info, XMapEvent * ev)
{
    Client *c = NULL;

    TRACE ("entering handleMapNotify");
    TRACE ("MapNotify on window (0x%lx)", ev->window);

    c = myDisplayGetClientFromWindow (display_info, ev->window, WINDOW);
    if (c)
    {
        TRACE ("MapNotify for \"%s\" (0x%lx)", c->name, c->window);
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MAP_PENDING))
        {
            FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_MAP_PENDING);
        }
        compositorMapWindow (display_info, c->frame);
    }
    else if (myDisplayGetScreenFromRoot (display_info, ev->event))
    {
        compositorMapWindow (display_info, ev->window);
    }
}

static void
handleUnmapNotify (DisplayInfo *display_info, XUnmapEvent * ev)
{
    ScreenInfo *screen_info = NULL;
    Client *c = NULL;

    TRACE ("entering handleUnmapNotify");
    TRACE ("UnmapNotify on window (0x%lx)", ev->window);
    
    if (ev->from_configure)
    {
        TRACE ("Ignoring UnmapNotify caused by parent's resize");
        return;
    }

    screen_info = myDisplayGetScreenFromWindow (display_info, ev->window);
    if (screen_info && (ev->event != ev->window) && (ev->event != screen_info->xroot || !ev->send_event))
    {
        TRACE ("handleUnmapNotify (): Event ignored");
        return;
    }

    c = myDisplayGetClientFromWindow (display_info, ev->window, WINDOW);
    if (c)
    {
        TRACE ("UnmapNotify for \"%s\" (0x%lx)", c->name, c->window);
        TRACE ("ignore_unmap for \"%s\" is %i", c->name, c->ignore_unmap);

        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MAP_PENDING))
        {
            /* 
             * This UnmapNotify event is caused by reparenting
             * so we just ignore it, so the window won't return 
             * to withdrawn state by mistake.
             */
            TRACE ("Client \"%s\" is not mapped, event ignored", c->name);
            return;
        }

        screen_info = c->screen_info;
        clientPassFocus (screen_info, c, c);
        compositorUnmapWindow (display_info, c->frame);
        
        /*
         * ICCCM spec states that a client wishing to switch
         * to WithdrawnState should send a synthetic UnmapNotify 
         * with the event field set to root if the client window 
         * is already unmapped.
         * Therefore, bypass the ignore_unmap counter and
         * unframe the client.
         */
        if ((ev->event == screen_info->xroot) && (ev->send_event))
        {
            TRACE ("ICCCM UnmapNotify for \"%s\"", c->name);
            clientUnframe (c, FALSE);
            return;
        }
        if (c->ignore_unmap)
        {
            c->ignore_unmap--;
            TRACE ("ignore_unmap for \"%s\" is now %i", 
                 c->name, c->ignore_unmap);
        }
        else
        {
            TRACE ("unmapping \"%s\" as ignore_unmap is %i", 
                 c->name, c->ignore_unmap);
            clientUnframe (c, FALSE);
        }
    }
    else
    {
        compositorUnmapWindow (display_info, ev->window);
    }
}

static void
handleConfigureNotify (DisplayInfo *display_info, XConfigureEvent * ev)
{
    ScreenInfo *screen_info = NULL;

    TRACE ("entering handleConfigureNotify");

    screen_info = myDisplayGetScreenFromRoot (display_info, ev->window);
    if (!screen_info)
    {
        return;
    }
    
    if (display_info->have_xrandr)
    {
#ifdef HAVE_RANDR
        XRRUpdateConfiguration ((XEvent *) ev);
#endif
    }
    else
    {
        TRACE ("ConfigureNotify on the screen_info->xroot win (0x%lx)", ev->window);
        screen_info->xscreen->width   = ev->width;
        screen_info->xscreen->height  = ev->height;
    }
    setNetWorkarea (display_info, screen_info->xroot, screen_info->workspace_count, 
                    gdk_screen_get_width (screen_info->gscr),
                    gdk_screen_get_height (screen_info->gscr),
                    screen_info->margins);
    placeSidewalks (screen_info, screen_info->params->wrap_workspaces);
    clientScreenResize (screen_info);
}

static void
handleConfigureRequest (DisplayInfo *display_info, XConfigureRequestEvent * ev)
{
    Client *c = NULL;
    XWindowChanges wc;
    XEvent otherEvent;

    TRACE ("entering handleConfigureRequest");
    TRACE ("ConfigureRequest on window (0x%lx)", ev->window);

    /* Compress events - logic taken from kwin */
    while (XCheckTypedWindowEvent (display_info->dpy, ev->window, ConfigureRequest, &otherEvent))
    {
        /* Update the display time */
        myDisplayUpdateCurentTime (display_info, &otherEvent);

        if (otherEvent.xconfigurerequest.value_mask == ev->value_mask)
        {
            ev = &otherEvent.xconfigurerequest;
        }
        else
        {
            XPutBackEvent (display_info->dpy, &otherEvent);
            break;
        }
    }

    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    wc.border_width = ev->border_width;

    c = myDisplayGetClientFromWindow (display_info, ev->window, WINDOW);
    if (!c)
    {
        /* Some app tend or try to manipulate the wm frame to achieve fullscreen mode */
        c = myDisplayGetClientFromWindow (display_info, ev->window, FRAME);
        if (c)
        {
            TRACE ("client %s (0x%lx) is attempting to manipulate its frame!", c->name, c->window);
            if (ev->value_mask & CWX)
            {
                wc.x += frameLeft (c);
            }
            if (ev->value_mask & CWY)
            {
                wc.y += frameTop (c);
            }
            if (ev->value_mask & CWWidth)
            {
                wc.width -= frameLeft (c) + frameRight (c);
            }
            if (ev->value_mask & CWHeight)
            {
                wc.height -= frameTop (c) + frameBottom (c);
            }
            /* We don't allow changing stacking order by accessing the frame
               window because that would break the layer management in xfwm4
             */
            ev->value_mask &= ~(CWSibling | CWStackMode);
        }
    }
    if (c)
    {
        gboolean constrained = FALSE;
        ScreenInfo *screen_info = c->screen_info;

        TRACE ("handleConfigureRequest managed window \"%s\" (0x%lx)", c->name, c->window);
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING))
        {
            /* Sorry, but it's not the right time for configure request */
            return;
        }
        if (c->type == WINDOW_DESKTOP)
        {
            /* Ignore stacking request for DESKTOP windows */
            ev->value_mask &= ~(CWSibling | CWStackMode);
        }
        clientCoordGravitate (c, APPLY, &wc.x, &wc.y);
        if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
        {
            GdkRectangle rect;
            gint monitor_nbr;
            int cx, cy;

            /* size request from fullscreen windows get fullscreen */

            cx = frameX (c) + (frameWidth (c) / 2);
            cy = frameY (c) + (frameHeight (c) / 2);

            monitor_nbr = find_monitor_at_point (screen_info->gscr, cx, cy);
            gdk_screen_get_monitor_geometry (screen_info->gscr, monitor_nbr, &rect);

            wc.x = rect.x;
            wc.y = rect.y;
            wc.width = rect.width;
            wc.height = rect.height;

            ev->value_mask |= (CWX | CWY | CWWidth | CWHeight);
        }
        /* Clean up buggy requests that set all flags */
        if ((ev->value_mask & CWX) && (wc.x == c->x))
        {
            ev->value_mask &= ~CWX;
        }
        if ((ev->value_mask & CWY) && (wc.y == c->y))
        {
            ev->value_mask &= ~CWY;
        }
        if ((ev->value_mask & CWWidth) && (wc.width == c->width))
        {
            ev->value_mask &= ~CWWidth;
        }
        if ((ev->value_mask & CWHeight) && (wc.height == c->height))
        {
            ev->value_mask &= ~CWHeight;
        }
        /* Still a move/resize after cleanup? */
        if (ev->value_mask & (CWX | CWY | CWWidth | CWHeight))
        {
            if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
            {
                clientRemoveMaximizeFlag (c);
            }
            constrained = TRUE;
        }
        if (ev->value_mask & CWStackMode)
        {
            clientPassGrabMouseButton (NULL);
        }
#if 0
        /* Let's say that if the client performs a XRaiseWindow, we show the window if hidden */
        if ((ev->value_mask & CWStackMode) && (wc.stack_mode == Above))
        {
            if ((c->win_workspace == screen_info->current_ws) || 
                (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY)))
            {
                if (FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
                {
                    clientShow (c, TRUE);
                }
            }
        }
#endif
        clientConfigure (c, &wc, ev->value_mask, (constrained ? CFG_CONSTRAINED : 0) | CFG_REQUEST);
    }
    else
    {
        TRACE ("unmanaged configure request for win 0x%lx", ev->window);
        XConfigureWindow (display_info->dpy, ev->window, ev->value_mask, &wc);
    }
}

static void
handleEnterNotify (DisplayInfo *display_info, XCrossingEvent * ev)
{
    static Time lastresist = (Time) 0;
    Client *c = NULL;
    ScreenInfo *screen_info = NULL;
    gboolean warp_pointer = FALSE;

    TRACE ("entering handleEnterNotify");

    if ((ev->mode == NotifyGrab) || (ev->mode == NotifyUngrab)
        || (ev->detail > NotifyNonlinearVirtual))
    {
        /* We're not interested in such notifications */
        return;
    }

    TRACE ("EnterNotify on window (0x%lx)", ev->window);

    c = myDisplayGetClientFromWindow (display_info, ev->window, FRAME);
    if (c)
    {
        screen_info = c->screen_info;
        
        if (!(screen_info->params->click_to_focus) && clientAcceptFocus (c))
        {
            TRACE ("EnterNotify window is \"%s\"", c->name);
            if (!(c->type & (WINDOW_DOCK | WINDOW_DESKTOP)))
            {
                clientSetFocus (c->screen_info, c, ev->time, NO_FOCUS_FLAG);
            }
        }
        
        /* No need to process the event any further */
        return;
    }
    
    /* The event was not for a client window */

    if (display_info->nb_screens > 1)
    {
        /* Wrap workspace/wrap windows is disabled with multiscreen */
        return;
    }
    
    /* Get the screen structure from the root of the event */
    screen_info = myDisplayGetScreenFromRoot (display_info, ev->root);

    if (!screen_info)
    {
        return;
    }
    
    if (screen_info->workspace_count && screen_info->params->wrap_workspaces
        && screen_info->params->wrap_resistance)
    {
        int msx, msy, maxx, maxy;
        int rx, ry;

        msx = ev->x_root;
        msy = ev->y_root;
        maxx = gdk_screen_get_width (screen_info->gscr) - 1;
        maxy = gdk_screen_get_height (screen_info->gscr) - 1;
        rx = 0;
        ry = 0;
        warp_pointer = FALSE;

        if ((msx == 0) || (msx == maxx))
        {
            if ((ev->time - lastresist) > 250)  /* ms */
            {
                edge_scroll_x = 0;
            }
            else
            {
                edge_scroll_x++;
            }
            if (msx == 0) 
            {
                rx = 1;
            }
            else
            {
                rx = -1;
            }
            warp_pointer = TRUE;
            lastresist = ev->time;
        }
        if ((msy == 0) || (msy == maxy))
        {
            if ((ev->time - lastresist) > 250)  /* ms */
            {
                edge_scroll_y = 0;
            }
            else
            {
                edge_scroll_y++;
            }
            if (msy == 0) 
            {
                ry = 1;
            }
            else
            {
                ry = -1;
            }
            warp_pointer = TRUE;
            lastresist = ev->time;
        }

        if (edge_scroll_x > screen_info->params->wrap_resistance)
        {
            edge_scroll_x = 0;
            if (msx == 0)
            {
                if (workspaceMove (screen_info, 0, -1, NULL))
                {
                    rx = 4 * maxx / 5;
                }
            }
            else
            {
                if (workspaceMove (screen_info, 0, 1, NULL))
                {
                    rx = -4 * maxx / 5;
                }
            }
            warp_pointer = TRUE;
        }
        if (edge_scroll_y > screen_info->params->wrap_resistance)
        {
            edge_scroll_y = 0;
            if (msy == 0)
            {
                if (workspaceMove (screen_info, -1, 0, NULL))
                {
                    ry = 4 * maxy / 5;
                }
            }
            else
            {
                if (workspaceMove (screen_info, 1, 0, NULL))
                {
                    ry = -4 * maxy / 5;
                }
            }
            warp_pointer = TRUE;
        }
        if (warp_pointer)
        {
            XWarpPointer (display_info->dpy, None, None, 0, 0, 0, 0, rx, ry);
            XFlush (display_info->dpy);
        }
    }
}

static void
handleLeaveNotify (DisplayInfo *display_info, XCrossingEvent * ev)
{
    TRACE ("entering handleLeaveNotify");
}

static void
handleFocusIn (DisplayInfo *display_info, XFocusChangeEvent * ev)
{
    ScreenInfo *screen_info = NULL;
    Client *c = NULL;
    Client *last_raised = NULL;
        

    TRACE ("entering handleFocusIn");
    TRACE ("handleFocusIn (0x%lx) mode = %s",
                ev->window,
                (ev->mode == NotifyNormal) ?
                "NotifyNormal" :
                (ev->mode == NotifyWhileGrabbed) ?
                "NotifyWhileGrabbed" :
                "(unknown)");
    TRACE ("handleFocusIn (0x%lx) detail = %s",
                ev->window,
                (ev->detail == NotifyAncestor) ?
                "NotifyAncestor" :
                (ev->detail == NotifyVirtual) ?
                "NotifyVirtual" :
                (ev->detail == NotifyInferior) ?
                "NotifyInferior" :
                (ev->detail == NotifyNonlinear) ?
                "NotifyNonlinear" :
                (ev->detail == NotifyNonlinearVirtual) ?
                "NotifyNonlinearVirtual" :
                (ev->detail == NotifyPointer) ?
                "NotifyPointer" :
                (ev->detail == NotifyPointerRoot) ?
                "NotifyPointerRoot" :
                (ev->detail == NotifyDetailNone) ?
                "NotifyDetailNone" :
                "(unknown)");

    screen_info = myDisplayGetScreenFromWindow (display_info, ev->window);
    if (screen_info && (ev->window == screen_info->xroot) && (ev->mode == NotifyNormal) && 
        (ev->detail == NotifyDetailNone))
    {
        /* Handle focus transition to root (means that an unknown
           window has vanished and the focus is returned to the root
         */
        c = clientGetFocus ();
        if (c)
        {
            clientSetFocus (c->screen_info, c, myDisplayGetCurrentTime (display_info), FOCUS_FORCE);
        }
        return;
    }
    
    if ((ev->mode == NotifyGrab) || (ev->mode == NotifyUngrab) ||
             (ev->detail > NotifyNonlinearVirtual))
    {
        /* We're not interested in such notifications */
        return;
    }

    c = myDisplayGetClientFromWindow (display_info, ev->window, WINDOW);
    TRACE ("FocusIn on window (0x%lx)", ev->window);
    if (c)
    {
        TRACE ("focus set to \"%s\" (0x%lx)", c->name, c->window);
        screen_info = c->screen_info;
        clientUpdateFocus (screen_info, c, FOCUS_SORT);
        last_raised = clientGetLastRaise (screen_info);
        if ((screen_info->params->click_to_focus) && 
            (screen_info->params->raise_on_click) && 
            (last_raised != NULL) && (c != last_raised))
        {
            clientRaise (c, None);
        }
        if (screen_info->params->raise_on_focus)
        {
            reset_timeout (screen_info);
        }
    }
}

static void
handleFocusOut (DisplayInfo *display_info, XFocusChangeEvent * ev)
{
    Client *c = NULL;
    
    TRACE ("entering handleFocusOut");
    TRACE ("handleFocusOut (0x%lx) mode = %s",
                ev->window,
                (ev->mode == NotifyNormal) ?
                "NotifyNormal" :
                (ev->mode == NotifyWhileGrabbed) ?
                "NotifyWhileGrabbed" :
                "(unknown)");
    TRACE ("handleFocusOut (0x%lx) detail = %s",
                ev->window,
                (ev->detail == NotifyAncestor) ?
                "NotifyAncestor" :
                (ev->detail == NotifyVirtual) ?
                "NotifyVirtual" :
                (ev->detail == NotifyInferior) ?
                "NotifyInferior" :
                (ev->detail == NotifyNonlinear) ?
                "NotifyNonlinear" :
                (ev->detail == NotifyNonlinearVirtual) ?
                "NotifyNonlinearVirtual" :
                (ev->detail == NotifyPointer) ?
                "NotifyPointer" :
                (ev->detail == NotifyPointerRoot) ?
                "NotifyPointerRoot" :
                (ev->detail == NotifyDetailNone) ?
                "NotifyDetailNone" :
                "(unknown)");
    if ((ev->mode == NotifyNormal)
        && ((ev->detail == NotifyNonlinear) 
            || (ev->detail == NotifyNonlinearVirtual)))
    {
        c = myDisplayGetClientFromWindow (display_info, ev->window, WINDOW);
        TRACE ("FocusOut on window (0x%lx)", ev->window);
        if ((c) && (c == clientGetFocus ()))
        {
            TRACE ("focus lost from \"%s\" (0x%lx)", c->name, c->window);
            clientPassGrabMouseButton (NULL);
            clientUpdateFocus (c->screen_info, NULL, NO_FOCUS_FLAG);
            /* Clear timeout */
            clear_timeout ();
        }
    }
}

static void
handlePropertyNotify (DisplayInfo *display_info, XPropertyEvent * ev)
{
    ScreenInfo *screen_info = NULL;
    Client *c = NULL;
    Window w = None;
    char *names;
    int length;

    TRACE ("entering handlePropertyNotify");

    c = myDisplayGetClientFromWindow (display_info, ev->window, WINDOW);
    if (c)
    {
        screen_info = c->screen_info;
        if (ev->atom == XA_WM_NORMAL_HINTS)
        {
            TRACE ("client \"%s\" (0x%lx) has received a XA_WM_NORMAL_HINTS notify", c->name, c->window);
            clientGetWMNormalHints (c, TRUE);
        }
        else if ((ev->atom == XA_WM_NAME) || (ev->atom == display_info->atoms[NET_WM_NAME]))
        {
            TRACE ("client \"%s\" (0x%lx) has received a XA_WM_NAME notify", c->name, c->window);
            if (c->name)
            {
                free (c->name);
            }
            getWindowName (display_info, c->window, &c->name);
            FLAG_SET (c->flags, CLIENT_FLAG_NAME_CHANGED);
            frameDraw (c, TRUE, FALSE);
        }
        else if (ev->atom == display_info->atoms[MOTIF_WM_HINTS])
        {
            TRACE ("client \"%s\" (0x%lx) has received a motif_wm_hints notify", c->name, c->window);
            clientGetMWMHints (c, TRUE);
        }
        else if (ev->atom == XA_WM_HINTS)
        {
            TRACE ("client \"%s\" (0x%lx) has received a XA_WM_HINTS notify", c->name, c->window);
            c->wmhints = XGetWMHints (display_info->dpy, c->window);
            if (c->wmhints)
            {
                if (c->wmhints->flags & WindowGroupHint)
                {
                    c->group_leader = c->wmhints->window_group;
                }
                if ((c->wmhints->flags & IconPixmapHint) && (screen_info->params->show_app_icon))
                {
                    clientUpdateIcon (c);
                    frameDraw (c, TRUE, FALSE);
                }
            }
            clientUpdateUrgency (c);
        }
        else if (ev->atom == display_info->atoms[WM_PROTOCOLS])
        {
            TRACE ("client \"%s\" (0x%lx) has received a wm_protocols notify", c->name, c->window);
            clientGetWMProtocols (c);
        }
        else if (ev->atom == display_info->atoms[WM_TRANSIENT_FOR])
        {
            TRACE ("client \"%s\" (0x%lx) has received a wm_transient_for notify", c->name, c->window);
            getTransientFor (display_info, c->screen_info->xroot, c->window, &w);
            if (clientCheckTransientWindow (c, w))
            {
                c->transient_for = w;
                clientRaise (c, w);
            }
        }
        else if (ev->atom == display_info->atoms[WIN_HINTS])
        {
            TRACE ("client \"%s\" (0x%lx) has received a win_hints notify", c->name, c->window);
            getHint (display_info, c->window, WIN_HINTS, (long *) &c->win_hints);
        }
        else if (ev->atom == display_info->atoms[NET_WM_WINDOW_TYPE])
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_wm_window_type notify", c->name, c->window);
            clientGetNetWmType (c);
            frameDraw (c, TRUE, FALSE);
        }
        else if ((ev->atom == display_info->atoms[NET_WM_STRUT]) || 
                 (ev->atom == display_info->atoms[NET_WM_STRUT_PARTIAL]))
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_wm_strut notify", c->name, c->window);
            clientGetNetStruts (c);
        }
        else if (ev->atom == display_info->atoms[WM_COLORMAP_WINDOWS])
        {
            TRACE ("client \"%s\" (0x%lx) has received a wm_colormap_windows notify", c->name, c->window);
            clientUpdateColormaps (c);
            if (c == clientGetFocus ())
            {
                clientInstallColormaps (c);
            }
        }
        else if (ev->atom == display_info->atoms[NET_WM_USER_TIME])
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_wm_user_time notify", c->name, c->window);
            if (getNetWMUserTime (display_info, c->window, &c->user_time))
            {
                FLAG_SET (c->flags, CLIENT_FLAG_HAS_USER_TIME);
            }
        }
        else if (ev->atom == display_info->atoms[NET_WM_WINDOW_OPACITY])
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_wm_opacity notify", c->name, c->window);
            if (!getOpacity (display_info, c->window, &c->opacity))
            {
                c->opacity =  NET_WM_OPAQUE;
            }
            compositorWindowSetOpacity (display_info, c->frame, c->opacity);
        }
        else if ((screen_info->params->show_app_icon) &&
                 ((ev->atom == display_info->atoms[NET_WM_ICON]) ||
                  (ev->atom == display_info->atoms[KWM_WIN_ICON])))
        {
            clientUpdateIcon (c);
            frameDraw (c, TRUE, FALSE);
        }
#ifdef HAVE_STARTUP_NOTIFICATION
        else if (ev->atom == display_info->atoms[NET_STARTUP_ID])
        {
            if (c->startup_id)
            {
                free (c->startup_id);
                c->startup_id = NULL;
            }
            getWindowStartupId (display_info, c->window, &c->startup_id);
        }
#endif
        return;
    }
    
    screen_info = myDisplayGetScreenFromWindow (display_info, ev->window);
    if (!screen_info)
    {
        return;
    }

    if (ev->atom == display_info->atoms[NET_DESKTOP_NAMES])
    {
        TRACE ("root has received a net_desktop_names notify");
        if (getUTF8String (display_info, screen_info->xroot, NET_DESKTOP_NAMES, &names, &length))
        {
            workspaceSetNames (screen_info, names, length);
        }
    }
    else if (ev->atom == display_info->atoms[GNOME_PANEL_DESKTOP_AREA])
    {
        TRACE ("root has received a gnome_panel_desktop_area notify");
        getGnomeDesktopMargins (display_info, screen_info->xroot, screen_info->gnome_margins);
        workspaceUpdateArea (screen_info);
    }
    else if (ev->atom == display_info->atoms[NET_DESKTOP_LAYOUT])
    {
        TRACE ("root has received a net_desktop_layout notify");
        getDesktopLayout(display_info, screen_info->xroot, screen_info->workspace_count, &screen_info->desktop_layout);
        placeSidewalks(screen_info, screen_info->params->wrap_workspaces);

    }
}

static void
handleClientMessage (DisplayInfo *display_info, XClientMessageEvent * ev)
{
    ScreenInfo *screen_info = NULL;
    Client *c = NULL;
    gboolean is_transient = FALSE;

    TRACE ("entering handleClientMessage");

    c = myDisplayGetClientFromWindow (display_info, ev->window, WINDOW);
    if (c)
    {
        screen_info = c->screen_info;
        is_transient = clientIsValidTransientOrModal (c);
        
        if ((ev->message_type == display_info->atoms[WM_CHANGE_STATE]) && (ev->format == 32) && (ev->data.l[0] == IconicState))
        {
            TRACE ("client \"%s\" (0x%lx) has received a wm_change_state event", c->name, c->window);
            if (!FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED) &&  CLIENT_CAN_HIDE_WINDOW (c))
            {
                clientHide (c, c->win_workspace, TRUE);
            }
        }
        else if ((ev->message_type == display_info->atoms[WIN_STATE]) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a win_state event", c->name, c->window);
            clientUpdateWinState (c, ev);
        }
        else if ((ev->message_type == display_info->atoms[WIN_LAYER]) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a win_layer event", c->name, c->window);
            if ((ev->data.l[0] != c->win_layer) && !is_transient)
            {
                clientSetLayer (c, ev->data.l[0]);
            }
        }
        else if ((ev->message_type == display_info->atoms[WIN_WORKSPACE]) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a win_workspace event", c->name, c->window);
            if ((ev->data.l[0] != c->win_workspace) && !is_transient)
            {
                clientSetWorkspace (c, ev->data.l[0], TRUE);
            }
        }
        else if ((ev->message_type == display_info->atoms[NET_WM_DESKTOP]) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_wm_desktop event", c->name, c->window);
            if (!is_transient)
            {
                if (ev->data.l[0] == ALL_WORKSPACES)
                {
                    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_STICK) && !FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
                    {
                        clientStick (c, TRUE);
                        frameDraw (c, FALSE, FALSE);
                    }
                }
                else
                {
                    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_STICK) && FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
                    {
                        clientUnstick (c, TRUE);
                        frameDraw (c, FALSE, FALSE);
                    }
                    if (ev->data.l[0] != c->win_workspace)
                    {
                        clientSetWorkspace (c, ev->data.l[0], TRUE);
                    }
                }
            }
        }
        else if ((ev->message_type == display_info->atoms[NET_CLOSE_WINDOW]) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_close_window event", c->name, c->window);
            clientClose (c);
        }
        else if ((ev->message_type == display_info->atoms[NET_WM_STATE]) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_wm_state event", c->name, c->window);
            clientUpdateNetState (c, ev);
        }
        else if ((ev->message_type == display_info->atoms[NET_WM_MOVERESIZE]) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_wm_moveresize event", c->name, c->window);
            g_warning ("Operation not supported (yet)");
            /* TBD */
        }
        else if ((ev->message_type == display_info->atoms[NET_ACTIVE_WINDOW]) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_active_window event", c->name, c->window);
            clientSetWorkspace (c, screen_info->current_ws, TRUE);
            clientShow (c, TRUE);
            clientRaise (c, None);
            if (ev->data.l[0] != 0)
            {
                clientSetFocus (screen_info, c, (Time) ev->data.l[1], NO_FOCUS_FLAG);
            }
            else
            {
                clientSetFocus (screen_info, c, CurrentTime, NO_FOCUS_FLAG);
            }
        }
        else if (ev->message_type == display_info->atoms[NET_REQUEST_FRAME_EXTENTS])
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_request_frame_extents event", c->name, c->window);
            setNetFrameExtents (display_info, c->window, frameTop (c), frameLeft (c),
                                                         frameRight (c), frameBottom (c)); 
        }
    }
    else
    {
        screen_info = myDisplayGetScreenFromWindow (display_info, ev->window);
        if (!screen_info)
        {
            return;
        }
        
        if (((ev->message_type == display_info->atoms[WIN_WORKSPACE]) || 
             (ev->message_type == display_info->atoms[NET_CURRENT_DESKTOP])) && (ev->format == 32))
        {
            TRACE ("root has received a win_workspace or a net_current_desktop event %i", ev->data.l[0]);
            if ((ev->data.l[0] >= 0) && (ev->data.l[0] < screen_info->workspace_count) && (ev->data.l[0] != screen_info->current_ws))
            {
                workspaceSwitch (screen_info, ev->data.l[0], NULL, TRUE);
            }
        }
        else if (((ev->message_type == display_info->atoms[WIN_WORKSPACE_COUNT]) || 
                  (ev->message_type == display_info->atoms[NET_NUMBER_OF_DESKTOPS])) && (ev->format == 32))
        {
            TRACE ("root has received a win_workspace_count event");
            if (ev->data.l[0] != screen_info->workspace_count)
            {
                workspaceSetCount (screen_info, ev->data.l[0]);
                getDesktopLayout(display_info, screen_info->xroot, screen_info->workspace_count, &screen_info->desktop_layout);
            }
        }
        else if ((ev->message_type == display_info->atoms[MANAGER]) && (ev->data.l[1] == screen_info->net_system_tray_selection) && (ev->format == 32))
        {
            TRACE ("root has received a net_system_tray_manager event");
            screen_info->systray = getSystrayWindow (display_info, screen_info->net_system_tray_selection);
        }
        else if ((ev->message_type == display_info->atoms[NET_SHOWING_DESKTOP]) && (ev->format == 32))
        {
            TRACE ("root has received a net_showing_desktop event");
            clientToggleShowDesktop (screen_info, ev->data.l[0]);
            setHint (display_info, screen_info->xroot, NET_SHOWING_DESKTOP, ev->data.l[0]);
        }
        else if (ev->message_type == display_info->atoms[NET_REQUEST_FRAME_EXTENTS])
        {
            TRACE ("window (0x%lx) has received a net_request_frame_extents event", c->name, ev->window);
            /* Size estimate from the decoration extents */
            setNetFrameExtents (display_info, ev->window, 
                                frameDecorationTop (screen_info),
                                frameDecorationLeft (screen_info),
                                frameDecorationRight (screen_info),
                                frameDecorationBottom (screen_info));
        }
        else
        {
            TRACE ("unidentified client message for window 0x%lx", ev->window);
        }
    }
}

static void
handleShape (DisplayInfo *display_info, XShapeEvent * ev)
{
    Client *c = NULL;

    TRACE ("entering handleShape");

    c = myDisplayGetClientFromWindow (display_info, ev->window, WINDOW);
    if (c)
    {
        if (ev->kind == ShapeBounding)
        {
            if ((ev->shaped) && !FLAG_TEST (c->flags, CLIENT_FLAG_HAS_SHAPE))
            {
                FLAG_SET (c->flags, CLIENT_FLAG_HAS_SHAPE);
            }
            else if (!(ev->shaped) && FLAG_TEST (c->flags, CLIENT_FLAG_HAS_SHAPE))
            {
                FLAG_UNSET (c->flags, CLIENT_FLAG_HAS_SHAPE);
            }
        }
        frameDraw (c, FALSE, TRUE);
    }
}

static void
handleColormapNotify (DisplayInfo *display_info, XColormapEvent * ev)
{
    Client *c = NULL;

    TRACE ("entering handleColormapNotify");

    c = myDisplayGetClientFromWindow (display_info, ev->window, WINDOW);
    if ((c) && (ev->window == c->window) && (ev->new))
    {
        if (c == clientGetFocus ())
        {
            clientInstallColormaps (c);
        }
    }
}

static void
handleMappingNotify (DisplayInfo *display_info, XMappingEvent * ev)
{
    GSList *screens;

    TRACE ("entering handleMappingNotify");

    /* Refreshes the stored modifier and keymap information */
    XRefreshKeyboardMapping (ev);

    /* Update internal modifiers masks if necessary */
    if (ev->request == MappingModifier)
    {
        TRACE ("handleMappingNotify: modifiers mapping has changed");
        initModifiers (display_info->dpy);
    }

    /* Regrab all keys if the notify is for keyboard (ie not pointer) */
    if (ev->request != MappingPointer)
    {
        TRACE ("handleMappingNotify: Regrab keys");
        for (screens = display_info->screens; screens; screens = g_slist_next (screens))
        {
            ScreenInfo *screen_info = (ScreenInfo *) screens->data;

            /* We need to reload all the settings to recompute the key bindings... */
            loadSettings (screen_info);

            /* ...then update all frames grabs... */
            clientUpdateAllFrames (screen_info, UPDATE_BUTTON_GRABS);

            /* ...and at last regrab the keys in out default window! */
            myScreenGrabKeys (screen_info);
        }
    }
}

static void
handleEvent (DisplayInfo *display_info, XEvent * ev)
{
    TRACE ("entering handleEvent");

    /* Update the display time */
    myDisplayUpdateCurentTime (display_info, ev);

    sn_process_event (ev);
    compositorHandleEvent (display_info, ev);
    switch (ev->type)
    {
        case MotionNotify:
            handleMotionNotify (display_info, (XMotionEvent *) ev);
            break;
        case KeyPress:
            handleKeyPress (display_info, (XKeyEvent *) ev);
            break;
        case ButtonPress:
            handleButtonPress (display_info, (XButtonEvent *) ev);
            break;
        case ButtonRelease:
            handleButtonRelease (display_info, (XButtonEvent *) ev);
            break;
        case DestroyNotify:
            handleDestroyNotify (display_info, (XDestroyWindowEvent *) ev);
            break;
        case UnmapNotify:
            handleUnmapNotify (display_info, (XUnmapEvent *) ev);
            break;
        case MapRequest:
            handleMapRequest (display_info, (XMapRequestEvent *) ev);
            break;
        case MapNotify:
            handleMapNotify (display_info, (XMapEvent *) ev);
            break;
        case ConfigureNotify:
            handleConfigureNotify (display_info, (XConfigureEvent *) ev);
            break;
        case ConfigureRequest:
            handleConfigureRequest (display_info, (XConfigureRequestEvent *) ev);
            break;
        case EnterNotify:
            handleEnterNotify (display_info, (XCrossingEvent *) ev);
            break;
        case LeaveNotify:
            handleLeaveNotify (display_info, (XCrossingEvent *) ev);
            break;
        case FocusIn:
            handleFocusIn (display_info, (XFocusChangeEvent *) ev);
            break;
        case FocusOut:
            handleFocusOut (display_info, (XFocusChangeEvent *) ev);
            break;
        case PropertyNotify:
            handlePropertyNotify (display_info, (XPropertyEvent *) ev);
            break;
        case ClientMessage:
            handleClientMessage (display_info, (XClientMessageEvent *) ev);
            break;
        case ColormapNotify:
            handleColormapNotify (display_info, (XColormapEvent *) ev);
            break;
        case MappingNotify:
            handleMappingNotify (display_info, (XMappingEvent *) ev);
            break;
        default:
            if ((display_info->have_shape) && (ev->type == display_info->shape_event_base))
            {
                handleShape (display_info, (XShapeEvent *) ev);
            }
    }
    if (!gdk_events_pending () && !XPending (display_info->dpy))
    {
        if (xfwm4_reload)
        {
            reloadSettings (display_info, UPDATE_ALL);
            xfwm4_reload = FALSE;
        }
        else if (xfwm4_quit)
        {
            gtk_main_quit ();
        }
    }
}

XfceFilterStatus
xfwm4_event_filter (XEvent * xevent, gpointer data)
{
    DisplayInfo *display_info = (DisplayInfo *) data;
    
    g_assert (display_info);
    
    TRACE ("entering xfwm4_event_filter");
    handleEvent (display_info, xevent);
    TRACE ("leaving xfwm4_event_filter");
    return XEV_FILTER_STOP;
}

/* GTK specific stuff */

static void
menu_callback (Menu * menu, MenuOp op, Window xid, gpointer menu_data, gpointer item_data)
{
    Client *c = NULL;

    TRACE ("entering menu_callback");

    if (!xfwmWindowDeleted(&menu_event_window))
    {
        xfwmWindowDelete (&menu_event_window);
    }

    if ((menu_data != NULL) && (xid != None))
    {
        ScreenInfo *screen_info = (ScreenInfo *) menu_data;
        c = clientGetFromWindow (screen_info, xid, WINDOW);
    }

    if (c)
    {
        c->button_pressed[MENU_BUTTON] = FALSE;

        switch (op)
        {
            case MENU_OP_QUIT:
                gtk_main_quit ();
                break;
            case MENU_OP_MAXIMIZE:
            case MENU_OP_UNMAXIMIZE:
                if (CLIENT_CAN_MAXIMIZE_WINDOW (c))
                {
                    clientToggleMaximized (c, WIN_STATE_MAXIMIZED, TRUE);
                }
                break;
            case MENU_OP_MINIMIZE:
                if (CLIENT_CAN_HIDE_WINDOW (c))
                {
                    clientHide (c, c->win_workspace, TRUE);
                }
                frameDraw (c, FALSE, FALSE);
                break;
            case MENU_OP_MINIMIZE_ALL:
                clientHideAll (c, c->win_workspace);
                frameDraw (c, FALSE, FALSE);
                break;
            case MENU_OP_UNMINIMIZE:
                clientShow (c, TRUE);
                break;
            case MENU_OP_SHADE:
            case MENU_OP_UNSHADE:
                clientToggleShaded (c);
                break;
            case MENU_OP_STICK:
            case MENU_OP_UNSTICK:
                clientToggleSticky (c, TRUE);
                frameDraw (c, FALSE, FALSE);
                break;
            case MENU_OP_WORKSPACES:
                clientSetWorkspace (c, GPOINTER_TO_INT (item_data), TRUE);
                frameDraw (c, FALSE, FALSE);
                break;
            case MENU_OP_DELETE:
                frameDraw (c, FALSE, FALSE);
                clientClose (c);
                break;
            case MENU_OP_CONTEXT_HELP:
                clientEnterContextMenuState (c);
                frameDraw (c, FALSE, FALSE);
                break;
            case MENU_OP_ABOVE:
            case MENU_OP_NORMAL:
                clientToggleAbove (c);
                /* Fall thru */
            default:
                frameDraw (c, FALSE, FALSE);
                break;
        }
    }
    else
    {
        gdk_beep ();
    }
    menu_free (menu);
}

void
initMenuEventWin (void)
{
    xfwmWindowInit (&menu_event_window);
}

static gboolean
show_popup_cb (GtkWidget * widget, GdkEventButton * ev, gpointer data)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;
    Menu *menu;
    MenuOp ops;
    MenuOp insensitive;
    Client *c = (Client *) data;
    gint x = ev->x_root;
    gint y = ev->y_root;
    
    TRACE ("entering show_popup_cb");

    if ((c) && ((ev->button == 1) || (ev->button == 3)))
    {
        c->button_pressed[MENU_BUTTON] = TRUE;
        frameDraw (c, FALSE, FALSE);
        y = c->y;
        ops = MENU_OP_DELETE | MENU_OP_MINIMIZE_ALL | MENU_OP_WORKSPACES;
        insensitive = 0;

        if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_CLOSE))
        {
            insensitive |= MENU_OP_DELETE;
        }

        if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
        {
            ops |= MENU_OP_UNMAXIMIZE;
            if (!CLIENT_CAN_MAXIMIZE_WINDOW (c))
            {
                insensitive |= MENU_OP_UNMAXIMIZE;
            }
        }
        else
        {
            ops |= MENU_OP_MAXIMIZE;
            if (!CLIENT_CAN_MAXIMIZE_WINDOW (c))
            {
                insensitive |= MENU_OP_MAXIMIZE;
            }
        }

        if (FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
        {
            ops |= MENU_OP_UNMINIMIZE;
            if (!CLIENT_CAN_HIDE_WINDOW (c))
            {
                insensitive |= MENU_OP_UNMINIMIZE;
            }
        }
        else
        {
            ops |= MENU_OP_MINIMIZE;
            if (!CLIENT_CAN_HIDE_WINDOW (c))
            {
                insensitive |= MENU_OP_MINIMIZE;
            }
        }

        if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            ops |= MENU_OP_UNSHADE;
        }
        else
        {
            ops |= MENU_OP_SHADE;
        }

        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
        {
            ops |= MENU_OP_UNSTICK;
            if (!CLIENT_CAN_STICK_WINDOW(c))
            {
                insensitive |= MENU_OP_UNSTICK;
            }
        }
        else
        {
            ops |= MENU_OP_STICK;
            if (!CLIENT_CAN_STICK_WINDOW(c))
            {
                insensitive |= MENU_OP_STICK;
            }
        }
    
        /* KDE extension */
        clientGetWMProtocols(c);
        if (FLAG_TEST (c->wm_flags, WM_FLAG_CONTEXT_HELP))
        {
            ops |= MENU_OP_CONTEXT_HELP;
        }

        if (FLAG_TEST(c->flags, CLIENT_FLAG_ABOVE))
        {
            ops |= MENU_OP_NORMAL;
            if (clientIsValidTransientOrModal (c) || 
                FLAG_TEST (c->flags, CLIENT_FLAG_BELOW | CLIENT_FLAG_FULLSCREEN))
            {
                insensitive |= MENU_OP_NORMAL;
            }
        }
        else
        {
            ops |= MENU_OP_ABOVE;
            if (clientIsValidTransientOrModal (c) || 
                FLAG_TEST (c->flags, CLIENT_FLAG_BELOW | CLIENT_FLAG_FULLSCREEN))
            {
                insensitive |= MENU_OP_ABOVE;
            }
        }

        if (clientIsValidTransientOrModal (c)
            || !FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_STICK)
            || FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
        {
            insensitive |= MENU_OP_WORKSPACES;
        }
    }
    else
    {
        return (TRUE);
    }
    
    /* c is not null here */
    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    
    if (screen_info->button_handler_id)
    {
        g_signal_handler_disconnect (GTK_OBJECT (myScreenGetGtkWidget (screen_info)), screen_info->button_handler_id);
    }
    screen_info->button_handler_id = g_signal_connect (GTK_OBJECT (myScreenGetGtkWidget (screen_info)),
                                              "button_press_event", GTK_SIGNAL_FUNC (show_popup_cb), (gpointer) NULL);

    if (!xfwmWindowDeleted(&menu_event_window))
    {
        xfwmWindowDelete (&menu_event_window);
    }
    /*
       Since all button press/release events are catched by the windows frames, there is some
       side effect with GTK menu. When a menu is opened, any click on the window frame is not
       detected as a click outside the menu, and the menu doesn't close.
       To avoid this (painless but annoying) behaviour, we just setup a no event window that
       "hides" the events to regular windows.
       That might look tricky, but it's very efficient and save plenty of lines of complicated
       code.
       Don't forget to delete that window once the menu is closed, though, or we'll get in
       trouble.
     */
    xfwmWindowTemp (screen_info,  
                    NULL, 0,
                    screen_info->xroot,
                    &menu_event_window, 0, 0, 
                    gdk_screen_get_width (screen_info->gscr),
                    gdk_screen_get_height (screen_info->gscr), 
                    NoEventMask);

    menu = menu_default (screen_info->gscr, c->window, ops, insensitive, menu_callback, 
                         c->win_workspace, screen_info->workspace_count, 
                         screen_info->workspace_names, screen_info->workspace_names_length,
                         display_info->xfilter, screen_info);

    if (!menu_popup (menu, x, y, ev->button, ev->time))
    {
        TRACE ("Cannot open menu");
        gdk_beep ();
        c->button_pressed[MENU_BUTTON] = FALSE;
        frameDraw (c, FALSE, FALSE);
        xfwmWindowDelete (&menu_event_window);
        menu_free (menu);
    }
    return (TRUE);
}

static gboolean
set_reload (GObject * obj, GdkEvent * ev, gpointer data)
{
    TRACE ("setting reload flag so all prefs will be reread at next event loop");
    xfwm4_reload = TRUE;
    return (TRUE);
}

static gboolean
dbl_click_time_cb (GObject * obj, GdkEvent * ev, gpointer data)
{
    DisplayInfo *display_info = (DisplayInfo *) data;
    GValue tmp_val = { 0, };

    g_return_val_if_fail (display_info, TRUE);
    
    g_value_init (&tmp_val, G_TYPE_INT);
    if (gdk_setting_get ("gtk-double-click-time", &tmp_val))
    {
        display_info->dbl_click_time = abs (g_value_get_int (&tmp_val));
    }

    return (TRUE);
}

static gboolean
client_event_cb (GtkWidget * widget, GdkEventClient * ev, gpointer data)
{
    TRACE ("entering client_event_cb");

    if (!atom_rcfiles)
    {
        atom_rcfiles = gdk_atom_intern ("_GTK_READ_RCFILES", FALSE);
    }

    if (ev->message_type == atom_rcfiles)
    {
        set_reload (G_OBJECT (widget), (GdkEvent *) ev, data);
    }

    return (FALSE);
}

void
initGtkCallbacks (ScreenInfo *screen_info)
{
    GtkSettings *settings;

    screen_info->button_handler_id = 
        g_signal_connect (GTK_OBJECT (myScreenGetGtkWidget (screen_info)),
                          "button_press_event", GTK_SIGNAL_FUNC (show_popup_cb), (gpointer) NULL);
    g_signal_connect (GTK_OBJECT (myScreenGetGtkWidget (screen_info)), "client_event",
                      GTK_SIGNAL_FUNC (client_event_cb), NULL);
    settings = gtk_settings_get_default ();
    if (settings)
    {
        g_signal_connect (settings, "notify::gtk-theme-name",
            G_CALLBACK (set_reload), NULL);
        g_signal_connect (settings, "notify::gtk-font-name",
            G_CALLBACK (set_reload), NULL);
        g_signal_connect (settings, "notify::gtk-double-click-time",
            G_CALLBACK (dbl_click_time_cb), (gpointer) (screen_info->display_info));
    }
}
