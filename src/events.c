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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include "main.h"
#include "workspaces.h"
#include "settings.h"
#include "frame.h"
#include "client.h"
#include "menu.h"
#include "gtktoxevent.h"
#include "debug.h"

static guint raise_timeout = 0;
static gulong button_handler_id = 0;
static GdkAtom atom_rcfiles = GDK_NONE;

static void menu_callback(Menu * menu, MenuOp op, Window client_xwindow, gpointer menu_data, gpointer item_data);
static gboolean show_popup_cb(GtkWidget * widget, GdkEventButton * ev, gpointer data);
static gboolean client_event_cb(GtkWidget * widget, GdkEventClient * ev);

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
    raise_timeout = gtk_timeout_add(raise_delay, (GtkFunction) raise_cb, NULL);
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
        if((keys[key].keycode == ev->keycode) && (keys[key].modifier == state))
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
                if((c->has_border) && !(c->fullscreen))
                {
                    clientMove(c, (XEvent *) ev);
                }
                break;
            case KEY_RESIZE_UP:
            case KEY_RESIZE_DOWN:
            case KEY_RESIZE_LEFT:
            case KEY_RESIZE_RIGHT:
                clientResize(c, CORNER_BOTTOM_RIGHT, (XEvent *) ev);
                break;
            case KEY_CYCLE_WINDOWS:
                clientCycle(c);
                break;
            case KEY_CLOSE_WINDOW:
                clientClose(c);
                break;
            case KEY_HIDE_WINDOW:
                clientHide(c, True);
                break;
            case KEY_MAXIMIZE_WINDOW:
                clientToggleMaximized(c, WIN_STATE_MAXIMIZED);
                break;
            case KEY_MAXIMIZE_VERT:
                clientToggleMaximized(c, WIN_STATE_MAXIMIZED_VERT);
                break;
            case KEY_MAXIMIZE_HORIZ:
                clientToggleMaximized(c, WIN_STATE_MAXIMIZED_HORIZ);
                break;
            case KEY_SHADE_WINDOW:
                clientToggleShaded(c);
                break;
            case KEY_RAISE_WINDOW_LAYER:
                clientSetLayer(c, c->win_layer + 1);
                break;
            case KEY_LOWER_WINDOW_LAYER:
                clientSetLayer(c, c->win_layer - 1);
                break;
            case KEY_NEXT_WORKSPACE:
                workspaceSwitch(workspace + 1, NULL);
                break;
            case KEY_PREV_WORKSPACE:
                workspaceSwitch(workspace - 1, NULL);
                break;
            case KEY_ADD_WORKSPACE:
                workspaceSetCount(workspace_count + 1);
                break;
            case KEY_DEL_WORKSPACE:
                workspaceSetCount(workspace_count - 1);
                break;
            case KEY_STICK_WINDOW:
                clientToggleSticky(c);
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
            case KEY_NEXT_WORKSPACE:
                workspaceSwitch(workspace + 1, NULL);
                break;
            case KEY_PREV_WORKSPACE:
                workspaceSwitch(workspace - 1, NULL);
                break;
            case KEY_ADD_WORKSPACE:
                workspaceSetCount(workspace_count + 1);
                break;
            case KEY_DEL_WORKSPACE:
                workspaceSetCount(workspace_count - 1);
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
        }
    }

    while(XCheckTypedEvent(dpy, EnterNotify, &e));
}

