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
        xfwm4    - (c) 2002-2003 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfcegui4/libxfcegui4.h>
#include "main.h"
#include "hints.h"
#include "workspaces.h"
#include "settings.h"
#include "mywindow.h"
#include "frame.h"
#include "client.h"
#include "menu.h"
#include "startup_notification.h"

#define WIN_IS_BUTTON(win)      ((win == MYWINDOW_XWINDOW(c->buttons[HIDE_BUTTON])) || \
                                 (win == MYWINDOW_XWINDOW(c->buttons[CLOSE_BUTTON])) || \
                                 (win == MYWINDOW_XWINDOW(c->buttons[MAXIMIZE_BUTTON])) || \
                                 (win == MYWINDOW_XWINDOW(c->buttons[SHADE_BUTTON])) || \
                                 (win == MYWINDOW_XWINDOW(c->buttons[STICK_BUTTON])))

#define DBL_CLICK_GRAB          (ButtonMotionMask | \
                                 PointerMotionMask | \
                                 PointerMotionHintMask | \
                                 ButtonPressMask | \
                                 ButtonReleaseMask)
                                 
#define MODIFIER_MASK           (ShiftMask | \
                                 ControlMask | \
                                 AltMask | \
                                 MetaMask | \
                                 SuperMask | \
                                 HyperMask)

static guint raise_timeout = 0;
static gulong button_handler_id = 0;
static GdkAtom atom_rcfiles = GDK_NONE;
static Window menu_event_window = None;

static void menu_callback (Menu * menu, MenuOp op, Window client_xwindow,
    gpointer menu_data, gpointer item_data);
static gboolean show_popup_cb (GtkWidget * widget, GdkEventButton * ev,
    gpointer data);
static gboolean client_event_cb (GtkWidget * widget, GdkEventClient * ev);

typedef enum
{
    XFWM_BUTTON_UNDEFINED = 0,
    XFWM_BUTTON_DRAG = 1,
    XFWM_BUTTON_CLICK = 2,
    XFWM_BUTTON_CLICK_AND_DRAG = 3,
    XFWM_BUTTON_DOUBLE_CLICK = 4
}
XfwmButtonClickType;

static inline XfwmButtonClickType
typeOfClick (Window w, XEvent * ev, gboolean allow_double_click)
{
    unsigned int button;
    int xcurrent, ycurrent, x, y, total;
    int g = GrabSuccess;
    int clicks;
    Time t0, t1;

    g_return_val_if_fail (ev != NULL, XFWM_BUTTON_UNDEFINED);
    g_return_val_if_fail (w != None, XFWM_BUTTON_UNDEFINED);

    g = XGrabPointer (dpy, w, FALSE, DBL_CLICK_GRAB, GrabModeAsync,
        GrabModeAsync, None, None, ev->xbutton.time);
    if (g != GrabSuccess)
    {
        TRACE ("grab failed in typeOfClick");
        gdk_beep ();
        return XFWM_BUTTON_UNDEFINED;
    }

    button = ev->xbutton.button;
    x = xcurrent = ev->xbutton.x_root;
    y = ycurrent = ev->xbutton.y_root;
    t0 = ev->xbutton.time;
    t1 = t0;
    total = 0;
    clicks = 1;

    while ((ABS (x - xcurrent) < 1) && (ABS (y - ycurrent) < 1)
        && (total < params.dbl_click_time)
        && ((t1 - t0) < params.dbl_click_time))
    {
        g_usleep (10000);
        total += 10;
        if (XCheckMaskEvent (dpy, ButtonReleaseMask | ButtonPressMask, ev))
        {
            if (ev->xbutton.button == button)
            {
                clicks++;
            }
            t1 = ev->xbutton.time;
        }
        if (XCheckMaskEvent (dpy,
                ButtonMotionMask | PointerMotionMask | PointerMotionHintMask,
                ev))
        {
            xcurrent = ev->xmotion.x_root;
            ycurrent = ev->xmotion.y_root;
            t1 = ev->xmotion.time;
        }
        if ((XfwmButtonClickType) clicks == XFWM_BUTTON_DOUBLE_CLICK
            || (!allow_double_click
                && (XfwmButtonClickType) clicks == XFWM_BUTTON_CLICK))
        {
            break;
        }
    }
    XUngrabPointer (dpy, ev->xbutton.time);
    return (XfwmButtonClickType) clicks;
}

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
    Client *c;
    TRACE ("entering raise_cb");

    clear_timeout ();
    c = clientGetFocus ();
    if (c)
    {
        clientRaise (c);
    }
    return (TRUE);
}

static void
reset_timeout (void)
{
    if (raise_timeout)
    {
        g_source_remove (raise_timeout);
    }
    raise_timeout =
        g_timeout_add_full (0, params.raise_delay, (GtkFunction) raise_cb,
        NULL, NULL);
}

static inline void
moveRequest (Client * c, XEvent * ev)
{
    if (CLIENT_FLAG_TEST_AND_NOT (c,
            CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_MOVE,
            CLIENT_FLAG_FULLSCREEN))
    {
        clientMove (c, ev);
    }
}

