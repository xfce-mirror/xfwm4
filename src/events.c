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

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <libxfcegui4/libxfcegui4.h>
#include "main.h"
#include "workspaces.h"
#include "settings.h"
#include "frame.h"
#include "client.h"
#include "menu.h"
#include "debug.h"
#include "my_intl.h"

static guint raise_timeout = 0;
static gulong button_handler_id = 0;
static GdkAtom atom_rcfiles = GDK_NONE;
static Window menu_event_window = None;

static void menu_callback(Menu * menu, MenuOp op, Window client_xwindow, gpointer menu_data, gpointer item_data);
static gboolean show_popup_cb(GtkWidget * widget, GdkEventButton * ev, gpointer data);
static gboolean client_event_cb(GtkWidget * widget, GdkEventClient * ev);

typedef enum
{
    XFWM_BUTTON_UNDEFINED = 0,
    XFWM_BUTTON_DRAG = 1,
    XFWM_BUTTON_CLICK = 2,
    XFWM_BUTTON_CLICK_AND_DRAG = 3,
    XFWM_BUTTON_DOUBLE_CLICK = 4
}
XfwmButtonClickType;

static inline XfwmButtonClickType typeOfClick(Window w, XEvent * ev, gboolean allow_double_click)
{
    unsigned int button;
    int xcurrent, ycurrent, x, y, total;
    int g = GrabSuccess;
    int clicks;
    Time t0, t1;

    g_return_val_if_fail(ev != NULL, XFWM_BUTTON_UNDEFINED);
    g_return_val_if_fail(w != None, XFWM_BUTTON_UNDEFINED);

    g = XGrabPointer(dpy, w, False, ButtonMotionMask | PointerMotionMask | PointerMotionHintMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, ev->xbutton.time);
    if(g != GrabSuccess)
    {
        DBG("grab failed in typeOfClick\n");
        gdk_beep();
        return XFWM_BUTTON_UNDEFINED;
    }

    button = ev->xbutton.button;
    x = xcurrent = ev->xbutton.x_root;
    y = ycurrent = ev->xbutton.y_root;
    t0 = ev->xbutton.time;
    t1 = t0;
    total = 0;
    clicks = 1;

    while((ABS(x - xcurrent) < 1) && (ABS(y - ycurrent) < 1) && (total < params.dbl_click_time) && ((t1 - t0) < params.dbl_click_time))
    {
        g_usleep(10000);
        total += 10;
        if(XCheckMaskEvent(dpy, ButtonReleaseMask | ButtonPressMask, ev))
        {
            if(ev->xbutton.button == button)
            {
                clicks++;
            }
            t1 = ev->xbutton.time;
        }
        if(XCheckMaskEvent(dpy, ButtonMotionMask | PointerMotionMask | PointerMotionHintMask, ev))
        {
            xcurrent = ev->xmotion.x_root;
            ycurrent = ev->xmotion.y_root;
            t1 = ev->xmotion.time;
        }
        if((XfwmButtonClickType) clicks == XFWM_BUTTON_DOUBLE_CLICK || (!allow_double_click && (XfwmButtonClickType) clicks == XFWM_BUTTON_CLICK))
        {
            break;
        }
    }
    XUngrabPointer(dpy, ev->xbutton.time);
    return (XfwmButtonClickType) clicks;
}

static void clear_timeout(void)
{
    if(raise_timeout)
    {
        gtk_timeout_remove(raise_timeout);
        raise_timeout = 0;
    }
}

static gboolean raise_cb(gpointer data)
{
    Client *c;
    DBG("entering raise_cb\n");

    clear_timeout();
    c = clientGetFocus();
    if(c)
    {
        clientRaise(c);
    }
    return (TRUE);
}

static void reset_timeout(void)
{
    if(raise_timeout)
    {
        gtk_timeout_remove(raise_timeout);
    }
    raise_timeout = gtk_timeout_add(params.raise_delay, (GtkFunction) raise_cb, NULL);
}

static inline void _moveRequest(Client * c, XEvent * ev)
{
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_BORDER) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_MOVE) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_FULLSCREEN))
    {
        clientMove(c, ev);
    }
}

static inline void _resizeRequest(Client * c, int corner, XEvent * ev)
{
    clientSetFocus(c, True);

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_RESIZE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_IS_RESIZABLE))
    {
        clientResize(c, corner, ev);
    }
    else if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_BORDER) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_MOVE) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_FULLSCREEN))
    {
        clientMove(c, ev);
    }
}

static inline void spawn_shortcut(int i)
{
    GError *error = NULL;
    if ((i >= NB_KEY_SHORTCUTS) || (!params.shortcut_exec[i]) || !strlen(params.shortcut_exec[i]))
    {
        return;
    }
    if (!g_spawn_command_line_async (params.shortcut_exec[i], &error))
    {
        if (error)
	{
	    g_warning("%s: %s",  g_get_prgname(), error->message);
	    g_error_free(error);
	}
    }
}