static inline void handleButtonPress(XButtonEvent * ev)
{
    Client *c;
    Window win;
    int state, replay = False;
    static Time last_button_time;

    DBG("entering handleButtonPress\n");

    /* Clear timeout */
    clear_timeout();

    c = clientGetFromWindow(ev->window, FRAME);
    if(c)
    {

        state = ev->state & (ShiftMask | ControlMask | AltMask | MetaMask);
        win = getMouseWindow(dpy, c->frame);

        clientSetFocus(c, True);

        if((win == c->buttons[HIDE_BUTTON]) || (win == c->buttons[CLOSE_BUTTON]) || (win == c->buttons[MAXIMIZE_BUTTON]) || (win == c->buttons[SHADE_BUTTON]) || (win == c->buttons[STICK_BUTTON]))
        {
            clientRaise(c);
            clientButtonPress(c, win, ev);
        }
        else if(((win == c->title) && (ev->button == Button3)) || ((win == c->buttons[MENU_BUTTON]) && (ev->button == Button1)))
        {
            clientRaise(c);
            ev->window = ev->root;
            if(button_handler_id)
            {
                g_signal_handler_disconnect(GTK_OBJECT(getDefaultGtkWidget()), button_handler_id);
            }
            button_handler_id = g_signal_connect(GTK_OBJECT(getDefaultGtkWidget()), "button_press_event", GTK_SIGNAL_FUNC(show_popup_cb), (gpointer) c);
            /* Let GTK handle this for us. */
        }
        else if(((win == c->title) && ((ev->button == Button1) && (state == 0))) || ((ev->button == Button1) && (state == AltMask)))
        {
            clientRaise(c);
            if((ev->time - last_button_time <= 250) && (last_button_time != 0))
            {
                switch (double_click_action)
                {
                    case ACTION_MAXIMIZE:
                        clientToggleMaximized(c, WIN_STATE_MAXIMIZED);
                        break;
                    case ACTION_SHADE:
                        clientToggleShaded(c);
                        break;
                    case ACTION_HIDE:
                        clientHide(c, True);
                        break;
                }
                last_button_time = 0;
            }
            else
            {
                if((c->has_border) && !(c->fullscreen))
                {
                    clientMove(c, (XEvent *) ev);
                }
                last_button_time = ev->time;
            }
        }
        else if((win == c->corners[CORNER_TOP_LEFT]) && (ev->button == Button1) && (state == 0))
        {
            clientRaise(c);
            clientResize(c, CORNER_TOP_LEFT, (XEvent *) ev);
        }
        else if((win == c->corners[CORNER_TOP_RIGHT]) && (ev->button == Button1) && (state == 0))
        {
            clientRaise(c);
            clientResize(c, CORNER_TOP_RIGHT, (XEvent *) ev);
        }
        else if((win == c->corners[CORNER_BOTTOM_LEFT]) && (ev->button == Button1) && (state == 0))
        {
            clientRaise(c);
            clientResize(c, CORNER_BOTTOM_LEFT, (XEvent *) ev);
        }
        else if((win == c->corners[CORNER_BOTTOM_RIGHT]) && (ev->button == Button1) && (state == 0))
        {
            clientRaise(c);
            clientResize(c, CORNER_BOTTOM_RIGHT, (XEvent *) ev);
        }
        else if((win == c->sides[SIDE_BOTTOM]) && (ev->button == Button1) && (state == 0))
        {
            clientRaise(c);
            clientResize(c, 4 + SIDE_BOTTOM, (XEvent *) ev);
        }
        else if((win == c->sides[SIDE_LEFT]) && (ev->button == Button1) && (state == 0))
        {
            clientRaise(c);
            clientResize(c, 4 + SIDE_LEFT, (XEvent *) ev);
        }
        else if((win == c->sides[SIDE_RIGHT]) && (ev->button == Button1) && (state == 0))
        {
            clientRaise(c);
            clientResize(c, 4 + SIDE_RIGHT, (XEvent *) ev);
        }
        else if(((win != c->window) && (ev->button == Button2) && (state == 0)) || ((ev->button == Button2) && (state == (AltMask | ControlMask))))
        {
            clientLower(c);
        }
        else
        {
            clientRaise(c);
            if(win == c->window)
            {
                replay = True;
            }
        }

        if(replay)
        {
            XAllowEvents(dpy, ReplayPointer, CurrentTime);
        }
        else
        {
            XAllowEvents(dpy, SyncPointer, CurrentTime);
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

    DBG("entering handleConfigureRequest\n");

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
        if (c->type == WINDOW_DESKTOP)
	{
	    /* Ignore stacking request for DESKTOP windows */
	    wc.stack_mode &= ~CWStackMode;
	}
        clientCoordGravitate(c, APPLY, &wc.x, &wc.y);
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
    if(c && !(click_to_focus) && (clientAcceptFocus(c)))
    {
        DBG("EnterNotify window is \"%s\"\n", c->name);
        clientSetFocus(c, True);
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
        if(raise_on_focus)
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
            DBG("client \"%s\" (%#lx) has received a XA_WM_NORMAL_HINTS notify\n", c->name, c->window);
            XGetWMNormalHints(dpy, c->window, c->size, &dummy);
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
        }
        else if(ev->atom == win_workspace)
        {
            DBG("client \"%s\" (%#lx) has received a win_workspace notify\n", c->name, c->window);
            getGnomeHint(dpy, c->window, win_workspace, &dummy);
            clientSetWorkspace(c, dummy);
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
            getGnomeDesktopMargins(dpy, gnome_margins);
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
            clientHide(c, True);
        }
        else if((ev->message_type == win_state) && (ev->format == 32) && (ev->data.l[0] & WIN_STATE_SHADED))
        {
            DBG("client \"%s\" (%#lx) has received a win_state/shaded event\n", c->name, c->window);
            clientToggleShaded(c);
        }
        else if((ev->message_type == win_state) && (ev->format == 32) && (ev->data.l[0] & WIN_STATE_STICKY))
        {
            DBG("client \"%s\" (%#lx) has received a win_state/stick event\n", c->name, c->window);
            clientToggleSticky(c);
        }
        else if((ev->message_type == win_layer) && (ev->format == 32))
        {
            DBG("client \"%s\" (%#lx) has received a win_layer event\n", c->name, c->window);
            clientSetLayer(c, ev->data.l[0]);
        }
        else if((ev->message_type == win_workspace) && (ev->format == 32))
        {
            DBG("client \"%s\" (%#lx) has received a win_workspace event\n", c->name, c->window);
            clientSetWorkspace(c, ev->data.l[0]);
        }
        else if((ev->message_type == net_wm_desktop) && (ev->format == 32))
        {
            DBG("client \"%s\" (%#lx) has received a net_wm_desktop event\n", c->name, c->window);
            if((ev->data.l[0] == (int)0xFFFFFFFF))
            {
                clientStick(c);
            }
            else
            {
                clientSetWorkspace(c, ev->data.l[0]);
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
            g_message("Operation not supported (yet)\n");
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
            reloadSettings();
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
            if(c)
            {
                clientToggleMaximized(c, WIN_STATE_MAXIMIZED);
            }
            break;
        case MENU_OP_MINIMIZE:
            if(c)
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
                clientToggleSticky(c);
                frameDraw(c);
            }
            break;
        case MENU_OP_DELETE:
            if(c)
            {
                clientClose(c);
                frameDraw(c);
            }
            break;
        case MENU_OP_DESTROY:
            if(c)
            {
                clientKill(c);
                frameDraw(c);
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
        ops = MENU_OP_DELETE | MENU_OP_DESTROY | MENU_OP_MINIMIZE_ALL;
        insensitive = 0;

        if(c->win_state & (WIN_STATE_MAXIMIZED | WIN_STATE_MAXIMIZED_HORIZ | WIN_STATE_MAXIMIZED_VERT))
        {
            ops |= MENU_OP_UNMAXIMIZE;
        }
        else
        {
            ops |= MENU_OP_MAXIMIZE;
        }

        if(c->hidden)
        {
            ops |= MENU_OP_UNMINIMIZE;
        }
        else
        {
            ops |= MENU_OP_MINIMIZE;
        }

        if(c->win_state & WIN_STATE_SHADED)
        {
            ops |= MENU_OP_UNSHADE;
        }
        else
        {
            ops |= MENU_OP_SHADE;
        }

        if(c->sticky)
        {
            ops |= MENU_OP_UNSTICK;
        }
        else
        {
            ops |= MENU_OP_STICK;
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

    menu = menu_default(ops, insensitive, menu_callback, c);
    if(!menu_popup(menu, x, y, ev->button, ev->time))
    {
        DBG("Cannot open menu\n");
        gdk_beep();
        c->button_pressed[MENU_BUTTON] = False;
        frameDraw(c);
        menu_free(menu);
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
        DBG("setting reload flag so all prefs will be reread at next event loop\n");
        reload = True;
    }

    return (FALSE);
}

void initGtkCallbacks(void)
{
    GtkSettings *settings;
    
    button_handler_id = g_signal_connect(GTK_OBJECT(getDefaultGtkWidget()), "button_press_event", GTK_SIGNAL_FUNC(show_popup_cb), (gpointer) NULL);
    g_signal_connect (GTK_OBJECT(getDefaultGtkWidget()), "client_event", GTK_SIGNAL_FUNC(client_event_cb), (gpointer) NULL);

    settings = gtk_settings_get_default();
    g_signal_connect (settings, "notify::gtk-theme-name", G_CALLBACK (client_event_cb), NULL);
    g_signal_connect (settings, "notify::gtk-font-name", G_CALLBACK (client_event_cb), NULL);
}