static inline void
resizeRequest (Client * c, int corner, XEvent * ev)
{
    clientSetFocus (c, TRUE);

    if (CLIENT_FLAG_TEST_ALL (c,
            CLIENT_FLAG_HAS_RESIZE | CLIENT_FLAG_IS_RESIZABLE))
    {
        clientResize (c, corner, ev);
    }
    else if (CLIENT_FLAG_TEST_AND_NOT (c,
            CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_MOVE,
            CLIENT_FLAG_FULLSCREEN))
    {
        clientMove (c, ev);
    }
}

static inline void
spawn_shortcut (int i)
{
    GError *error = NULL;
    if ((i >= NB_KEY_SHORTCUTS) || (!params.shortcut_exec[i])
        || !strlen (params.shortcut_exec[i]))
    {
        return;
    }
    if (!g_spawn_command_line_async (params.shortcut_exec[i], &error))
    {
        if (error)
        {
            g_warning ("%s: %s", g_get_prgname (), error->message);
            g_error_free (error);
        }
    }
}

static inline void
handleMotionNotify (XMotionEvent * ev)
{
    int msx, msy, max;
    static int edge_scroll_x = 0;

    TRACE ("entering handleMotionNotify");

    if (params.workspace_count && params.wrap_workspaces
        && params.wrap_resistance)
    {
        msx = ev->x_root;
        msy = ev->y_root;
        max = MyDisplayFullWidth (dpy, screen) - 1;

        if ((msx == 0) || (msx == max))
        {
            edge_scroll_x++;
        }
        else
        {
            edge_scroll_x = 0;
        }
        if (edge_scroll_x > params.wrap_resistance)
        {
            edge_scroll_x = 0;
            if (msx == 0)
            {
                XWarpPointer (dpy, None, root, 0, 0, 0, 0, max - 10, msy);
                workspaceSwitch (workspace - 1, NULL);
            }
            else if (msx == max)
            {
                XWarpPointer (dpy, None, root, 0, 0, 0, 0, 10, msy);
                workspaceSwitch (workspace + 1, NULL);
            }
        }
    }
}