static inline void handleKeyPress(XKeyEvent * ev)
{
    Client *c;
    int state, key;
    XEvent e;

    DBG("entering handleKeyEvent\n");

    c = clientGetFocus();
    state = ev->state & (ShiftMask | ControlMask | AltMask | MetaMask);
    for(key = 0; key < KEY_COUNT; key++)
    {
        if((params.keys[key].keycode == ev->keycode) && (params.keys[key].modifier == state))
        {
            break;
        }
    }

    if(c)
    {
        switch (key)
        {
            case KEY_MOVE_UP:
            case KEY_MOVE_DOWN:
            case KEY_MOVE_LEFT:
            case KEY_MOVE_RIGHT:
                _moveRequest(c, (XEvent *) ev);
                break;
            case KEY_RESIZE_UP:
            case KEY_RESIZE_DOWN:
            case KEY_RESIZE_LEFT:
            case KEY_RESIZE_RIGHT:
                if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_RESIZE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_IS_RESIZABLE))
                {
                    clientResize(c, CORNER_BOTTOM_RIGHT, (XEvent *) ev);
                }
                break;
            case KEY_CYCLE_WINDOWS:
                clientCycle(c);
                break;
            case KEY_CLOSE_WINDOW:
                clientClose(c);
                break;
            case KEY_HIDE_WINDOW:
                if(CLIENT_CAN_HIDE_WINDOW(c))
                {
                    clientHide(c, True);
                }
                break;
            case KEY_MAXIMIZE_WINDOW:
                if(CLIENT_CAN_MAXIMIZE_WINDOW(c))
                {
                    clientToggleMaximized(c, WIN_STATE_MAXIMIZED);
                }
                break;
            case KEY_MAXIMIZE_VERT:
                if(CLIENT_CAN_MAXIMIZE_WINDOW(c))
                {
                    clientToggleMaximized(c, WIN_STATE_MAXIMIZED_VERT);
                }
                break;
            case KEY_MAXIMIZE_HORIZ:
                if(CLIENT_CAN_MAXIMIZE_WINDOW(c))
                {
                    clientToggleMaximized(c, WIN_STATE_MAXIMIZED_HORIZ);
                }
                break;
            case KEY_SHADE_WINDOW:
                clientToggleShaded(c);
                break;
            case KEY_STICK_WINDOW:
                clientToggleSticky(c, TRUE);
                break;
            case KEY_MOVE_NEXT_WORKSPACE:
                workspaceSwitch(workspace + 1, c);
                break;
            case KEY_MOVE_PREV_WORKSPACE:
                workspaceSwitch(workspace - 1, c);
                break;
            case KEY_MOVE_WORKSPACE_1:
                workspaceSwitch(0, c);
                break;
            case KEY_MOVE_WORKSPACE_2:
                workspaceSwitch(1, c);
                break;
            case KEY_MOVE_WORKSPACE_3:
                workspaceSwitch(2, c);
                break;
            case KEY_MOVE_WORKSPACE_4:
                workspaceSwitch(3, c);
                break;
            case KEY_MOVE_WORKSPACE_5:
                workspaceSwitch(4, c);
                break;
            case KEY_MOVE_WORKSPACE_6:
                workspaceSwitch(5, c);
                break;
            case KEY_MOVE_WORKSPACE_7:
                workspaceSwitch(6, c);
                break;
            case KEY_MOVE_WORKSPACE_8:
                workspaceSwitch(7, c);
                break;
            case KEY_MOVE_WORKSPACE_9:
                workspaceSwitch(8, c);
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
                if(clients)
                {
                    clientCycle(clients->prev);
                }
                break;
	    default:
	        break;
        }
    }
    switch (key)
    {
        case KEY_NEXT_WORKSPACE:
            workspaceSwitch(workspace + 1, NULL);
            break;
        case KEY_PREV_WORKSPACE:
            workspaceSwitch(workspace - 1, NULL);
            break;
        case KEY_ADD_WORKSPACE:
            workspaceSetCount(params.workspace_count + 1);
            break;
        case KEY_DEL_WORKSPACE:
            workspaceSetCount(params.workspace_count - 1);
            break;
        case KEY_WORKSPACE_1:
            workspaceSwitch(0, NULL);
            break;
        case KEY_WORKSPACE_2:
            workspaceSwitch(1, NULL);
            break;
        case KEY_WORKSPACE_3:
            workspaceSwitch(2, NULL);
            break;
        case KEY_WORKSPACE_4:
            workspaceSwitch(3, NULL);
            break;
        case KEY_WORKSPACE_5:
            workspaceSwitch(4, NULL);
            break;
        case KEY_WORKSPACE_6:
            workspaceSwitch(5, NULL);
            break;
        case KEY_WORKSPACE_7:
            workspaceSwitch(6, NULL);
            break;
        case KEY_WORKSPACE_8:
            workspaceSwitch(7, NULL);
            break;
        case KEY_WORKSPACE_9:
            workspaceSwitch(8, NULL);
            break;
        case KEY_SHORTCUT_1:
	    spawn_shortcut(0);
            break;
        case KEY_SHORTCUT_2:
	    spawn_shortcut(1);
            break;
        case KEY_SHORTCUT_3:
	    spawn_shortcut(2);
            break;
        case KEY_SHORTCUT_4:
	    spawn_shortcut(3);
            break;
        case KEY_SHORTCUT_5:
	    spawn_shortcut(4);
            break;
        case KEY_SHORTCUT_6:
	    spawn_shortcut(5);
            break;
        case KEY_SHORTCUT_7:
	    spawn_shortcut(6);
            break;
        case KEY_SHORTCUT_8:
	    spawn_shortcut(7);
            break;
        case KEY_SHORTCUT_9:
	    spawn_shortcut(8);
            break;
        case KEY_SHORTCUT_10:
	    spawn_shortcut(9);
            break;
	default:
	    break;
    }
    while(XCheckTypedEvent(dpy, EnterNotify, &e));
}