static inline void
handleKeyPress (XKeyEvent * ev)
{
    Client *c;
    int state, key;

    TRACE ("entering handleKeyEvent");

    c = clientGetFocus ();
    state = ev->state & MODIFIER_MASK;
    
    for (key = 0; key < KEY_COUNT; key++)
    {
        if ((params.keys[key].keycode == ev->keycode)
            && (params.keys[key].modifier == state))
        {
            break;
        }
    }

    if (c)
    {
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
                if (CLIENT_FLAG_TEST_ALL (c,
                        CLIENT_FLAG_HAS_RESIZE | CLIENT_FLAG_IS_RESIZABLE))
                {
                    clientResize (c, CORNER_BOTTOM_RIGHT, (XEvent *) ev);
                }
                break;
            case KEY_CYCLE_WINDOWS:
                clientCycle (c);
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
                clientToggleMaximized (c, WIN_STATE_MAXIMIZED);
                break;
            case KEY_MAXIMIZE_VERT:
                clientToggleMaximized (c, WIN_STATE_MAXIMIZED_VERT);
                break;
            case KEY_MAXIMIZE_HORIZ:
                clientToggleMaximized (c, WIN_STATE_MAXIMIZED_HORIZ);
                break;
            case KEY_SHADE_WINDOW:
                clientToggleShaded (c);
                break;
            case KEY_STICK_WINDOW:
                clientToggleSticky (c, TRUE);
                frameDraw (c, FALSE, FALSE);
                break;
            case KEY_MOVE_NEXT_WORKSPACE:
                workspaceSwitch (workspace + 1, c);
                break;
            case KEY_MOVE_PREV_WORKSPACE:
                workspaceSwitch (workspace - 1, c);
                break;
            case KEY_MOVE_WORKSPACE_1:
                workspaceSwitch (0, c);
                break;
            case KEY_MOVE_WORKSPACE_2:
                workspaceSwitch (1, c);
                break;
            case KEY_MOVE_WORKSPACE_3:
                workspaceSwitch (2, c);
                break;
            case KEY_MOVE_WORKSPACE_4:
                workspaceSwitch (3, c);
                break;
            case KEY_MOVE_WORKSPACE_5:
                workspaceSwitch (4, c);
                break;
            case KEY_MOVE_WORKSPACE_6:
                workspaceSwitch (5, c);
                break;
            case KEY_MOVE_WORKSPACE_7:
                workspaceSwitch (6, c);
                break;
            case KEY_MOVE_WORKSPACE_8:
                workspaceSwitch (7, c);
                break;
            case KEY_MOVE_WORKSPACE_9:
                workspaceSwitch (8, c);
                break;
            default:
                break;
        }
    }
    else
    {
        switch (key)
        {
            case KEY_CYCLE_WINDOWS:
                if (clients)
                {
                    clientCycle (clients->prev);
                }
                break;
            default:
                break;
        }
    }
    switch (key)
    {
        case KEY_NEXT_WORKSPACE:
            workspaceSwitch (workspace + 1, NULL);
            break;
        case KEY_PREV_WORKSPACE:
            workspaceSwitch (workspace - 1, NULL);
            break;
        case KEY_ADD_WORKSPACE:
            workspaceSetCount (params.workspace_count + 1);
            break;
        case KEY_DEL_WORKSPACE:
            workspaceSetCount (params.workspace_count - 1);
            break;
        case KEY_WORKSPACE_1:
            workspaceSwitch (0, NULL);
            break;
        case KEY_WORKSPACE_2:
            workspaceSwitch (1, NULL);
            break;
        case KEY_WORKSPACE_3:
            workspaceSwitch (2, NULL);
            break;
        case KEY_WORKSPACE_4:
            workspaceSwitch (3, NULL);
            break;
        case KEY_WORKSPACE_5:
            workspaceSwitch (4, NULL);
            break;
        case KEY_WORKSPACE_6:
            workspaceSwitch (5, NULL);
            break;
        case KEY_WORKSPACE_7:
            workspaceSwitch (6, NULL);
            break;
        case KEY_WORKSPACE_8:
            workspaceSwitch (7, NULL);
            break;
        case KEY_WORKSPACE_9:
            workspaceSwitch (8, NULL);
            break;
        case KEY_SHORTCUT_1:
            spawn_shortcut (0);
            break;
        case KEY_SHORTCUT_2:
            spawn_shortcut (1);
            break;
        case KEY_SHORTCUT_3:
            spawn_shortcut (2);
            break;
        case KEY_SHORTCUT_4:
            spawn_shortcut (3);
            break;
        case KEY_SHORTCUT_5:
            spawn_shortcut (4);
            break;
        case KEY_SHORTCUT_6:
            spawn_shortcut (5);
            break;
        case KEY_SHORTCUT_7:
            spawn_shortcut (6);
            break;
        case KEY_SHORTCUT_8:
            spawn_shortcut (7);
            break;
        case KEY_SHORTCUT_9:
            spawn_shortcut (8);
            break;
        case KEY_SHORTCUT_10:
            spawn_shortcut (9);
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
static inline void
edgeButton (Client * c, int part, XButtonEvent * ev)
{
    if (ev->button == Button2)
    {
        XfwmButtonClickType tclick;

        tclick = typeOfClick (c->frame, (XEvent *) ev, FALSE);

        if (tclick == XFWM_BUTTON_CLICK)
        {
            clientLower (c);
        }
        else
        {
            moveRequest (c, (XEvent *) ev);
        }
    }
    else
    {
        if (ev->button == Button1)
        {
            clientRaise (c);
        }
        if ((ev->button == Button1) || (ev->button == Button3))
        {
            resizeRequest (c, part, (XEvent *) ev);
        }
    }
}

static inline void
button1Action (Client * c, XButtonEvent * ev)
{
    XEvent copy_event = (XEvent) * ev;
    XfwmButtonClickType tclick;

    g_return_if_fail (c != NULL);
    g_return_if_fail (ev != NULL);

    clientSetFocus (c, TRUE);
    clientRaise (c);

    tclick = typeOfClick (c->frame, &copy_event, TRUE);

    if ((tclick == XFWM_BUTTON_DRAG)
        || (tclick == XFWM_BUTTON_CLICK_AND_DRAG))
    {
        moveRequest (c, (XEvent *) ev);
    }
    else if (tclick == XFWM_BUTTON_DOUBLE_CLICK)
    {
        switch (params.double_click_action)
        {
            case ACTION_MAXIMIZE:
                clientToggleMaximized (c, WIN_STATE_MAXIMIZED);
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

static inline void
titleButton (Client * c, int state, XButtonEvent * ev)
{
    g_return_if_fail (c != NULL);
    g_return_if_fail (ev != NULL);

    if (ev->button == Button1)
    {
        button1Action (c, ev);
    }
    else if (ev->button == Button2)
    {
        clientLower (c);
    }
    else if (ev->button == Button3)
    {
        /*
           We need to copy the event to keep the original event untouched
           for gtk to handle it (in case we open up the menu)
         */

        XEvent copy_event = (XEvent) * ev;
        XfwmButtonClickType tclick;

        tclick = typeOfClick (c->frame, &copy_event, FALSE);

        if (tclick == XFWM_BUTTON_DRAG)
        {
            moveRequest (c, (XEvent *) ev);
        }
        else
        {
            clientSetFocus (c, TRUE);
            if (params.raise_on_click)
            {
                clientRaise (c);
            }
            ev->window = ev->root;
            if (button_handler_id)
            {
                g_signal_handler_disconnect (GTK_OBJECT (getDefaultGtkWidget
                        ()), button_handler_id);
            }
            button_handler_id =
                g_signal_connect (GTK_OBJECT (getDefaultGtkWidget ()),
                "button_press_event", GTK_SIGNAL_FUNC (show_popup_cb),
                (gpointer) c);
            /* Let GTK handle this for us. */
        }
    }
    else if (ev->button == Button4)
    {
        /* Mouse wheel scroll up */
        if (!CLIENT_FLAG_TEST (c, CLIENT_FLAG_SHADED))
        {
            clientShade (c);
        }
    }
    else if (ev->button == Button5)
    {
        /* Mouse wheel scroll down */
        if (CLIENT_FLAG_TEST (c, CLIENT_FLAG_SHADED))
        {
            clientUnshade (c);
        }
    }
}

static inline void
rootScrollButton (XButtonEvent * ev)
{
    static Time lastscroll = (Time) 0;
    XEvent otherEvent;

    while (XCheckTypedWindowEvent (dpy, root, ButtonPress, &otherEvent))
    {
        if (otherEvent.xbutton.button != ev->button)
        {
            XPutBackEvent (dpy, &otherEvent);
        }
    }
    if ((ev->time - lastscroll) < 100)  /* ms */
    {
        /* Too many events in too little time, drop this event... */
        return;
    }
    lastscroll = ev->time;
    if (ev->button == Button4)
    {
        workspaceSwitch (workspace - 1, NULL);
    }
    else if (ev->button == Button5)
    {
        workspaceSwitch (workspace + 1, NULL);
    }
}


static inline void
handleButtonPress (XButtonEvent * ev)
{
    Client *c;
    Window win;
    int state, replay = FALSE;

    TRACE ("entering handleButtonPress");

    /* Clear timeout */
    clear_timeout ();

    c = clientGetFromWindow (ev->window, ANY);
    if (c)
    {
        state = ev->state &  MODIFIER_MASK;
        win = ev->subwindow;

        if ((ev->button == Button1) && (state == AltMask) && (params.easy_click))
        {
            button1Action (c, ev);
        }
        else if ((ev->button == Button2) && (state == AltMask) && (params.easy_click))
        {
            clientLower (c);
        }
        else if ((ev->button == Button3) && (state == AltMask) && (params.easy_click))
        {
            edgeButton (c, CORNER_BOTTOM_RIGHT, ev);
        }
        else if (WIN_IS_BUTTON (win))
        {
            if (ev->button <= Button3)
            {
                clientSetFocus (c, TRUE);
                if (params.raise_on_click)
                {
                    clientRaise (c);
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

                XEvent copy_event = (XEvent) * ev;
                XfwmButtonClickType tclick;

                tclick = typeOfClick (c->frame, &copy_event, TRUE);

                if (tclick == XFWM_BUTTON_DOUBLE_CLICK)
                {
                    clientClose (c);
                }
                else
                {
                    clientSetFocus (c, TRUE);
                    if (params.raise_on_click)
                    {
                        clientRaise (c);
                    }
                    ev->window = ev->root;
                    if (button_handler_id)
                    {
                        g_signal_handler_disconnect (GTK_OBJECT
                            (getDefaultGtkWidget ()), button_handler_id);
                    }
                    button_handler_id =
                        g_signal_connect (GTK_OBJECT (getDefaultGtkWidget ()),
                        "button_press_event", GTK_SIGNAL_FUNC (show_popup_cb),
                        (gpointer) c);
                    /* Let GTK handle this for us. */
                }
            }
        }
        else if (((ev->window != c->window) && (ev->button == Button2)
                && (state == 0)) || ((ev->button == Button2)
                && (state == (AltMask | ControlMask))))
        {
            clientLower (c);
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
        else
        {
            if (ev->button == Button1)
            {
                clientSetFocus (c, TRUE);
                if (params.raise_on_click)
                {
                    clientRaise (c);
                }
            }
            if (ev->window == c->window)
            {
                replay = TRUE;
            }
        }

        if (replay)
        {
            XAllowEvents (dpy, ReplayPointer, ev->time);
        }
        else
        {
            XAllowEvents (dpy, SyncPointer, ev->time);
        }
    }
    else if ((ev->window == root) && ((ev->button == Button4)
            || (ev->button == Button5)))
    {
        rootScrollButton (ev);
    }
    else
    {
        XUngrabPointer (dpy, CurrentTime);
        XSendEvent (dpy, gnome_win, FALSE, SubstructureNotifyMask,
            (XEvent *) ev);
    }
}

static inline void
handleButtonRelease (XButtonEvent * ev)
{
    TRACE ("entering handleButtonRelease");

    XSendEvent (dpy, gnome_win, FALSE, SubstructureNotifyMask, (XEvent *) ev);
}

static inline void
handleDestroyNotify (XDestroyWindowEvent * ev)
{
    Client *c;

    TRACE ("entering handleDestroyNotify");
    TRACE ("destroyed window is (0x%lx)", ev->window);

    c = clientGetFromWindow (ev->window, WINDOW);
    if (c)
    {
        TRACE ("DestroyNotify for \"%s\" (0x%lx)", c->name, c->window);
        clientPassFocus (c);
        clientUnframe (c, FALSE);
    }
}

static inline void
handleUnmapNotify (XUnmapEvent * ev)
{
    Client *c;

    TRACE ("entering handleUnmapNotify");
    TRACE ("unmapped window is (0x%lx)", ev->window);

    c = clientGetFromWindow (ev->window, WINDOW);
    if (c)
    {
        TRACE ("UnmapNotify for \"%s\" (0x%lx)", c->name, c->window);
        /* Reparenting generates an unmapnotify, don't pass focus in that case */
        if (CLIENT_FLAG_TEST (c, CLIENT_FLAG_REPARENTING))
        {
            CLIENT_FLAG_UNSET (c, CLIENT_FLAG_REPARENTING);
        }
        else
        {
            clientPassFocus (c);
        }
        if (c->ignore_unmap)
        {
            c->ignore_unmap--;
        }
        else
        {
            clientUnframe (c, FALSE);
        }
    }
}

static inline void
handleMapRequest (XMapRequestEvent * ev)
{
    Client *c;

    TRACE ("entering handleMapRequest");
    TRACE ("mapped window is (0x%lx)", ev->window);

    if (ev->window == None)
    {
        TRACE ("Mapping None ???");
        return;
    }

    c = clientGetFromWindow (ev->window, WINDOW);
    if (c)
    {
        clientShow (c, TRUE);
    }
    else
    {
        clientFrame (ev->window, FALSE);
    }
}

static inline void
handleConfigureRequest (XConfigureRequestEvent * ev)
{
    Client *c;
    XWindowChanges wc;
    XEvent otherEvent;

    TRACE ("entering handleConfigureRequest");
    TRACE ("configured window is (0x%lx)", ev->window);

    /* Compress events - logic taken from kwin */
    while (XCheckTypedWindowEvent (dpy, ev->window, ConfigureRequest,
            &otherEvent))
    {
        if (otherEvent.xconfigurerequest.value_mask == ev->value_mask)
        {
            ev = &otherEvent.xconfigurerequest;
        }
        else
        {
            XPutBackEvent (dpy, &otherEvent);
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

    c = clientGetFromWindow (ev->window, WINDOW);
    if (!c)
    {
        /* Some app tend or try to manipulate the wm frame to achieve fullscreen mode */
        c = clientGetFromWindow (ev->window, FRAME);
        if (c)
        {
            TRACE ("client %s (0x%lx) is attempting to manipulate its frame!",
                c->name, c->window);
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

        TRACE ("handleConfigureRequest managed window \"%s\" (0x%lx)",
            c->name, c->window);
        if (CLIENT_FLAG_TEST (c, CLIENT_FLAG_MOVING_RESIZING))
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
        if (ev->value_mask & (CWX | CWY | CWWidth | CWHeight))
        {
            if (CLIENT_FLAG_TEST (c, CLIENT_FLAG_MAXIMIZED))
            {
                clientRemoveMaximizeFlag (c);
            }
            constrained = TRUE;
        }
        /* Let's say that if the client performs a XRaiseWindow, we show the window if hidden */
        if ((ev->value_mask & CWStackMode) && (wc.stack_mode == Above))
        {
            if (c->win_workspace == workspace)
            {
                if (CLIENT_FLAG_TEST (c, CLIENT_FLAG_HIDDEN))
                {
                    clientShow (c, TRUE);
                }
            }
        }
        clientConfigure (c, &wc, ev->value_mask, constrained);
    }
    else
    {
        TRACE ("unmanaged configure request for win 0x%lx", ev->window);
        XConfigureWindow (dpy, ev->window, ev->value_mask, &wc);
    }
}

static inline void
handleEnterNotify (XCrossingEvent * ev)
{
    Client *c;

    TRACE ("entering handleEnterNotify");

    if ((ev->mode == NotifyGrab) || (ev->mode == NotifyUngrab)
        || (ev->detail > NotifyNonlinearVirtual))
    {
        /* We're not interested in such notifications */
        return;
    }

    TRACE ("entered window is (0x%lx)", ev->window);

    c = clientGetFromWindow (ev->window, FRAME);
    if (c && !(params.click_to_focus) && (clientAcceptFocus (c)))
    {
        TRACE ("EnterNotify window is \"%s\"", c->name);
        if ((c->type != WINDOW_DOCK) && (c->type != WINDOW_DESKTOP))
        {
            clientSetFocus (c, TRUE);
        }
    }
}

static inline void
handleLeaveNotify (XCrossingEvent * ev)
{
    TRACE ("entering handleLeaveNotify");

    /* Actually, we have nothing to do here... */
}

static inline void
handleFocusIn (XFocusChangeEvent * ev)
{
    Client *c;

    TRACE ("entering handleFocusIn");

    if ((ev->mode == NotifyGrab) || (ev->mode == NotifyUngrab)
        || (ev->detail > NotifyNonlinearVirtual))
    {
        /* We're not interested in such notifications */
        return;
    }

    c = clientGetFromWindow (ev->window, WINDOW);
    TRACE ("focused window is (0x%lx)", ev->window);
    if (c)
    {
        TRACE ("focus set to \"%s\" (0x%lx)", c->name, c->window);
        clientUpdateFocus (c);
        if (params.raise_on_focus && !params.click_to_focus)
        {
            reset_timeout ();
        }
    }
}

static inline void
handleFocusOut (XFocusChangeEvent * ev)
{
    Client *c;
    TRACE ("entering handleFocusOut");

    if ((ev->mode == NotifyGrab) || (ev->mode == NotifyUngrab)
        || (ev->detail != NotifyNonlinear))
    {
        /* We're not interested in such notifications */
        return;
    }

    c = clientGetFromWindow (ev->window, WINDOW);
    TRACE ("focused out window is (0x%lx)", ev->window);
    if (c && (c == clientGetFocus ()))
    {
        TRACE ("focus lost from \"%s\" (0x%lx)", c->name, c->window);
        clientUpdateFocus (NULL);
        /* Clear timeout */
        clear_timeout ();
    }
}

static inline void
handlePropertyNotify (XPropertyEvent * ev)
{
    Client *c;

    TRACE ("entering handlePropertyNotify");

    c = clientGetFromWindow (ev->window, WINDOW);
    if (c)
    {
        if (ev->atom == XA_WM_NORMAL_HINTS)
        {
            TRACE
                ("client \"%s\" (0x%lx) has received a XA_WM_NORMAL_HINTS notify",
                c->name, c->window);
            clientGetWMNormalHints (c, TRUE);
        }
        else if ((ev->atom == XA_WM_NAME) || (ev->atom == net_wm_name))
        {
            TRACE ("client \"%s\" (0x%lx) has received a XA_WM_NAME notify",
                c->name, c->window);
            if (c->name)
            {
                free (c->name);
            }
            getWindowName (dpy, c->window, &c->name);
            CLIENT_FLAG_SET (c, CLIENT_FLAG_NAME_CHANGED);
            frameDraw (c, TRUE, FALSE);
        }
        else if (ev->atom == motif_wm_hints)
        {
            TRACE
                ("client \"%s\" (0x%lx) has received a motif_wm_hints notify",
                c->name, c->window);
            clientGetMWMHints (c, TRUE);
        }
        else if (ev->atom == XA_WM_HINTS)
        {
            TRACE
                ("client \"%s\" (0x%lx) has received a XA_WM_HINTS notify\n",
                c->name, c->window);
            c->wmhints = XGetWMHints (dpy, c->window);
            if (c->wmhints)
            {
                c->group_leader = c->wmhints->window_group;
            }
        }
        else if (ev->atom == win_hints)
        {
            TRACE ("client \"%s\" (0x%lx) has received a win_hints notify",
                c->name, c->window);
            getGnomeHint (dpy, c->window, win_hints, &c->win_hints);
        }
        else if (ev->atom == net_wm_window_type)
        {
            TRACE
                ("client \"%s\" (0x%lx) has received a net_wm_window_type notify",
                c->name, c->window);
            clientGetNetWmType (c);
            frameDraw (c, TRUE, FALSE);
        }
        else if (ev->atom == net_wm_strut)
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_wm_strut notify",
                c->name, c->window);
            clientGetNetStruts (c);
        }
        else if (ev->atom == wm_colormap_windows)
        {
            clientUpdateColormaps (c);
            if (c == clientGetFocus ())
            {
                clientInstallColormaps (c);
            }
        }
#ifdef HAVE_STARTUP_NOTIFICATION
        else if (ev->atom == net_startup_id)
        {
            if (c->startup_id)
            {
                free (c->startup_id);
                c->startup_id = NULL;
            }
            getWindowStartupId (dpy, c->window, &c->startup_id);
        }
#endif

    }
    else if (ev->atom == gnome_panel_desktop_area)
    {
        TRACE ("root has received a gnome_panel_desktop_area notify");
        getGnomeDesktopMargins (dpy, screen, gnome_margins);
        workspaceUpdateArea (margins, gnome_margins);
    }
}

static inline void
handleClientMessage (XClientMessageEvent * ev)
{
    Client *c;

    TRACE ("entering handleClientMessage");

    /* Don't get surprised with the multiple "if (!clientIsTransient(c))" tests
       xfwm4 really treats transient differently
     */

    c = clientGetFromWindow (ev->window, WINDOW);
    if (c)
    {
        if ((ev->message_type == wm_change_state) && (ev->format == 32)
            && (ev->data.l[0] == IconicState))
        {
            TRACE
                ("client \"%s\" (0x%lx) has received a wm_change_state event",
                c->name, c->window);
            if (!CLIENT_FLAG_TEST (c, CLIENT_FLAG_HIDDEN) && 
                 CLIENT_CAN_HIDE_WINDOW (c))
            {
                clientHide (c, c->win_workspace, TRUE);
            }
        }
        else if ((ev->message_type == win_state) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a win_state event",
                c->name, c->window);
            clientUpdateWinState (c, ev);
        }
        else if ((ev->message_type == win_layer) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a win_layer event",
                c->name, c->window);
            if ((ev->data.l[0] != c->win_layer) && !clientIsTransient (c))
            {
                clientSetLayer (c, ev->data.l[0]);
                clientSetNetState (c);
            }
        }
        else if ((ev->message_type == win_workspace) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a win_workspace event",
                c->name, c->window);
            if ((ev->data.l[0] != c->win_workspace) && !clientIsTransient (c))
            {
                clientSetWorkspace (c, ev->data.l[0], TRUE);
            }
        }
        else if ((ev->message_type == net_wm_desktop) && (ev->format == 32))
        {
            TRACE
                ("client \"%s\" (0x%lx) has received a net_wm_desktop event",
                c->name, c->window);
            if (!clientIsTransient (c))
            {
                if (ev->data.l[0] == ALL_WORKSPACES)
                {
                    if (CLIENT_FLAG_TEST_AND_NOT (c, CLIENT_FLAG_HAS_STICK,
                            CLIENT_FLAG_STICKY))
                    {
                        clientStick (c, TRUE);
                        frameDraw (c, FALSE, FALSE);
                    }
                }
                else
                {
                    if (CLIENT_FLAG_TEST_ALL (c,
                            CLIENT_FLAG_HAS_STICK | CLIENT_FLAG_STICKY))
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
        else if ((ev->message_type == net_close_window) && (ev->format == 32))
        {
            TRACE
                ("client \"%s\" (0x%lx) has received a net_close_window event",
                c->name, c->window);
            clientClose (c);
        }
        else if ((ev->message_type == net_wm_state) && (ev->format == 32))
        {
            TRACE ("client \"%s\" (0x%lx) has received a net_wm_state event",
                c->name, c->window);
            clientUpdateNetState (c, ev);
        }
        else if ((ev->message_type == net_wm_moveresize)
            && (ev->format == 32))
        {
            TRACE
                ("client \"%s\" (0x%lx) has received a net_wm_moveresize event",
                c->name, c->window);
            g_message (_("%s: Operation not supported (yet)\n"),
                g_get_prgname ());
            /* TBD */
        }
        else if ((ev->message_type == net_active_window)
            && (ev->format == 32))
        {
            TRACE
                ("client \"%s\" (0x%lx) has received a net_active_window event",
                c->name, c->window);
            workspaceSwitch (c->win_workspace, NULL);
            clientShow (c, TRUE);
            clientRaise (c);
            clientSetFocus (c, TRUE);
        }
    }
    else
    {
        if (((ev->message_type == win_workspace)
                || (ev->message_type == net_current_desktop))
            && (ev->format == 32))
        {
            TRACE
                ("root has received a win_workspace or a net_current_desktop event");
            if (ev->data.l[0] != workspace)
            {
                workspaceSwitch (ev->data.l[0], NULL);
            }
        }
        else if (((ev->message_type == win_workspace_count)
                || (ev->message_type == net_number_of_desktops))
            && (ev->format == 32))
        {
            TRACE ("root has received a win_workspace_count event");
            if (ev->data.l[0] != params.workspace_count)
            {
                workspaceSetCount (ev->data.l[0]);
            }
        }
        else
        {
            TRACE ("unidentified client message for window 0x%lx",
                ev->window);
        }
    }
}

static inline void
handleShape (XShapeEvent * ev)
{
    Client *c;

    TRACE ("entering handleShape");

    c = clientGetFromWindow (ev->window, WINDOW);
    if (c)
    {
        frameDraw (c, FALSE, TRUE);
    }
}

static inline void
handleColormapNotify (XColormapEvent * ev)
{
    Client *c;

    TRACE ("entering handleColormapNotify");

    c = clientGetFromWindow (ev->window, WINDOW);
    if ((c) && (ev->window == c->window) && (ev->new))
    {
        if (c == clientGetFocus ())
        {
            clientInstallColormaps (c);
        }
    }
}

void
handleEvent (XEvent * ev)
{
    TRACE ("entering handleEvent");

    sn_process_event (ev);
    switch (ev->type)
    {
        case MotionNotify:
            handleMotionNotify ((XMotionEvent *) ev);
            break;
        case KeyPress:
            handleKeyPress ((XKeyEvent *) ev);
            break;
        case ButtonPress:
            handleButtonPress ((XButtonEvent *) ev);
            break;
        case ButtonRelease:
            handleButtonRelease ((XButtonEvent *) ev);
            break;
        case DestroyNotify:
            handleDestroyNotify ((XDestroyWindowEvent *) ev);
            break;
        case UnmapNotify:
            handleUnmapNotify ((XUnmapEvent *) ev);
            break;
        case MapRequest:
            handleMapRequest ((XMapRequestEvent *) ev);
            break;
        case ConfigureRequest:
            handleConfigureRequest ((XConfigureRequestEvent *) ev);
            break;
        case EnterNotify:
            handleEnterNotify ((XCrossingEvent *) ev);
            break;
        case LeaveNotify:
            handleLeaveNotify ((XCrossingEvent *) ev);
            break;
        case FocusIn:
            handleFocusIn ((XFocusChangeEvent *) ev);
            break;
        case FocusOut:
            handleFocusOut ((XFocusChangeEvent *) ev);
            break;
        case PropertyNotify:
            handlePropertyNotify ((XPropertyEvent *) ev);
            break;
        case ClientMessage:
            handleClientMessage ((XClientMessageEvent *) ev);
            break;
        case ColormapNotify:
            handleColormapNotify ((XColormapEvent *) ev);
            break;
        default:
            if (shape && (ev->type == shape_event))
            {
                handleShape ((XShapeEvent *) ev);
            }
    }
    if (!gdk_events_pending () && !XPending (dpy))
    {
        if (reload)
        {
            reloadSettings (UPDATE_ALL);
            reload = FALSE;
        }
        else if (quit)
        {
            gtk_main_quit ();
        }
    }
}


GtkToXEventFilterStatus
xfwm4_event_filter (XEvent * xevent, gpointer data)
{
    TRACE ("entering xfwm4_event_filter");
    handleEvent (xevent);
    TRACE ("leaving xfwm4_event_filter");
    return XEV_FILTER_STOP;
}

/* GTK specific stuff */

static void
menu_callback (Menu * menu, MenuOp op, Window client_xwindow,
    gpointer menu_data, gpointer item_data)
{
    Client *c = NULL;

    TRACE ("entering menu_callback");

    if (menu_event_window)
    {
        removeTmpEventWin (menu_event_window);
        menu_event_window = None;
    }

    if (menu_data)
    {
        c = (Client *) menu_data;
        c = clientGetFromWindow (c->window, WINDOW);
        if (!c)
        {
            menu_free (menu);
            return;
        }
        c->button_pressed[MENU_BUTTON] = FALSE;
    }

    switch (op)
    {
        case MENU_OP_QUIT:
            gtk_main_quit ();
            break;
        case MENU_OP_MAXIMIZE:
        case MENU_OP_UNMAXIMIZE:
            if (CLIENT_CAN_MAXIMIZE_WINDOW (c))
            {
                clientToggleMaximized (c, WIN_STATE_MAXIMIZED);
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
        default:
            frameDraw (c, FALSE, FALSE);
            break;
    }
    menu_free (menu);
}

static gboolean
show_popup_cb (GtkWidget * widget, GdkEventButton * ev, gpointer data)
{
    Menu *menu;
    MenuOp ops;
    MenuOp insensitive;
    Client *c = NULL;
    gint x = ev->x_root;
    gint y = ev->y_root;

    TRACE ("entering show_popup_cb");

    if (((ev->button == 1) || (ev->button == 3)) && (c = (Client *) data))
    {
        c->button_pressed[MENU_BUTTON] = TRUE;
        frameDraw (c, FALSE, FALSE);
        y = c->y;
        ops = MENU_OP_DELETE | MENU_OP_MINIMIZE_ALL | MENU_OP_WORKSPACES;
        insensitive = 0;

        if (!CLIENT_FLAG_TEST (c, CLIENT_FLAG_HAS_CLOSE))
        {
            insensitive |= MENU_OP_DELETE;
        }

        if (CLIENT_FLAG_TEST (c, CLIENT_FLAG_MAXIMIZED))
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

        if (CLIENT_FLAG_TEST (c, CLIENT_FLAG_HIDDEN))
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

        if (CLIENT_FLAG_TEST (c, CLIENT_FLAG_SHADED))
        {
            ops |= MENU_OP_UNSHADE;
        }
        else
        {
            ops |= MENU_OP_SHADE;
        }

        if (CLIENT_FLAG_TEST (c, CLIENT_FLAG_STICKY))
        {
            ops |= MENU_OP_UNSTICK;
            if (!CLIENT_FLAG_TEST (c, CLIENT_FLAG_HAS_STICK))
            {
                insensitive |= MENU_OP_UNSTICK;
            }
        }
        else
        {
            ops |= MENU_OP_STICK;
            if (!CLIENT_FLAG_TEST (c, CLIENT_FLAG_HAS_STICK))
            {
                insensitive |= MENU_OP_STICK;
            }
        }

        if (clientIsTransient (c)
            || !CLIENT_FLAG_TEST (c, CLIENT_FLAG_HAS_STICK)
            || CLIENT_FLAG_TEST (c, CLIENT_FLAG_STICKY))
        {
            insensitive |= MENU_OP_WORKSPACES;
        }
    }
    else
    {
        return (TRUE);
    }

    if (button_handler_id)
    {
        g_signal_handler_disconnect (GTK_OBJECT (getDefaultGtkWidget ()),
            button_handler_id);
    }
    button_handler_id =
        g_signal_connect (GTK_OBJECT (getDefaultGtkWidget ()),
        "button_press_event", GTK_SIGNAL_FUNC (show_popup_cb),
        (gpointer) NULL);

    if (menu_event_window)
    {
        removeTmpEventWin (menu_event_window);
        menu_event_window = None;
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
    menu_event_window = setTmpEventWin (NoEventMask);
    menu =
        menu_default (ops, insensitive, menu_callback, c->win_workspace,
        params.workspace_count, c);
    if (!menu_popup (menu, x, y, ev->button, ev->time))
    {
        TRACE ("Cannot open menu");
        gdk_beep ();
        c->button_pressed[MENU_BUTTON] = FALSE;
        frameDraw (c, FALSE, FALSE);
        removeTmpEventWin (menu_event_window);
        menu_event_window = None;
        menu_free (menu);
    }
    return (TRUE);
}

static gboolean
set_reload (void)
{
    TRACE
        ("setting reload flag so all prefs will be reread at next event loop");
    reload = TRUE;
    return (TRUE);
}

static gboolean
dbl_click_time (void)
{
    GValue tmp_val = { 0, };

    TRACE ("setting dbl_click_time");

    g_value_init (&tmp_val, G_TYPE_INT);
    if (gdk_setting_get ("gtk-double-click-time", &tmp_val))
    {
        params.dbl_click_time = abs (g_value_get_int (&tmp_val));
    }

    return (TRUE);
}

static gboolean
client_event_cb (GtkWidget * widget, GdkEventClient * ev)
{
    TRACE ("entering client_event_cb");

    if (!atom_rcfiles)
    {
        atom_rcfiles = gdk_atom_intern ("_GTK_READ_RCFILES", FALSE);
    }

    if (ev->message_type == atom_rcfiles)
    {
        set_reload ();
    }

    return (FALSE);
}

void
initGtkCallbacks (void)
{
    GtkSettings *settings;

    button_handler_id =
        g_signal_connect (GTK_OBJECT (getDefaultGtkWidget ()),
        "button_press_event", GTK_SIGNAL_FUNC (show_popup_cb),
        (gpointer) NULL);
    g_signal_connect (GTK_OBJECT (getDefaultGtkWidget ()), "client_event",
        GTK_SIGNAL_FUNC (client_event_cb), (gpointer) NULL);

    settings = gtk_settings_get_default ();
    if (settings)
    {
        g_signal_connect (settings, "notify::gtk-theme-name",
            G_CALLBACK (set_reload), NULL);
        g_signal_connect (settings, "notify::gtk-font-name",
            G_CALLBACK (set_reload), NULL);
        g_signal_connect (settings, "notify::gtk-double-click-time",
            G_CALLBACK (dbl_click_time), NULL);
    }
}