/* User has clicked on an edge or corner.
 * Button 1 : Raise and resize
 * Button 2 : Move
 * Button 3 : Resize
 */
static inline void _edgeButton(Client * c, int part, XButtonEvent * ev)
{
    if(ev->button == Button2)
    {
        XfwmButtonClickType tclick;

        tclick = typeOfClick(c->frame, (XEvent *) ev, FALSE);

        if(tclick == XFWM_BUTTON_CLICK)
        {
            clientLower(c);
        }
        else
        {
            _moveRequest(c, (XEvent *) ev);
        }
    }
    else
    {
        if(ev->button == Button1)
            clientRaise(c);
        if(ev->button == Button1 || ev->button == Button3)
            _resizeRequest(c, part, (XEvent *) ev);
    }
}

static inline void handleButtonPress(XButtonEvent * ev)
{
    Client *c;
    Window win;
    int state, replay = False;

    DBG("entering handleButtonPress\n");

    /* Clear timeout */
    clear_timeout();

    c = clientGetFromWindow(ev->window, ANY);
    if(c)
    {
        state = ev->state & (ShiftMask | ControlMask | AltMask | MetaMask);
        win = ev->subwindow;

        if((win == c->buttons[HIDE_BUTTON]) || (win == c->buttons[CLOSE_BUTTON]) || (win == c->buttons[MAXIMIZE_BUTTON]) || (win == c->buttons[SHADE_BUTTON]) || (win == c->buttons[STICK_BUTTON]))
        {
            clientSetFocus(c, True);
            if(params.raise_on_click)
            {
                clientRaise(c);
            }
            clientButtonPress(c, win, ev);
        }
        else if(((win == c->title) && (ev->button == Button3)) || ((win == c->buttons[MENU_BUTTON]) && (ev->button == Button1)))
        {
            /*
               We need to copy the event to keep the original event untouched
               for gtk to handle it (in case we open up the menu)
             */
            XEvent copy_event = (XEvent) * ev;
            XfwmButtonClickType tclick;

            tclick = typeOfClick(c->frame, &copy_event, win == c->buttons[MENU_BUTTON]);

            if(tclick == XFWM_BUTTON_DOUBLE_CLICK)
            {
                clientClose(c);
            }
            else if(ev->button == Button3 && (tclick == XFWM_BUTTON_DRAG))
            {
                _moveRequest(c, (XEvent *) ev);
            }
            else
            {
                clientSetFocus(c, True);
                if(params.raise_on_click)
                {
                    clientRaise(c);
                }
                ev->window = ev->root;
                if(button_handler_id)
                {
                    g_signal_handler_disconnect(GTK_OBJECT(getDefaultGtkWidget()), button_handler_id);
                }
                button_handler_id = g_signal_connect(GTK_OBJECT(getDefaultGtkWidget()), "button_press_event", GTK_SIGNAL_FUNC(show_popup_cb), (gpointer) c);
                /* Let GTK handle this for us. */
            }
        }
        else if(((win == c->title) && ((ev->button == Button1) && (state == 0))) || ((ev->button == Button1) && (state == AltMask)))
        {
            XEvent copy_event = (XEvent) * ev;
            XfwmButtonClickType tclick;

            clientSetFocus(c, True);
            clientRaise(c);
            tclick = typeOfClick(c->frame, &copy_event, TRUE);
            if((tclick == XFWM_BUTTON_DRAG) || (tclick == XFWM_BUTTON_CLICK_AND_DRAG))
            {
                _moveRequest(c, (XEvent *) ev);
            }
            else if(tclick == XFWM_BUTTON_DOUBLE_CLICK)
            {
                switch (params.double_click_action)
                {
                    case ACTION_MAXIMIZE:
                        if(CLIENT_CAN_MAXIMIZE_WINDOW(c))
                        {
                            clientToggleMaximized(c, WIN_STATE_MAXIMIZED);
                        }
                        break;
                    case ACTION_SHADE:
                        clientToggleShaded(c);
                        break;
                    case ACTION_HIDE:
                        if(CLIENT_CAN_HIDE_WINDOW(c))
                        {
                            clientHide(c, True);
                        }
                        break;
                }
            }
        }
        else if((win == c->corners[CORNER_TOP_LEFT]) && (state == 0))
        {
            _edgeButton(c, CORNER_TOP_LEFT, ev);
        }
        else if((win == c->corners[CORNER_TOP_RIGHT]) && (state == 0))
        {
            _edgeButton(c, CORNER_TOP_RIGHT, ev);
        }
        else if((win == c->corners[CORNER_BOTTOM_LEFT]) && (state == 0))
        {
            _edgeButton(c, CORNER_BOTTOM_LEFT, ev);
        }
        else if((win == c->corners[CORNER_BOTTOM_RIGHT]) && (state == 0))
        {
            _edgeButton(c, CORNER_BOTTOM_RIGHT, ev);
        }
        else if((win == c->sides[SIDE_BOTTOM]) && (state == 0))
        {
            _edgeButton(c, 4 + SIDE_BOTTOM, ev);
        }
        else if((win == c->sides[SIDE_LEFT]) && (state == 0))
        {
            _edgeButton(c, 4 + SIDE_LEFT, ev);
        }
        else if((win == c->sides[SIDE_RIGHT]) && (state == 0))
        {
            _edgeButton(c, 4 + SIDE_RIGHT, ev);
        }
        else if(((ev->window != c->window) && (ev->button == Button2) && (state == 0)) || ((ev->button == Button2) && (state == (AltMask | ControlMask))))
        {
            clientLower(c);
        }
        else
        {
            if(ev->button == Button1)
            {
                clientSetFocus(c, True);
                if(params.raise_on_click)
                    clientRaise(c);
            }
            if(ev->window == c->window)
            {
                replay = True;
            }
        }

        if(replay)
        {
            XAllowEvents(dpy, ReplayPointer, ev->time);
        }
        else
        {
            XAllowEvents(dpy, SyncPointer, ev->time);
        }
    }
    else
    {
        XUngrabPointer(dpy, CurrentTime);
        XSendEvent(dpy, gnome_win, False, SubstructureNotifyMask, (XEvent *) ev);
    }
}

static inline void handleButtonRelease(XButtonEvent * ev)
{
    DBG("entering handleButtonRelease\n");

    XSendEvent(dpy, gnome_win, False, SubstructureNotifyMask, (XEvent *) ev);
}

static inline void handleDestroyNotify(XDestroyWindowEvent * ev)
{
    Client *c;

    DBG("entering handleDestroyNotify\n");

    c = clientGetFromWindow(ev->window, WINDOW);
    if(c)
    {
        clientUnframe(c, False);
        if(clients)
        {
            clientSetFocus(clientGetNext(clients->prev, 0), True);
        }
        else
        {
            clientSetFocus(NULL, True);
        }
    }
}

static inline void handleUnmapNotify(XUnmapEvent * ev)
{
    Client *c;

    DBG("entering handleUnmapNotify\n");

    c = clientGetFromWindow(ev->window, WINDOW);
    if(c)
    {
        if(c->ignore_unmap)
        {
            c->ignore_unmap--;
        }
        else
        {
            clientUnframe(c, False);
            if(clients)
            {
                clientSetFocus(clientGetNext(clients->prev, 0), True);
            }
            else
            {
                clientSetFocus(NULL, True);
            }
        }
    }
}

static inline void handleMapRequest(XMapRequestEvent * ev)
{
    Client *c;

    DBG("entering handleMapRequest\n");

    if(ev->window == None)
    {
        DBG("Mapping None ???\n");
        return;
    }
    c = clientGetFromWindow(ev->window, WINDOW);
    if(c)
    {
        clientShow(c, True);
    }
    else
    {
        clientFrame(ev->window);
    }
}

static inline void handleConfigureRequest(XConfigureRequestEvent * ev)
{
    Client *c;
    XWindowChanges wc;
    XEvent otherEvent;

    DBG("entering handleConfigureRequest\n");

    /* Compress events - logic taken from kwin */
    while (XCheckTypedWindowEvent (dpy, ev->window, ConfigureRequest, &otherEvent)) 
    {
        if (otherEvent.xconfigurerequest.value_mask == ev->value_mask)
	{
            ev = &otherEvent.xconfigurerequest;
        }
	else 
	{
            XPutBackEvent(dpy, &otherEvent);
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

    c = clientGetFromWindow(ev->window, WINDOW);
    if(c)
    {
        if (CLIENT_FLAG_TEST(c, CLIENT_FLAG_MOVING | CLIENT_FLAG_RESIZING))
	{
	    /* Sorry, but it's not the right time for configure request */
	    return;
	}
        if(c->type == WINDOW_DESKTOP)
        {
            /* Ignore stacking request for DESKTOP windows */
            wc.stack_mode &= ~CWStackMode;
        }
        clientCoordGravitate(c, APPLY, &wc.x, &wc.y);
        if((ev->value_mask & (CWX | CWY | CWWidth | CWHeight)) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED))
        {
            clientRemoveMaximizeFlag(c);
        }
        clientConfigure(c, &wc, ev->value_mask);
    }
    else
    {
        XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
    }
}

static inline void handleEnterNotify(XCrossingEvent * ev)
{
    Client *c;

    DBG("entering handleEnterNotify\n");

    while(XCheckTypedEvent(dpy, EnterNotify, (XEvent *) ev));

    DBG("EnterNotify window is (%#lx)\n", ev->window);

    c = clientGetFromWindow(ev->window, FRAME);
    if(c && !(params.click_to_focus) && (clientAcceptFocus(c)))
    {
        DBG("EnterNotify window is \"%s\"\n", c->name);
        if((c->type != WINDOW_DOCK) && (c->type != WINDOW_DESKTOP))
        {
            clientSetFocus(c, True);
        }
    }
}

static inline void handleFocusIn(XFocusChangeEvent * ev)
{
    Client *c;

    DBG("entering handleFocusIn\n");
    DBG("FocusIn window is (%#lx)\n", ev->window);

    if(ev->window == gnome_win)
    {
        /* Don't get fooled by our own gtk window ! */
        return;
    }

    c = clientGetFromWindow(ev->window, WINDOW);
    if(c)
    {
        DBG("focus set to \"%s\" (%#lx)\n", c->name, c->window);
        clientUpdateFocus(c);
        frameDraw(c);
        if(params.raise_on_focus && !params.click_to_focus)
        {
            reset_timeout();
        }
    }
    else if(clients)
    {
        DBG("focus set to top window in list\n");
        clientSetFocus(clientGetNext(clients->prev, 0), True);
    }
    else
    {
        DBG("focus set to default fallback window\n");
        clientSetFocus(NULL, True);
    }
}

static inline void handleFocusOut(XFocusChangeEvent * ev)
{
    DBG("entering handleFocusOut\n");
}

static inline void handlePropertyNotify(XPropertyEvent * ev)
{
    Client *c;
    long dummy;

    DBG("entering handlePropertyNotify\n");

    c = clientGetFromWindow(ev->window, WINDOW);
    if(c)
    {
        if(ev->atom == XA_WM_NORMAL_HINTS)
        {
            unsigned long previous_value;

            DBG("client \"%s\" (%#lx) has received a XA_WM_NORMAL_HINTS notify\n", c->name, c->window);
            XGetWMNormalHints(dpy, c->window, c->size, &dummy);
            previous_value = (c->client_flag & CLIENT_FLAG_IS_RESIZABLE);
            if(!(c->size->flags & (PMinSize | PMaxSize)) || ((c->size->flags & (PMinSize | PMaxSize)) && ((c->size->min_width != c->size->max_width) || (c->size->min_height != c->size->max_height))))
            {
                CLIENT_FLAG_SET(c, CLIENT_FLAG_IS_RESIZABLE);
            }
            if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_IS_RESIZABLE) != previous_value)
            {
                frameDraw(c);
            }
        }
        else if((ev->atom == XA_WM_NAME) || (ev->atom == net_wm_name))
        {
            DBG("client \"%s\" (%#lx) has received a XA_WM_NAME notify\n", c->name, c->window);
            if(c->name)
            {
                free(c->name);
            }
            getWindowName(dpy, c->window, &c->name);
            frameDraw(c);
        }
        else if(ev->atom == motif_wm_hints)
        {
            DBG("client \"%s\" (%#lx) has received a motif_wm_hints notify\n", c->name, c->window);
            clientUpdateMWMHints(c);
            frameDraw(c);
        }
        else if(ev->atom == win_hints)
        {
            DBG("client \"%s\" (%#lx) has received a win_hints notify\n", c->name, c->window);
            getGnomeHint(dpy, c->window, win_hints, &c->win_hints);
        }
        else if(ev->atom == win_layer)
        {
            DBG("client \"%s\" (%#lx) has received a win_layer notify\n", c->name, c->window);
            getGnomeHint(dpy, c->window, win_layer, &dummy);
            clientSetLayer(c, dummy);
            clientSetNetState(c);
        }
        else if(ev->atom == net_wm_window_type)
        {
            DBG("client \"%s\" (%#lx) has received a net_wm_window_type notify\n", c->name, c->window);
            clientGetNetWmType(c);
            frameDraw(c);
        }
        else if((ev->atom == win_workspace) && !(c->transient_for))
        {
            DBG("client \"%s\" (%#lx) has received a win_workspace notify\n", c->name, c->window);
            getGnomeHint(dpy, c->window, win_workspace, &dummy);
            clientSetWorkspace(c, dummy, TRUE);
        }
        else if(ev->atom == net_wm_strut)
        {
            DBG("client \"%s\" (%#lx) has received a net_wm_strut notify\n", c->name, c->window);
            clientGetNetStruts(c);
        }
        else if(ev->atom == wm_colormap_windows)
        {
            clientUpdateColormaps(c);
            if(c == clientGetFocus())
            {
                clientInstallColormaps(c);
            }
        }
    }
    else
    {
        if(ev->atom == win_workspace_count)
        {
            DBG("root has received a win_workspace_count notify\n");
            getGnomeHint(dpy, root, win_workspace_count, &dummy);
            workspaceSetCount(dummy);
        }
        else if(ev->atom == gnome_panel_desktop_area)
        {
            DBG("root has received a gnome_panel_desktop_area notify\n");
            getGnomeDesktopMargins(dpy, screen, gnome_margins);
            workspaceUpdateArea(margins, gnome_margins);
        }
    }
}

static inline void handleClientMessage(XClientMessageEvent * ev)
{
    Client *c;

    DBG("entering handleClientMessage\n");

    c = clientGetFromWindow(ev->window, WINDOW);
    if(c)
    {
        if((ev->message_type == wm_change_state) && (ev->format == 32) && (ev->data.l[0] == IconicState))
        {
            DBG("client \"%s\" (%#lx) has received a wm_change_state event\n", c->name, c->window);
            if(CLIENT_CAN_HIDE_WINDOW(c))
            {
                clientHide(c, True);
            }
        }
        else if((ev->message_type == win_state) && (ev->format == 32) && (ev->data.l[0] & WIN_STATE_SHADED))
        {
            DBG("client \"%s\" (%#lx) has received a win_state/shaded event\n", c->name, c->window);
	    if (ev->data.l[1] == WIN_STATE_SHADED)
	    {
                clientShade(c);
	    }
	    else
	    {
                clientUnshade(c);
	    }
            clientToggleShaded(c);
        }
        else if((ev->message_type == win_state) && (ev->format == 32) && (ev->data.l[0] & WIN_STATE_STICKY))
        {
            DBG("client \"%s\" (%#lx) has received a win_state/stick event\n", c->name, c->window);
	    if (ev->data.l[1] == WIN_STATE_STICKY)
	    {
                clientStick(c, TRUE);
	    }
	    else
	    {
                clientUnstick(c, TRUE);
	    }
        }
        else if((ev->message_type == win_layer) && (ev->format == 32))
        {
            DBG("client \"%s\" (%#lx) has received a win_layer event\n", c->name, c->window);
            clientSetLayer(c, ev->data.l[0]);
        }
        else if((ev->message_type == win_workspace) && (ev->format == 32) && !(c->transient_for))
        {
            DBG("client \"%s\" (%#lx) has received a win_workspace event\n", c->name, c->window);
            clientSetWorkspace(c, ev->data.l[0], TRUE);
        }
        else if((ev->message_type == net_wm_desktop) && (ev->format == 32))
        {
            DBG("client \"%s\" (%#lx) has received a net_wm_desktop event\n", c->name, c->window);
            if((ev->data.l[0] == (int)0xFFFFFFFF))
            {
                clientStick(c, TRUE);
            }
            else if(!(c->transient_for))
            {
                clientSetWorkspace(c, ev->data.l[0], TRUE);
            }
        }
        else if((ev->message_type == net_close_window) && (ev->format == 32))
        {
            DBG("client \"%s\" (%#lx) has received a net_close_window event\n", c->name, c->window);
            clientClose(c);
        }
        else if((ev->message_type == net_wm_state) && (ev->format == 32))
        {
            DBG("client \"%s\" (%#lx) has received a net_wm_state event\n", c->name, c->window);
            clientUpdateNetState(c, ev);
        }
        else if((ev->message_type == net_wm_moveresize) && (ev->format == 32))
        {
            DBG("client \"%s\" (%#lx) has received a net_wm_moveresize event\n", c->name, c->window);
            g_message(_("%s: Operation not supported (yet)\n"), g_get_prgname());
            /* TBD */
        }
        else if((ev->message_type == net_active_window) && (ev->format == 32))
        {
            DBG("client \"%s\" (%#lx) has received a net_active_window event\n", c->name, c->window);
            workspaceSwitch(c->win_workspace, NULL);
            clientShow(c, True);
            clientRaise(c);
            clientSetFocus(c, True);
        }
    }
    else
    {
        if(((ev->message_type == win_workspace) || (ev->message_type == net_current_desktop)) && (ev->format == 32))
        {
            DBG("root has received a win_workspace or a net_current_desktop event\n");
            workspaceSwitch(ev->data.l[0], NULL);
        }
        else if(((ev->message_type == win_workspace_count) || (ev->message_type == net_number_of_desktops)) && (ev->format == 32))
        {
            DBG("root has received a win_workspace_count event\n");
            workspaceSetCount(ev->data.l[0]);
        }
    }
}

static inline void handleShape(XShapeEvent * ev)
{
    Client *c;

    DBG("entering handleShape\n");

    c = clientGetFromWindow(ev->window, WINDOW);
    if(c)
    {
        frameDraw(c);
    }
}

static inline void handleColormapNotify(XColormapEvent * ev)
{
    Client *c;

    DBG("entering handleColormapNotify\n");

    c = clientGetFromWindow(ev->window, WINDOW);
    if((c) && (ev->window == c->window) && (ev->new))
    {
        if(c == clientGetFocus())
        {
            clientInstallColormaps(c);
        }
    }
}

void handleEvent(XEvent * ev)
{
    DBG("entering handleEvent\n");

    switch (ev->type)
    {
        case KeyPress:
            handleKeyPress((XKeyEvent *) ev);
            break;
        case ButtonPress:
            handleButtonPress((XButtonEvent *) ev);
            break;
        case ButtonRelease:
            handleButtonRelease((XButtonEvent *) ev);
            break;
        case DestroyNotify:
            handleDestroyNotify((XDestroyWindowEvent *) ev);
            break;
        case UnmapNotify:
            handleUnmapNotify((XUnmapEvent *) ev);
            break;
        case MapRequest:
            handleMapRequest((XMapRequestEvent *) ev);
            break;
        case ConfigureRequest:
            handleConfigureRequest((XConfigureRequestEvent *) ev);
            break;
        case EnterNotify:
            handleEnterNotify((XCrossingEvent *) ev);
            break;
        case FocusIn:
            handleFocusIn((XFocusChangeEvent *) ev);
            break;
        case FocusOut:
            handleFocusOut((XFocusChangeEvent *) ev);
            break;
        case PropertyNotify:
            handlePropertyNotify((XPropertyEvent *) ev);
            break;
        case ClientMessage:
            handleClientMessage((XClientMessageEvent *) ev);
            break;
        case ColormapNotify:
            handleColormapNotify((XColormapEvent *) ev);
            break;
        default:
            if(shape && (ev->type == shape_event))
            {
                handleShape((XShapeEvent *) ev);
            }
    }
    if(!gdk_events_pending() && !XPending(dpy))
    {
        if(reload)
        {
            reloadSettings(UPDATE_ALL);
            reload = False;
        }
        else if(quit)
        {
            gtk_main_quit();
        }
    }
}


GtkToXEventFilterStatus xfwm4_event_filter(XEvent * xevent, gpointer data)
{
    DBG("entering xfwm4_event_filter\n");
    handleEvent(xevent);
    DBG("leaving xfwm4_event_filter\n");
    return XEV_FILTER_STOP;
}

/* GTK stuff (menu, etc...) */

static void menu_callback(Menu * menu, MenuOp op, Window client_xwindow, gpointer menu_data, gpointer item_data)
{
    Client *c = NULL;

    DBG("entering menu_callback\n");

    if(menu_event_window)
    {
        removeTmpEventWin(menu_event_window);
        menu_event_window = None;
    }

    if(menu_data)
    {
        c = (Client *) menu_data;
        c = clientGetFromWindow(c->window, WINDOW);
        if(c)
        {
            c->button_pressed[MENU_BUTTON] = False;
        }
    }

    switch (op)
    {
        case MENU_OP_QUIT:
            gtk_main_quit();
            break;
        case MENU_OP_MAXIMIZE:
        case MENU_OP_UNMAXIMIZE:
            if((c) && CLIENT_CAN_MAXIMIZE_WINDOW(c))
            {
                clientToggleMaximized(c, WIN_STATE_MAXIMIZED);
            }
            break;
        case MENU_OP_MINIMIZE:
            if((c) && CLIENT_CAN_HIDE_WINDOW(c))
            {
                clientHide(c, True);
            }
            break;
        case MENU_OP_MINIMIZE_ALL:
            clientHideAll(c);
            break;
        case MENU_OP_UNMINIMIZE:
            if(c)
            {
                clientShow(c, True);
            }
            break;
        case MENU_OP_SHADE:
        case MENU_OP_UNSHADE:
            if(c)
            {
                clientToggleShaded(c);
            }
            break;
        case MENU_OP_STICK:
        case MENU_OP_UNSTICK:
            if(c)
            {
                frameDraw(c);
                clientToggleSticky(c, TRUE);
            }
            break;
        case MENU_OP_DELETE:
            if(c)
            {
                frameDraw(c);
                clientClose(c);
            }
            break;
        default:
            if(c)
            {
                frameDraw(c);
            }
            break;
    }
    menu_free(menu);
}

static gboolean show_popup_cb(GtkWidget * widget, GdkEventButton * ev, gpointer data)
{
    Menu *menu;
    MenuOp ops;
    MenuOp insensitive;
    Client *c = NULL;
    gint x = ev->x_root;
    gint y = ev->y_root;

    DBG("entering show_popup_cb\n");

    if(((ev->button == 1) || (ev->button == 3)) && (c = (Client *) data))
    {
        c->button_pressed[MENU_BUTTON] = True;
        frameDraw(c);
        y = c->y;
        ops = MENU_OP_DELETE | MENU_OP_MINIMIZE_ALL;
        insensitive = 0;

        if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_CLOSE))
        {
            insensitive |= MENU_OP_DELETE;
        }

        if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED))
        {
            ops |= MENU_OP_UNMAXIMIZE;
            if(!CLIENT_CAN_MAXIMIZE_WINDOW(c))
            {
                insensitive |= MENU_OP_UNMAXIMIZE;
            }
        }
        else
        {
            ops |= MENU_OP_MAXIMIZE;
            if(!CLIENT_CAN_MAXIMIZE_WINDOW(c))
            {
                insensitive |= MENU_OP_MAXIMIZE;
            }
        }

        if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_HIDDEN))
        {
            ops |= MENU_OP_UNMINIMIZE;
            if(!CLIENT_CAN_HIDE_WINDOW(c))
            {
                insensitive |= MENU_OP_UNMINIMIZE;
            }
        }
        else
        {
            ops |= MENU_OP_MINIMIZE;
            if(!CLIENT_CAN_HIDE_WINDOW(c))
            {
                insensitive |= MENU_OP_MINIMIZE;
            }
        }

        if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_SHADED))
        {
            ops |= MENU_OP_UNSHADE;
        }
        else
        {
            ops |= MENU_OP_SHADE;
        }

        if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_STICKY))
        {
            ops |= MENU_OP_UNSTICK;
            if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_MENU))
            {
                insensitive |= MENU_OP_UNSTICK;
            }
        }
        else
        {
            ops |= MENU_OP_STICK;
            if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_MENU))
            {
                insensitive |= MENU_OP_STICK;
            }
        }
    }
    else
    {
        return (TRUE);
    }

    if(button_handler_id)
    {
        g_signal_handler_disconnect(GTK_OBJECT(getDefaultGtkWidget()), button_handler_id);
    }
    button_handler_id = g_signal_connect(GTK_OBJECT(getDefaultGtkWidget()), "button_press_event", GTK_SIGNAL_FUNC(show_popup_cb), (gpointer) NULL);

    if(menu_event_window)
    {
        removeTmpEventWin(menu_event_window);
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
    menu_event_window = setTmpEventWin(NoEventMask);
    menu = menu_default(ops, insensitive, menu_callback, c);
    if(!menu_popup(menu, x, y, ev->button, ev->time))
    {
        DBG("Cannot open menu\n");
        gdk_beep();
        c->button_pressed[MENU_BUTTON] = False;
        frameDraw(c);
        removeTmpEventWin(menu_event_window);
        menu_event_window = None;
        menu_free(menu);
    }
    return (TRUE);
}

static gboolean set_reload(void)
{
    DBG("setting reload flag so all prefs will be reread at next event loop\n");
    reload = True;
    return (TRUE);
}

static gboolean dbl_click_time(void)
{
    GValue tmp_val = { 0, };

    DBG("setting dbl_click_time\n");

    g_value_init(&tmp_val, G_TYPE_INT);
    if(gdk_setting_get("gtk-double-click-time", &tmp_val))
    {
        params.dbl_click_time = abs(g_value_get_int(&tmp_val));
    }

    return (TRUE);
}

static gboolean client_event_cb(GtkWidget * widget, GdkEventClient * ev)
{
    DBG("entering client_event_cb\n");

    if(!atom_rcfiles)
    {
        atom_rcfiles = gdk_atom_intern("_GTK_READ_RCFILES", FALSE);
    }

    if(ev->message_type == atom_rcfiles)
    {
        set_reload();
    }

    return (FALSE);
}

void initGtkCallbacks(void)
{
    GtkSettings *settings;

    button_handler_id = g_signal_connect(GTK_OBJECT(getDefaultGtkWidget()), "button_press_event", GTK_SIGNAL_FUNC(show_popup_cb), (gpointer) NULL);
    g_signal_connect(GTK_OBJECT(getDefaultGtkWidget()), "client_event", GTK_SIGNAL_FUNC(client_event_cb), (gpointer) NULL);

    settings = gtk_settings_get_default();
    if(settings)
    {
        g_signal_connect(settings, "notify::gtk-theme-name", G_CALLBACK(set_reload), NULL);
        g_signal_connect(settings, "notify::gtk-font-name", G_CALLBACK(set_reload), NULL);
        g_signal_connect(settings, "notify::gtk-double-click-time", G_CALLBACK(dbl_click_time), NULL);
    }
}
