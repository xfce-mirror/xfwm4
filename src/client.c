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

#ifdef GDK_MULTIHEAD_SAFE
#undef GDK_MULTIHEAD_SAFE
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib.h>

#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfcegui4/libxfcegui4.h>

#include "main.h"
#include "client.h"
#include "frame.h"
#include "hints.h"
#include "workspaces.h"
#include "mypixmap.h"
#include "mywindow.h"
#include "settings.h"
#include "tabwin.h"
#include "session.h"
#include "startup_notification.h"

/* Event mask definition */

#define POINTER_EVENT_MASK      ButtonPressMask|\
    ButtonReleaseMask

#define FRAME_EVENT_MASK        SubstructureNotifyMask|\
    SubstructureRedirectMask|\
    EnterWindowMask

#define CLIENT_EVENT_MASK       FocusChangeMask|\
    PropertyChangeMask

/* Useful macros */
#define ACCEPT_INPUT(wmhints) \
    (!(params.focus_hint) || \
     ((!(wmhints) || \
       ((wmhints) && !(wmhints->flags & InputHint)) || \
       ((wmhints) && (wmhints->flags & InputHint) && (wmhints->input)))))

#define START_ICONIC(c) \
    ((c->wmhints) && \
     (c->wmhints->initial_state == IconicState) && \
     !(c->transient_for))

#define CONSTRAINED_WINDOW(c) \
    ((c->win_layer > WIN_LAYER_DESKTOP) && (c->win_layer < WIN_LAYER_ABOVE_DOCK) && !(c->type & (WINDOW_DESKTOP | WINDOW_DOCK)))

/* You don't like that ? Me either, but, hell, it's the way glib lists are designed */
#define XWINDOW_TO_GPOINTER(w)  ((gpointer) (Window) (w))
#define GPOINTER_TO_XWINDOW(p)  ((Window) (p))

Client *clients = NULL;
Client *last_raise = NULL;
unsigned int client_count = 0;

static GSList *windows = NULL;
static GSList *windows_stack = NULL;
static Client *client_focus = NULL;

/* Forward decl */
static void clientToggleFullscreen(Client * c);
static void clientToggleAbove(Client * c);
static void clientToggleBelow(Client * c);
static void clientGetNetState(Client * c);
static void clientGetInitialNetWmDesktop(Client * c);
static void clientSetNetClientList(Atom a, GSList * list);
static void clientSetNetActions(Client * c);
static void clientWindowType(Client * c);
static void clientAddToList(Client * c);
static void clientRemoveFromList(Client * c);
static int clientGetWidthInc(Client * c);
static int clientGetHeightInc(Client * c);
static void clientSetWidth(Client * c, int w1);
static void clientSetHeight(Client * c, int h1);
static inline void clientApplyStackList(GSList * list);
static inline Client *clientGetLowestTransient(Client * c);
#if 0 /* NEVER USED */
static inline Client *clientGetHighestTransient(Client * c);
#endif
static inline Client *clientGetNextTopMost(int layer, Client * exclude);
static inline Client *clientGetTopMostFocusable(int layer, Client * exclude);
static inline Client *clientGetBottomMost(int layer, Client * exclude);
static inline void clientConstraintPos(Client * c, gboolean show_full);
static inline void clientKeepVisible(Client * c);
static inline unsigned long overlap(int x0, int y0, int x1, int y1, int tx0, int ty0, int tx1, int ty1);
static void clientInitPosition(Client * c);
static inline void clientFree(Client * c);
static inline void clientApplyInitialNetState(Client * c);
static inline gboolean clientSelectMask(Client * c, int mask);
static GSList *clientListTransients(Client *c);
static inline void clientSetWorkspaceSingle(Client * c, int ws);
static inline void clientSnapPosition(Client * c);
static GtkToXEventFilterStatus clientMove_event_filter(XEvent * xevent, gpointer data);
static GtkToXEventFilterStatus clientResize_event_filter(XEvent * xevent, gpointer data);
static GtkToXEventFilterStatus clientCycle_event_filter(XEvent * xevent, gpointer data);
static GtkToXEventFilterStatus clientButtonPress_event_filter(XEvent * xevent, gpointer data);

typedef struct _MoveResizeData MoveResizeData;
struct _MoveResizeData
{
    gboolean use_keys;
    gboolean grab;
    int mx, my;
    int ox, oy;
    int oldw, oldh;
    int corner;
    Window tmp_event_window;
    Client *c;
};

typedef struct _ClientCycleData ClientCycleData;
struct _ClientCycleData
{
    Client *c;
    Tabwin *tabwin;
};

typedef struct _ButtonPressData ButtonPressData;
struct _ButtonPressData
{
    int b;
    Client *c;
};

static void clientToggleFullscreen(Client * c)
{
    XWindowChanges wc;
    int layer;

    g_return_if_fail(c != NULL);
    TRACE("entering clientToggleFullscreen");
    TRACE("toggle fullscreen client \"%s\" (0x%lx)", c->name, c->window);

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_FULLSCREEN))
    {
        int cx, cy;

        cx = frameX(c) + (frameWidth(c) >> 1);
        cy = frameY(c) + (frameHeight(c) >> 1);

        c->fullscreen_old_x = c->x;
        c->fullscreen_old_y = c->y;
        c->fullscreen_old_width = c->width;
        c->fullscreen_old_height = c->height;
        c->fullscreen_old_layer = c->win_layer;

        wc.x = MyDisplayX(cx, cy);
        wc.y = MyDisplayY(cx, cy);
        wc.width = MyDisplayWidth(dpy, screen, cx, cy);
        wc.height = MyDisplayHeight(dpy, screen, cx, cy);
        layer = WIN_LAYER_ABOVE_DOCK;
    }
    else
    {
        wc.x = c->fullscreen_old_x;
        wc.y = c->fullscreen_old_y;
        wc.width = c->fullscreen_old_width;
        wc.height = c->fullscreen_old_height;
        layer = c->fullscreen_old_layer;
    }
    clientSetNetState(c);
    clientSetLayer(c, layer);

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MANAGED))
    {
        clientConfigure(c, &wc, CWX | CWY | CWWidth | CWHeight, FALSE);
    }
    else
    {
        c->x = wc.x;
        c->y = wc.y;
        c->height = wc.height;
        c->width = wc.width;
    }
}

static void clientToggleAbove(Client * c)
{
    int layer;

    g_return_if_fail(c != NULL);
    TRACE("entering clientToggleAbove");
    TRACE("toggle above client \"%s\" (0x%lx)", c->name, c->window);

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_ABOVE))
    {
        layer = WIN_LAYER_ABOVE_DOCK;
    }
    else
    {
        layer = c->initial_layer;
    }
    clientSetNetState(c);
    clientSetLayer(c, layer);
}

static void clientToggleBelow(Client * c)
{
    int layer;

    g_return_if_fail(c != NULL);
    TRACE("entering clientToggleBelow");
    TRACE("toggle below client \"%s\" (0x%lx)", c->name, c->window);

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_BELOW))
    {
        layer = WIN_LAYER_BELOW;
    }
    else
    {
        layer = c->initial_layer;
    }
    clientSetNetState(c);
    clientSetLayer(c, layer);
}

void clientSetNetState(Client * c)
{
    int i;
    Atom data[16];

    g_return_if_fail(c != NULL);
    TRACE("entering clientSetNetState");
    TRACE("client \"%s\" (0x%lx)", c->name, c->window);

    i = 0;
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_SHADED))
    {
        TRACE("clientSetNetState : shaded");
        data[i++] = net_wm_state_shaded;
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_STICKY))
    {
        TRACE("clientSetNetState : sticky");
        data[i++] = net_wm_state_sticky;
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_STATE_MODAL))
    {
        TRACE("clientSetNetState : modal");
        data[i++] = net_wm_state_modal;
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_SKIP_PAGER))
    {
        TRACE("clientSetNetState : skip_pager");
        data[i++] = net_wm_state_skip_pager;
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_SKIP_TASKBAR))
    {
        TRACE("clientSetNetState : skip_taskbar");
        data[i++] = net_wm_state_skip_taskbar;
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED))
    {
        TRACE("clientSetNetState : maximize vert + horiz");
        data[i++] = net_wm_state_maximized_horz;
        data[i++] = net_wm_state_maximized_vert;
    }
    else if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED_HORIZ))
    {
        TRACE("clientSetNetState : maximize horiz");
        data[i++] = net_wm_state_maximized_horz;
    }
    else if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED_VERT))
    {
        TRACE("clientSetNetState : vert");
        data[i++] = net_wm_state_maximized_vert;
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE("clientSetNetState : fullscreen");
        data[i++] = net_wm_state_fullscreen;
    }
    else if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_ABOVE))
    {
        TRACE("clientSetNetState : above");
        data[i++] = net_wm_state_above;
    }
    else if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_BELOW))
    {
        TRACE("clientSetNetState : below");
        data[i++] = net_wm_state_below;
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_HIDDEN))
    {
        TRACE("clientSetNetState : hidden");
        data[i++] = net_wm_state_hidden;
    }
    XChangeProperty(dpy, c->window, net_wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char *)data, i);
    setNetHint(dpy, c->window, net_wm_desktop, (unsigned long)c->win_workspace);
    /* 
       We also set GNOME hint here for consistency and convenience, 
       although the meaning of net_wm_state and win_state aren't the same.
     */
    setGnomeHint(dpy, c->window, win_state, c->win_state);
}

static void clientGetNetState(Client * c)
{
    int n_atoms = 0;
    Atom *atoms = NULL;

    g_return_if_fail(c != NULL);
    TRACE("entering clientGetNetState");
    TRACE("client \"%s\" (0x%lx)", c->name, c->window);

    if(get_atom_list(dpy, c->window, net_wm_state, &atoms, &n_atoms))
    {
        int i;
        TRACE("clientGetNetState: %i atoms detected", n_atoms);

        i = 0;
        while(i < n_atoms)
        {
            if((atoms[i] == net_wm_state_shaded) || (CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_SESSION_MANAGED | CLIENT_FLAG_SHADED)))
            {
                TRACE("clientGetNetState : shaded");
                c->win_state |= WIN_STATE_SHADED;
                CLIENT_FLAG_SET(c, CLIENT_FLAG_SHADED);
            }
            else if(atoms[i] == net_wm_state_hidden)
            {
                TRACE("clientGetNetState : hidden");
                CLIENT_FLAG_SET(c, CLIENT_FLAG_HIDDEN);
            }
            else if((atoms[i] == net_wm_state_sticky) || (CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_SESSION_MANAGED | CLIENT_FLAG_STICKY)))
            {
                TRACE("clientGetNetState : sticky");
                c->win_state |= WIN_STATE_STICKY;
                CLIENT_FLAG_SET(c, CLIENT_FLAG_STICKY);
            }
            else if((atoms[i] == net_wm_state_maximized_horz) || (CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_SESSION_MANAGED | CLIENT_FLAG_MAXIMIZED_HORIZ)))
            {
                TRACE("clientGetNetState : maximized horiz");
                c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
                CLIENT_FLAG_SET(c, CLIENT_FLAG_MAXIMIZED_HORIZ);
            }
            else if((atoms[i] == net_wm_state_maximized_vert) || (CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_SESSION_MANAGED | CLIENT_FLAG_MAXIMIZED_VERT)))
            {
                TRACE("clientGetNetState : maximized vert");
                c->win_state |= WIN_STATE_MAXIMIZED_VERT;
                CLIENT_FLAG_SET(c, CLIENT_FLAG_MAXIMIZED_VERT);
            }
            else if((atoms[i] == net_wm_state_fullscreen) && !CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_ABOVE | CLIENT_FLAG_BELOW))
            {
                TRACE("clientGetNetState : fullscreen");
                CLIENT_FLAG_SET(c, CLIENT_FLAG_FULLSCREEN);
            }
            else if((atoms[i] == net_wm_state_above) && !CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_FULLSCREEN | CLIENT_FLAG_BELOW))
            {
                TRACE("clientGetNetState : above");
                CLIENT_FLAG_SET(c, CLIENT_FLAG_ABOVE);
            }
            else if((atoms[i] == net_wm_state_below) && !CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_ABOVE | CLIENT_FLAG_FULLSCREEN))
            {
                TRACE("clientGetNetState : below");
                CLIENT_FLAG_SET(c, CLIENT_FLAG_BELOW);
            }
            else if(atoms[i] == net_wm_state_modal)
            {
                TRACE("clientGetNetState : modal");
                CLIENT_FLAG_SET(c, CLIENT_FLAG_STATE_MODAL | CLIENT_FLAG_STICKY);
            }
            else if(atoms[i] == net_wm_state_skip_pager)
            {
                TRACE("clientGetNetState : skip_pager");
                CLIENT_FLAG_SET(c, CLIENT_FLAG_SKIP_PAGER);
            }
            else if(atoms[i] == net_wm_state_skip_taskbar)
            {
                TRACE("clientGetNetState : skip_taskbar");
                CLIENT_FLAG_SET(c, CLIENT_FLAG_SKIP_TASKBAR);
            }
            else
            {
                g_message(_("%s: Unmanaged net_wm_state (window 0x%lx)"), g_get_prgname(), c->window);
            }

            ++i;
        }
        if(atoms)
        {
            XFree(atoms);
        }
    }

    if(c->win_state & (WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED_HORIZ))
    {
        c->win_state |= WIN_STATE_MAXIMIZED;
        c->win_state &= ~(WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED_HORIZ);
    }
}

void clientUpdateNetState(Client * c, XClientMessageEvent * ev)
{
    unsigned long action;
    Atom first;
    Atom second;

    g_return_if_fail(c != NULL);
    TRACE("entering clientUpdateNetState");
    TRACE("client \"%s\" (0x%lx)", c->name, c->window);

    action = ((XEvent *) ev)->xclient.data.l[0];
    first = ((XEvent *) ev)->xclient.data.l[1];
    second = ((XEvent *) ev)->xclient.data.l[2];

    if((first == net_wm_state_shaded) || (second == net_wm_state_shaded))
    {
        if((action == NET_WM_STATE_ADD) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_SHADED))
        {
            clientShade(c);
        }
        else if((action == NET_WM_STATE_REMOVE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_SHADED))
        {
            clientUnshade(c);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            clientToggleShaded(c);
        }
    }

#if 0
    /*
     * EWMH V 1.2 Implementation note
     * if an Application asks to toggle _NET_WM_STATE_HIDDEN the Window Manager
     * should probably just ignore the request, since _NET_WM_STATE_HIDDEN is a
     * function of some other aspect of the window such as minimization, rather
     * than an independent state.
     */
    if((first == net_wm_state_hidden) || (second == net_wm_state_hidden))
    {
        if((action == NET_WM_STATE_ADD) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_HIDDEN))
        {
            if(CLIENT_CAN_HIDE_WINDOW(c))
            {
                clientHide(c, True);
            }
        }
        else if((action == NET_WM_STATE_REMOVE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_HIDDEN))
        {
            clientShow(c, True);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_HIDDEN))
            {
                clientShow(c, True);
            }
            else if(CLIENT_CAN_HIDE_WINDOW(c))
            {
                clientHide(c, True);
            }
        }
    }
#endif

    if((first == net_wm_state_sticky) || (second == net_wm_state_sticky))
    {
        if((action == NET_WM_STATE_ADD) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_STICKY))
        {
            clientStick(c, TRUE);
        }
        else if((action == NET_WM_STATE_REMOVE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_STICKY))
        {
            clientUnstick(c, TRUE);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            clientToggleSticky(c, TRUE);
        }
    }

    if((first == net_wm_state_maximized_horz) || (second == net_wm_state_maximized_horz) || (first == net_wm_state_maximized_vert) || (second == net_wm_state_maximized_vert))
    {
        if((action == NET_WM_STATE_ADD) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED))
        {
            unsigned long mode = 0;
            if((first == net_wm_state_maximized_horz) || (second == net_wm_state_maximized_horz))
            {
                mode |= WIN_STATE_MAXIMIZED_HORIZ;
            }
            if((first == net_wm_state_maximized_vert) || (second == net_wm_state_maximized_vert))
            {
                mode |= WIN_STATE_MAXIMIZED_VERT;
            }
            if(mode & (WIN_STATE_MAXIMIZED_HORIZ | WIN_STATE_MAXIMIZED_VERT))
            {
                mode |= WIN_STATE_MAXIMIZED;
                mode &= ~(WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED_HORIZ);
            }
            clientToggleMaximized(c, mode);
        }
        else if((action == NET_WM_STATE_REMOVE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED))
        {
            unsigned long mode = 0;
            if(mode & WIN_STATE_MAXIMIZED)
            {
                mode |= (WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED_HORIZ);
            }
            mode &= ~WIN_STATE_MAXIMIZED;
            if((first == net_wm_state_maximized_horz) || (second == net_wm_state_maximized_horz))
            {
                mode &= ~WIN_STATE_MAXIMIZED_HORIZ;
            }
            if((first == net_wm_state_maximized_vert) || (second == net_wm_state_maximized_vert))
            {
                mode &= ~WIN_STATE_MAXIMIZED_VERT;
            }
            clientToggleMaximized(c, mode);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            unsigned long mode = 0;
            if(mode & WIN_STATE_MAXIMIZED)
            {
                mode &= ~WIN_STATE_MAXIMIZED;
                mode |= (WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED_HORIZ);
            }
            if((first == net_wm_state_maximized_horz) || (second == net_wm_state_maximized_horz))
            {
                mode ^= WIN_STATE_MAXIMIZED_HORIZ;
            }
            if((first == net_wm_state_maximized_vert) || (second == net_wm_state_maximized_vert))
            {
                mode ^= WIN_STATE_MAXIMIZED_VERT;
            }
            if(mode & (WIN_STATE_MAXIMIZED_HORIZ | WIN_STATE_MAXIMIZED_VERT))
            {
                mode |= WIN_STATE_MAXIMIZED;
                mode &= ~(WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED_HORIZ);
            }
            clientToggleMaximized(c, mode);
        }
    }

    if((first == net_wm_state_modal) || (second == net_wm_state_modal))
    {
        if((action == NET_WM_STATE_ADD) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_STATE_MODAL))
        {
            CLIENT_FLAG_SET(c, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState(c);
            clientWindowType(c);
        }
        else if((action == NET_WM_STATE_REMOVE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_STATE_MODAL))
        {
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState(c);
            clientWindowType(c);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            CLIENT_FLAG_TOGGLE(c, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState(c);
            clientWindowType(c);
        }
    }

    if(((first == net_wm_state_fullscreen) || (second == net_wm_state_fullscreen)) && !CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_ABOVE | CLIENT_FLAG_BELOW))
    {
        if((action == NET_WM_STATE_ADD) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_FULLSCREEN))
        {
            CLIENT_FLAG_SET(c, CLIENT_FLAG_FULLSCREEN);
        }
        else if((action == NET_WM_STATE_REMOVE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_FULLSCREEN))
        {
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_FULLSCREEN);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            CLIENT_FLAG_TOGGLE(c, CLIENT_FLAG_FULLSCREEN);
        }
        clientToggleFullscreen(c);
    }

    if(((first == net_wm_state_above) || (second == net_wm_state_above)) && !CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_FULLSCREEN | CLIENT_FLAG_BELOW))
    {
        if((action == NET_WM_STATE_ADD) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_ABOVE))
        {
            CLIENT_FLAG_SET(c, CLIENT_FLAG_ABOVE);
        }
        else if((action == NET_WM_STATE_REMOVE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_ABOVE))
        {
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_ABOVE);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            CLIENT_FLAG_TOGGLE(c, CLIENT_FLAG_ABOVE);
        }
        clientToggleAbove(c);
    }

    if(((first == net_wm_state_below) || (second == net_wm_state_below)) && !CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_FULLSCREEN | CLIENT_FLAG_ABOVE))
    {
        if((action == NET_WM_STATE_ADD) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_BELOW))
        {
            CLIENT_FLAG_SET(c, CLIENT_FLAG_BELOW);
        }
        else if((action == NET_WM_STATE_REMOVE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_BELOW))
        {
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_BELOW);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            CLIENT_FLAG_TOGGLE(c, CLIENT_FLAG_BELOW);
        }
        clientToggleBelow(c);
    }

    if((first == net_wm_state_skip_pager) || (second == net_wm_state_skip_pager))
    {
        if((action == NET_WM_STATE_ADD) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_SKIP_PAGER))
        {
            CLIENT_FLAG_SET(c, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState(c);
        }
        else if((action == NET_WM_STATE_REMOVE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_SKIP_PAGER))
        {
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState(c);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            CLIENT_FLAG_TOGGLE(c, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState(c);
        }
    }

    if((first == net_wm_state_skip_taskbar) || (second == net_wm_state_skip_taskbar))
    {
        if((action == NET_WM_STATE_ADD) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_SKIP_TASKBAR))
        {
            CLIENT_FLAG_SET(c, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState(c);
        }
        else if((action == NET_WM_STATE_REMOVE) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_SKIP_TASKBAR))
        {
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState(c);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            CLIENT_FLAG_TOGGLE(c, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState(c);
        }
    }
}

void clientGetNetWmType(Client * c)
{
    int n_atoms = 0;
    Atom *atoms = NULL;
    int i;

    g_return_if_fail(c != NULL);
    TRACE("entering clientGetNetWmType");
    TRACE("client \"%s\" (0x%lx)", c->name, c->window);

    n_atoms = 0;
    atoms = NULL;

    if(!get_atom_list(dpy, c->window, net_wm_window_type, &atoms, &n_atoms))
    {
        switch (c->win_layer)
        {
        case WIN_LAYER_DESKTOP:
            c->type_atom = net_wm_window_type_desktop;
            break;
        case WIN_LAYER_DOCK:
            c->type_atom = net_wm_window_type_dock;
            break;
        case WIN_LAYER_NORMAL:
            c->type_atom = net_wm_window_type_normal;
            break;
        default:
            c->type_atom = None;
            break;
        }
    }
    else
    {
        i = 0;
        while(i < n_atoms)
        {
            if((atoms[i] == net_wm_window_type_desktop) || (atoms[i] == net_wm_window_type_dock) || (atoms[i] == net_wm_window_type_toolbar) || (atoms[i] == net_wm_window_type_menu) || (atoms[i] == net_wm_window_type_dialog) || (atoms[i] == net_wm_window_type_normal) || (atoms[i] == net_wm_window_type_utility) || (atoms[i] == net_wm_window_type_splashscreen))
            {
                c->type_atom = atoms[i];
                break;
            }
            ++i;
        }
        if(atoms)
        {
            XFree(atoms);
        }
    }
    clientWindowType(c);
}

static void clientGetInitialNetWmDesktop(Client * c)
{
    Client *c2 = NULL;
    long val = 0;

    g_return_if_fail(c != NULL);
    TRACE("entering clientGetInitialNetWmDesktop");
    TRACE("client \"%s\" (0x%lx)", c->name, c->window);

    /* This is to make sure that transient are shown with their "master" window */
    if((c->transient_for) && (c2 = clientGetFromWindow(c->transient_for, WINDOW)))
    {
        CLIENT_FLAG_SET(c, CLIENT_FLAG_WORKSPACE_SET);
        c->win_workspace = c2->win_workspace;
        if (CLIENT_FLAG_TEST(c2, CLIENT_FLAG_STICKY))
        {
            CLIENT_FLAG_SET(c, CLIENT_FLAG_STICKY);
            c->win_state |= WIN_STATE_STICKY;
        }
    }
    else
    {
        if(!CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_SESSION_MANAGED | CLIENT_FLAG_WORKSPACE_SET))
        {
            CLIENT_FLAG_SET(c, CLIENT_FLAG_WORKSPACE_SET);
            c->win_workspace = workspace;
        }
        if(getNetHint(dpy, c->window, net_wm_desktop, &val))
        {
            TRACE("atom net_wm_desktop detected");
            if(val == (int)0xFFFFFFFF)
            {
                TRACE("atom net_wm_desktop specifies window \"%s\" is sticky", c->name);
                c->win_workspace = workspace;
                clientStick(c, FALSE);
            }
            else
            {
                TRACE("atom net_wm_desktop specifies window \"%s\" is on desk %i", c->name, (int)val);
                c->win_workspace = (int)val;
            }
            CLIENT_FLAG_SET(c, CLIENT_FLAG_WORKSPACE_SET);
        }
        else if(getGnomeHint(dpy, c->window, win_workspace, &val))
        {
            TRACE("atom win_workspace specifies window \"%s\" is on desk %i", c->name, (int)val);
            c->win_workspace = (int)val;
            CLIENT_FLAG_SET(c, CLIENT_FLAG_WORKSPACE_SET);
        }
    }
    TRACE("initial desktop for window \"%s\" is %i", c->name, c->win_workspace);
    if(c->win_workspace > params.workspace_count - 1)
    {
        TRACE("value off limits, using %i instead", params.workspace_count - 1);
        c->win_workspace = params.workspace_count - 1;
        CLIENT_FLAG_SET(c, CLIENT_FLAG_WORKSPACE_SET);
    }
    TRACE("initial desktop for window \"%s\" is %i", c->name, c->win_workspace);
    setGnomeHint(dpy, c->window, win_workspace, c->win_workspace);
    setNetHint(dpy, c->window, net_wm_desktop, c->win_workspace);
}

static void clientSetNetClientList(Atom a, GSList * list)
{
    Window *listw;
    Window *index_dest;
    GSList *index_src;
    gint size, i;

    TRACE("entering clientSetNetClientList");

    size = g_slist_length(list);
    if(size < 1)
    {
        XDeleteProperty(dpy, root, a);
    }
    else if((listw = (Window *) malloc((size + 1) * sizeof(Window))))
    {
        TRACE("%i windows in list for %i clients", size, client_count);
        for(i = 0, index_dest = listw, index_src = list; i < size; i++, index_dest++, index_src = g_slist_next(index_src))
        {
            Client *c = (Client *) index_src->data;
            *index_dest = c->window;
        }
        XChangeProperty(dpy, root, a, XA_WINDOW, 32, PropModeReplace, (unsigned char *)listw, size);
        free(listw);
    }
}

void clientGetNetStruts(Client * c)
{
    gulong *struts = NULL;
    int nitems;
    int i;

    g_return_if_fail(c != NULL);
    TRACE("entering clientGetNetStruts for \"%s\" (0x%lx)", c->name, c->window);

    for(i = 0; i < 4; i++)
    {
        c->struts[i] = 0;
    }
    CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_STRUTS);

    if(get_cardinal_list(dpy, c->window, net_wm_strut, &struts, &nitems))
    {
        if(nitems != 4)
        {
            XFree(struts);
            return;
        }

        CLIENT_FLAG_SET(c, CLIENT_FLAG_HAS_STRUTS);
        for(i = 0; i < 4; i++)
        {
            c->struts[i] = struts[i];
        }

        TRACE("NET_WM_STRUT for window \"%s\"= (%d,%d,%d,%d)", c->name, c->struts[0], c->struts[1], c->struts[2], c->struts[3]);
        XFree(struts);
        workspaceUpdateArea(margins, gnome_margins);
    }
}

static void clientSetNetActions(Client * c)
{
    Atom atoms[6];
    int i = 0;

    atoms[i++] = net_wm_action_change_desktop;
    atoms[i++] = net_wm_action_close;
    atoms[i++] = net_wm_action_maximize_horz;
    atoms[i++] = net_wm_action_maximize_vert;
    atoms[i++] = net_wm_action_stick;
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_BORDER))
    {
        atoms[i++] = net_wm_action_shade;
    }
    XChangeProperty(dpy, c->window, net_wm_allowed_actions, XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms, i);
}

static void clientWindowType(Client * c)
{
    WindowType old_type;

    g_return_if_fail(c != NULL);
    TRACE("entering clientWindowType");
    TRACE("type for client \"%s\" (0x%lx)", c->name, c->window);

    old_type = c->type;
    c->initial_layer = c->win_layer;
    if(c->type_atom != None)
    {
        if(c->type_atom == net_wm_window_type_desktop)
        {
            TRACE("atom net_wm_window_type_desktop detected");
            c->type = WINDOW_DESKTOP;
            c->initial_layer = WIN_LAYER_DESKTOP;
            c->win_state |= WIN_STATE_STICKY;
            CLIENT_FLAG_SET(c, CLIENT_FLAG_STICKY | CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_SKIP_TASKBAR);
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE | CLIENT_FLAG_HAS_MENU | CLIENT_FLAG_HAS_STICK);
        }
        else if(c->type_atom == net_wm_window_type_dock)
        {
            TRACE("atom net_wm_window_type_dock detected");
            c->type = WINDOW_DOCK;
            c->initial_layer = WIN_LAYER_DOCK;
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE | CLIENT_FLAG_HAS_STICK);
        }
        else if(c->type_atom == net_wm_window_type_toolbar)
        {
            TRACE("atom net_wm_window_type_toolbar detected");
            c->type = WINDOW_TOOLBAR;
            c->initial_layer = WIN_LAYER_NORMAL;
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE | CLIENT_FLAG_HAS_STICK);
        }
        else if(c->type_atom == net_wm_window_type_menu)
        {
            TRACE("atom net_wm_window_type_menu detected");
            c->type = WINDOW_MENU;
            c->initial_layer = WIN_LAYER_NORMAL;
            /* The policy here is unclear :
               http://mail.gnome.org/archives/wm-spec-list/2002-May/msg00001.html
               As it seems, GNOME and KDE don't treat menu the same way...
             */
            CLIENT_FLAG_SET(c, CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_SKIP_TASKBAR);
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE | CLIENT_FLAG_HAS_STICK);
        }
        else if(c->type_atom == net_wm_window_type_dialog)
        {
            TRACE("atom net_wm_window_type_dialog detected");
            c->type = WINDOW_DIALOG;
            c->initial_layer = WIN_LAYER_NORMAL;
        }
        else if(c->type_atom == net_wm_window_type_normal)
        {
            TRACE("atom net_wm_window_type_normal detected");
            c->type = WINDOW_NORMAL;
            c->initial_layer = WIN_LAYER_NORMAL;
        }
        else if(c->type_atom == net_wm_window_type_utility)
        {
            TRACE("atom net_wm_window_type_utility detected");
            c->type = WINDOW_UTILITY;
            c->initial_layer = WIN_LAYER_NORMAL;
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_STICK);
        }
        else if(c->type_atom == net_wm_window_type_splashscreen)
        {
            TRACE("atom net_wm_window_type_splashscreen detected");
            c->type = WINDOW_SPLASHSCREEN;
            c->initial_layer = WIN_LAYER_ABOVE_DOCK;
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MOVE | CLIENT_FLAG_HAS_RESIZE | CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_MENU | CLIENT_FLAG_HAS_STICK);
        }
    }
    else
    {
        TRACE("no \"net\" atom detected");
        c->type = UNSET;
        c->initial_layer = c->win_layer;
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_STATE_MODAL))
    {
        TRACE("window is modal");
        c->type = WINDOW_MODAL_DIALOG;
        c->initial_layer = WIN_LAYER_ONTOP;
        CLIENT_FLAG_SET(c, CLIENT_FLAG_STICKY);
        CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_STICK);
    }
    if(c->transient_for)
    {

        TRACE("Window is a transient");
        if (c->transient_for == root)
        {
            TRACE("window is transient  for root window");
            c->initial_layer = WIN_LAYER_ONTOP;
            CLIENT_FLAG_SET(c, CLIENT_FLAG_STICKY);
            /* Remove the transient field now */
            c->transient_for = None;
        }
        else
        {
            Client *c2;
            
            c2 = clientGetFromWindow(c->transient_for, WINDOW);
            if(c2)
            {
                c->initial_layer = c2->win_layer;
            }
        }
        CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_STICK);
    }
    if((old_type != c->type) || (c->initial_layer != c->win_layer))
    {
        TRACE("setting layer %i", c->initial_layer);
        clientSetNetState(c);
        clientSetLayer(c, c->initial_layer);
    }
}

void clientInstallColormaps(Client * c)
{
    XWindowAttributes attr;
    gboolean installed = False;
    int i;

    g_return_if_fail(c != NULL);
    TRACE("entering clientInstallColormaps");

    if(c->ncmap)
    {
        for(i = c->ncmap - 1; i >= 0; i--)
        {
            XGetWindowAttributes(dpy, c->cmap_windows[i], &attr);
            XInstallColormap(dpy, attr.colormap);
            if(c->cmap_windows[i] == c->window)
            {
                installed = True;
            }
        }
    }
    if((!installed) && (c->cmap))
    {
        XInstallColormap(dpy, c->cmap);
    }
}

void clientUpdateColormaps(Client * c)
{
    XWindowAttributes attr;

    g_return_if_fail(c != NULL);
    TRACE("entering clientUpdateColormaps");

    if(c->ncmap)
    {
        XFree(c->cmap_windows);
    }
    if(!XGetWMColormapWindows(dpy, c->window, &c->cmap_windows, &c->ncmap))
    {
        c->ncmap = 0;
    }
    c->cmap = attr.colormap;
}

void clientUpdateAllFrames(int mask)
{
    Client *c;
    int i;
    XWindowChanges wc;

    TRACE("entering clientRedrawAllFrames");
    for(c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        if(mask & UPDATE_KEYGRABS)
        {
            clientUngrabKeys(c);
            clientGrabKeys(c);
        }
        if(mask & UPDATE_CACHE)
        {
            clientClearPixmapCache(c);
        }
        if(mask & UPDATE_GRAVITY)
        {
            clientGravitate(c, REMOVE);
            clientGravitate(c, APPLY);
            wc.x = c->x;
            wc.y = c->y;
            wc.width = c->width;
            wc.height = c->height;
            clientConfigure(c, &wc, CWX | CWY | CWWidth | CWHeight, FALSE);
        }
        if(mask & UPDATE_FRAME)
        {
            frameDraw(c, FALSE, FALSE);
        }
    }
}

void clientGrabKeys(Client * c)
{
    g_return_if_fail(c != NULL);
    TRACE("entering clientGrabKeys");
    TRACE("grabbing keys for client \"%s\" (0x%lx)", c->name, c->window);

    grabKey(dpy, &params.keys[KEY_MOVE_UP], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_DOWN], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_LEFT], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_RIGHT], c->window);
    grabKey(dpy, &params.keys[KEY_RESIZE_UP], c->window);
    grabKey(dpy, &params.keys[KEY_RESIZE_DOWN], c->window);
    grabKey(dpy, &params.keys[KEY_RESIZE_LEFT], c->window);
    grabKey(dpy, &params.keys[KEY_RESIZE_RIGHT], c->window);
    grabKey(dpy, &params.keys[KEY_CLOSE_WINDOW], c->window);
    grabKey(dpy, &params.keys[KEY_HIDE_WINDOW], c->window);
    grabKey(dpy, &params.keys[KEY_MAXIMIZE_WINDOW], c->window);
    grabKey(dpy, &params.keys[KEY_MAXIMIZE_VERT], c->window);
    grabKey(dpy, &params.keys[KEY_MAXIMIZE_HORIZ], c->window);
    grabKey(dpy, &params.keys[KEY_SHADE_WINDOW], c->window);
    grabKey(dpy, &params.keys[KEY_CYCLE_WINDOWS], c->window);
    grabKey(dpy, &params.keys[KEY_NEXT_WORKSPACE], c->window);
    grabKey(dpy, &params.keys[KEY_PREV_WORKSPACE], c->window);
    grabKey(dpy, &params.keys[KEY_ADD_WORKSPACE], c->window);
    grabKey(dpy, &params.keys[KEY_DEL_WORKSPACE], c->window);
    grabKey(dpy, &params.keys[KEY_STICK_WINDOW], c->window);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_1], c->window);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_2], c->window);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_3], c->window);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_4], c->window);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_5], c->window);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_6], c->window);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_7], c->window);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_8], c->window);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_9], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_NEXT_WORKSPACE], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_PREV_WORKSPACE], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_WORKSPACE_1], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_WORKSPACE_2], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_WORKSPACE_3], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_WORKSPACE_4], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_WORKSPACE_5], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_WORKSPACE_6], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_WORKSPACE_7], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_WORKSPACE_8], c->window);
    grabKey(dpy, &params.keys[KEY_MOVE_WORKSPACE_9], c->window);
    grabKey(dpy, &params.keys[KEY_SHORTCUT_1], c->window);
    grabKey(dpy, &params.keys[KEY_SHORTCUT_2], c->window);
    grabKey(dpy, &params.keys[KEY_SHORTCUT_3], c->window);
    grabKey(dpy, &params.keys[KEY_SHORTCUT_4], c->window);
    grabKey(dpy, &params.keys[KEY_SHORTCUT_5], c->window);
    grabKey(dpy, &params.keys[KEY_SHORTCUT_6], c->window);
    grabKey(dpy, &params.keys[KEY_SHORTCUT_7], c->window);
    grabKey(dpy, &params.keys[KEY_SHORTCUT_8], c->window);
    grabKey(dpy, &params.keys[KEY_SHORTCUT_9], c->window);
    grabKey(dpy, &params.keys[KEY_SHORTCUT_10], c->window);
}

void clientUngrabKeys(Client * c)
{
    g_return_if_fail(c != NULL);
    TRACE("entering clientUngrabKeys");
    TRACE("ungrabing keys for client \"%s\" (0x%lx)", c->name, c->window);

    ungrabKeys(dpy, c->window);
}

void clientCoordGravitate(Client * c, int mode, int *x, int *y)
{
    int dx = 0, dy = 0;

    g_return_if_fail(c != NULL);
    TRACE("entering clientCoordGravitate");

    c->gravity = c->size->flags & PWinGravity ? c->size->win_gravity : NorthWestGravity;
    switch (c->gravity)
    {
    case CenterGravity:
        dx = (c->border_width << 1) - ((frameLeft(c) + frameRight(c)) >> 1);
        dy = (c->border_width << 1) - ((frameTop(c) + frameBottom(c)) >> 1);
        break;
    case NorthGravity:
        dx = (c->border_width << 1) - ((frameLeft(c) + frameRight(c)) >> 1);
        dy = frameTop(c);
        break;
    case SouthGravity:
        dx = (c->border_width << 1) - ((frameLeft(c) + frameRight(c)) >> 1);
        dy = (c->border_width << 1) - frameBottom(c);
        break;
    case EastGravity:
        dx = (c->border_width << 1) - frameRight(c);
        dy = (c->border_width << 1) - ((frameTop(c) + frameBottom(c)) >> 1);
        break;
    case WestGravity:
        dx = frameLeft(c);
        dy = (c->border_width << 1) - ((frameTop(c) + frameBottom(c)) >> 1);
        break;
    case NorthWestGravity:
        dx = frameLeft(c);
        dy = frameTop(c);
        break;
    case NorthEastGravity:
        dx = (c->border_width << 1) - frameRight(c);
        dy = frameTop(c);
        break;
    case SouthWestGravity:
        dx = frameLeft(c);
        dy = (c->border_width << 1) - frameBottom(c);
        break;
    case SouthEastGravity:
        dx = (c->border_width << 1) - frameRight(c);
        dy = (c->border_width << 1) - frameBottom(c);
        break;
    default:
        dx = 0;
        dy = 0;
        break;
    }
    *x = *x + (dx * mode);
    *y = *y + (dy * mode);
}

void clientGravitate(Client * c, int mode)
{
    int x, y;

    g_return_if_fail(c != NULL);
    TRACE("entering clientGravitate");

    x = c->x;
    y = c->y;
    clientCoordGravitate(c, mode, &x, &y);
    c->x = x;
    c->y = y;
}

static void clientAddToList(Client * c)
{
    Client *client_sibling = NULL;
    GSList *sibling = NULL;

    g_return_if_fail(c != NULL);
    TRACE("entering clientAddToList");

    client_count++;
    if(clients)
    {
        c->prev = clients->prev;
        c->next = clients;
        clients->prev->next = c;
        clients->prev = c;
    }
    else
    {
        clients = c;
        c->next = c;
        c->prev = c;
    }

    TRACE("adding window \"%s\" (0x%lx) to windows list", c->name, c->window);
    windows = g_slist_append(windows, c);

    client_sibling = clientGetLowestTransient(c);
    if(client_sibling)
    {
        /* The client has already a transient mapped */
        sibling = g_slist_find(windows_stack, (gconstpointer) client_sibling);
        windows_stack = g_slist_insert_before(windows_stack, sibling, c);
    }
    else
    {
        client_sibling = clientGetNextTopMost(c->win_layer, c);
        if(client_sibling)
        {
            sibling = g_slist_find(windows_stack, (gconstpointer) client_sibling);
            windows_stack = g_slist_insert_before(windows_stack, sibling, c);
        }
        else
        {
            windows_stack = g_slist_append(windows_stack, c);
        }
    }

    clientSetNetClientList(net_client_list, windows);
    clientSetNetClientList(win_client_list, windows);
    clientSetNetClientList(net_client_list_stacking, windows_stack);

    CLIENT_FLAG_SET(c, CLIENT_FLAG_MANAGED);
}

static void clientRemoveFromList(Client * c)
{
    g_return_if_fail(c != NULL);
    TRACE("entering clientRemoveFromList");
    g_assert(client_count > 0);

    CLIENT_FLAG_UNSET(c, CLIENT_FLAG_MANAGED);
    client_count--;
    if(client_count == 0)
    {
        clients = NULL;
    }
    else
    {
        c->next->prev = c->prev;
        c->prev->next = c->next;
        if(c == clients)
        {
            clients = clients->next;
        }
    }

    TRACE("removing window \"%s\" (0x%lx) from windows list", c->name, c->window);
    windows = g_slist_remove(windows, c);

    TRACE("removing window \"%s\" (0x%lx) from windows_stack list", c->name, c->window);
    windows_stack = g_slist_remove(windows_stack, c);

    clientSetNetClientList(net_client_list, windows);
    clientSetNetClientList(win_client_list, windows);
    clientSetNetClientList(net_client_list_stacking, windows_stack);
}

static int clientGetWidthInc(Client * c)
{
    g_return_val_if_fail(c != NULL, 0);
    if(c->size->flags & PResizeInc)
    {
        return c->size->width_inc;
    }
    return 1;
}

static int clientGetHeightInc(Client * c)
{
    g_return_val_if_fail(c != NULL, 0);
    if(c->size->flags & PResizeInc)
    {
        return c->size->height_inc;
    }
    return 1;
}

static void clientSetWidth(Client * c, int w1)
{
    int w2;

    g_return_if_fail(c != NULL);
    TRACE("entering clientSetWidth");
    TRACE("setting width %i for client \"%s\" (0x%lx)", w1, c->name, c->window);

    if(c->size->flags & PResizeInc)
    {
        w2 = (w1 - c->size->min_width) / c->size->width_inc;
        w1 = c->size->min_width + (w2 * c->size->width_inc);
    }
    if(c->size->flags & PMaxSize)
    {
        if(w1 > c->size->max_width)
        {
            w1 = c->size->max_width;
        }
    }
    if(c->size->flags & PMinSize)
    {
        if(w1 < c->size->min_width)
        {
            w1 = c->size->min_width;
        }
    }
    if(w1 < 1)
    {
        w1 = 1;
    }
    c->width = w1;
}

static void clientSetHeight(Client * c, int h1)
{
    int h2;

    g_return_if_fail(c != NULL);
    TRACE("entering clientSetHeight");
    TRACE("setting height %i for client \"%s\" (0x%lx)", h1, c->name, c->window);

    if(c->size->flags & PResizeInc)
    {
        h2 = (h1 - c->size->min_height) / c->size->height_inc;
        h1 = c->size->min_height + (h2 * c->size->height_inc);
    }
    if(c->size->flags & PMaxSize)
    {
        if(h1 > c->size->max_height)
        {
            h1 = c->size->max_height;
        }
    }
    if(c->size->flags & PMinSize)
    {
        if(h1 < c->size->min_height)
        {
            h1 = c->size->min_height;
        }
    }
    if(h1 < 1)
    {
        h1 = 1;
    }
    c->height = h1;
}

static inline void clientApplyStackList(GSList * list)
{
    Client *c;
    GSList *list_copy, *index;
    Window *xwinstack;
    guint nwindows;
    gint i = 0;

    g_return_if_fail(list != NULL);

    nwindows = g_slist_length(list);
    if (nwindows < 1)
    {
        return;
    }

    list_copy = g_slist_copy(list);
    list_copy = g_slist_reverse(list_copy);
    xwinstack = g_new(Window, nwindows);
    for(index = list_copy; index; index = g_slist_next(index))
    {
        c = (Client *) index->data;
        xwinstack[i++] = c->frame;
    }
    /* XRaiseWindow(dpy, xwinstack[0]); */
    XRestackWindows(dpy, xwinstack, (int)nwindows);
    g_slist_free(list_copy);
    g_free(xwinstack);
}

static inline Client *clientGetLowestTransient(Client * c)
{
    Client *lowest_transient = NULL, *c2;
    GSList *index;

    g_return_val_if_fail(c != NULL, NULL);

    TRACE("entering lowest_transient");

    for(index = windows_stack; index; index = g_slist_next(index))
    {
        c2 = (Client *) index->data;
        if((c2 != c) && (c2->transient_for == c->window))
        {
            lowest_transient = c2;
            break;
        }
    }
    return lowest_transient;
}

#if 0
static inline Client *clientGetHighestTransient(Client * c)
{
    Client *highest_transient = NULL;
    Client *c2, *c3;
    GSList *transients = NULL;
    GSList *index1, *index2;

    g_return_val_if_fail(c != NULL, NULL);
    TRACE("entering clientGetHighestTransient");

    for(index1 = windows_stack; index1; index1 = g_slist_next(index1))
    {
        c2 = (Client *) index1->data;
        if(c2)
        {
            if((c2 != c) && (c2->transient_for == c->window))
            {
                transients = g_slist_append(transients, c2);
                highest_transient = c2;
            }
            else
            {
                for(index2 = transients; index2; index2 = g_slist_next(index2))
                {
                    c3 = (Client *) index2->data;
                    if((c3 != c2) && (c2->transient_for == c3->window))
                    {
                        transients = g_slist_append(transients, c2);
                        highest_transient = c2;
                        break;
                    }
                }
            }
        }
    }
    if(transients)
    {
        g_slist_free(transients);
    }

    return highest_transient;
}
#endif

static inline Client *clientGetNextTopMost(int layer, Client * exclude)
{
    Client *top = NULL, *c;
    GSList *index;

    TRACE("entering clientGetNextTopMost");

    for(index = windows_stack; index; index = g_slist_next(index))
    {
        c = (Client *) index->data;
        TRACE("*** stack window \"%s\" (0x%lx), layer %i", c->name, c->window, (int)c->win_layer);
        if(!exclude || (c != exclude))
        {
            if(c->win_layer > layer)
            {
                top = c;
                break;
            }
        }
    }

    return top;
}

static inline Client *clientGetTopMostFocusable(int layer, Client * exclude)
{
    Client *top = NULL, *c;
    GSList *index;

    TRACE("entering clientGetTopMost");

    for(index = windows_stack; index; index = g_slist_next(index))
    {
        c = (Client *) index->data;
        TRACE("*** stack window \"%s\" (0x%lx), layer %i", c->name, c->window, (int)c->win_layer);
        if(!exclude || (c != exclude))
        {
            if((c->win_layer <= layer) && clientAcceptFocus(c) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_HIDDEN))
            {
                top = c;
            }
            else if (c->win_layer > layer)
            {
                break;
            }
        }
    }

    return top;
}

static inline Client *clientGetBottomMost(int layer, Client * exclude)
{
    Client *bot = NULL, *c;
    GSList *index;

    TRACE("entering clientGetBottomMost");

    for(index = windows_stack; index; index = g_slist_next(index))
    {
        c = (Client *) index->data;
        if(c)
        {
            TRACE("*** stack window \"%s\" (0x%lx), layer %i", c->name, c->window, (int)c->win_layer);
            if(!exclude || (c != exclude))
            {
                if(c->win_layer < layer)
                {
                    bot = c;
                }
                else if(c->win_layer >= layer)
                {
                    break;
                }
            }
        }
    }
    return bot;
}

/* clientConstraintPos() is used when moving windows
   to ensure that the window stays accessible to the user
 */
static inline void clientConstraintPos(Client * c, gboolean show_full)
{
    int cx, cy, left, right, top, bottom;
    int disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_x, frame_y, frame_height, frame_width, frame_top, frame_left;
    gboolean leftMostHead, rightMostHead, topMostHead, bottomMostHead;

    g_return_if_fail(c != NULL);
    TRACE("entering clientConstraintPos %s", show_full ? "(with show full)" : "(w/out show full)");
    TRACE("client \"%s\" (0x%lx)", c->name, c->window);

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE("ignoring constrained for client \"%s\" (0x%lx)", c->name, c->window);
        return;
    }
    /* We use a bunch of local vars to reduce the overhead of calling other functions all the time */
    frame_x = frameX(c);
    frame_y = frameY(c);
    frame_height = frameHeight(c);
    frame_width = frameWidth(c);
    frame_top = frameTop(c);
    frame_left = frameLeft(c);

    cx = frame_x + (frame_width >> 1);
    cy = frame_y + (frame_height >> 1);

    leftMostHead = isLeftMostHead(dpy, screen, cx, cy);
    rightMostHead = isRightMostHead(dpy, screen, cx, cy);
    topMostHead = isTopMostHead(dpy, screen, cx, cy);
    bottomMostHead = isBottomMostHead(dpy, screen, cx, cy);

    left = (leftMostHead ? (int)margins[MARGIN_LEFT] : 0);
    right = (rightMostHead ? (int)margins[MARGIN_RIGHT] : 0);
    top = (topMostHead ? (int)margins[MARGIN_TOP] : 0);
    bottom = (bottomMostHead ? (int)margins[MARGIN_BOTTOM] : 0);

    disp_x = MyDisplayX(cx, cy);
    disp_y = MyDisplayY(cx, cy);
    disp_max_x = MyDisplayMaxX(dpy, screen, cx, cy);
    disp_max_y = MyDisplayMaxY(dpy, screen, cx, cy);

    frame_x = frameX(c);
    frame_y = frameY(c);
    frame_height = frameHeight(c);
    frame_width = frameWidth(c);
    frame_top = frameTop(c);
    frame_left = frameLeft(c);

    if(show_full)
    {
        if(rightMostHead && (frame_x + frame_width > disp_max_x - right))
        {
            c->x = disp_max_x - right - frame_width + frame_left;
        }
        if(leftMostHead && (frame_x < disp_x + left))
        {
            c->x = disp_x + left + frame_left;
        }
        if(bottomMostHead && (frame_y + frame_height > disp_max_y - bottom))
        {
            c->y = disp_max_y - bottom - frame_height + frame_top;
        }
        if(topMostHead && (frame_y < disp_y + top))
        {
            c->y = disp_y + top + frame_top;
        }
    }
    else
    {
        if(rightMostHead && (frame_x + CLIENT_MIN_VISIBLE > disp_max_x - right))
        {
            c->x = disp_max_x - right - CLIENT_MIN_VISIBLE + frame_left;
        }
        if(leftMostHead && (frame_x + frame_width < disp_x + CLIENT_MIN_VISIBLE + left))
        {
            c->x = disp_x + CLIENT_MIN_VISIBLE + left - frame_width + frame_left;
        }
        if(bottomMostHead && (frame_y + CLIENT_MIN_VISIBLE > disp_max_y - bottom))
        {
            c->y = disp_max_y - bottom - CLIENT_MIN_VISIBLE + frame_top;
        }
        if(topMostHead && (frame_y + frame_height < disp_y + CLIENT_MIN_VISIBLE + top))
        {
            c->y = disp_y + CLIENT_MIN_VISIBLE + top - frame_height + frame_top;
        }
    }
}

/* clientKeepVisible is used at initial mapping, to make sure
   the window is visible on screen. It also does coordonate
   translation in Xinerama to center window on physical screen
   Not to be confused with clientConstraintPos()
 */
static inline void clientKeepVisible(Client * c)
{
    int cx, cy, left, right, top, bottom;

    g_return_if_fail(c != NULL);
    TRACE("entering clientKeepVisible");
    TRACE("client \"%s\" (0x%lx)", c->name, c->window);

    cx = frameX(c) + (frameWidth(c) >> 1);
    cy = frameY(c) + (frameHeight(c) >> 1);

    left = (isLeftMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_LEFT] : 0);
    right = (isRightMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_RIGHT] : 0);
    top = (isTopMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_TOP] : 0);
    bottom = (isBottomMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_BOTTOM] : 0);

    /* Translate coodinates to center on physical screen */
    if((use_xinerama) && (abs(c->x - ((gdk_screen_width() - c->width) / 2)) < 20) && (abs(c->y - ((gdk_screen_height() - c->height) / 2)) < 20))
    {
        /* We consider that the windows is centered on screen,
         * Thus, will move it so its center on the current
         * physical screen
         */
        c->x = MyDisplayX(cx, cy) + (MyDisplayWidth(dpy, screen, cx, cy) - c->width) / 2;
        c->y = MyDisplayY(cx, cy) + (MyDisplayHeight(dpy, screen, cx, cy) - c->height) / 2;
    }
    clientConstraintPos(c, TRUE);
}

/* Compute rectangle overlap area */
static inline unsigned long overlap(int x0, int y0, int x1, int y1, int tx0, int ty0, int tx1, int ty1)
{
    /* Compute overlapping box */
    if(tx0 > x0)
    {
        x0 = tx0;
    }
    if(ty0 > y0)
    {
        y0 = ty0;
    }
    if(tx1 < x1)
    {
        x1 = tx1;
    }
    if(ty1 < y1)
    {
        y1 = ty1;
    }
    if(x1 <= x0 || y1 <= y0)
    {
        return 0;
    }
    return (x1 - x0) * (y1 - y0);
}

static void clientInitPosition(Client * c)
{
    int test_x = 0, test_y = 0;
    Client *c2;
    int xmax, ymax, best_x, best_y, i, msx, msy;
    int left, right, top, bottom;
    int frame_x, frame_y, frame_height, frame_width;
    unsigned long best_overlaps = 0;
    gboolean first = TRUE;

    g_return_if_fail(c != NULL);
    TRACE("entering clientInitPosition");

    clientGravitate(c, APPLY);

    if(c->size->flags & (PPosition | USPosition))
    {
        if(CONSTRAINED_WINDOW(c))
        {
            clientKeepVisible(c);
        }
        return;
    }
    else if((c->transient_for) && (c2 = clientGetFromWindow(c->transient_for, WINDOW)))
    {
        /* Center transient relative to their parent window */
        c->x = c2->x + (c2->width - c->width) / 2;
        c->y = c2->y + (c2->height - c->height) / 2;
        if(CONSTRAINED_WINDOW(c))
        {
            clientKeepVisible(c);
        }
        return;
    }

    getMouseXY(root, &msx, &msy);
    left = (isLeftMostHead(dpy, screen, msx, msy) ? MAX((int)margins[MARGIN_LEFT], params.xfwm_margins[MARGIN_LEFT]) : 0);
    right = (isRightMostHead(dpy, screen, msx, msy) ? MAX((int)margins[MARGIN_RIGHT], params.xfwm_margins[MARGIN_RIGHT]) : 0);
    top = (isTopMostHead(dpy, screen, msx, msy) ? MAX((int)margins[MARGIN_TOP], params.xfwm_margins[MARGIN_TOP]) : 0);
    bottom = (isBottomMostHead(dpy, screen, msx, msy) ? MAX((int)margins[MARGIN_BOTTOM], params.xfwm_margins[MARGIN_BOTTOM]) : 0);

    frame_x = frameX(c);
    frame_y = frameY(c);
    frame_height = frameHeight(c);
    frame_width = frameWidth(c);

    xmax = MyDisplayMaxX(dpy, screen, msx, msy) - frame_width - right;
    ymax = MyDisplayMaxY(dpy, screen, msx, msy) - frame_height - bottom;
    best_x = MyDisplayX(msx, msy) + frameLeft(c) + left;
    best_y = MyDisplayY(msx, msy) + frameTop(c) + top;

    for(test_y = MyDisplayY(msx, msy) + frameTop(c) + top; test_y < ymax; test_y += 8)
    {
        for(test_x = MyDisplayX(msx, msy) + frameLeft(c) + left; test_x < xmax; test_x += 8)
        {
            unsigned long count_overlaps = 0;
            TRACE("analyzing %i clients", client_count);
            for(c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
            {
                if((c2 != c) && (c2->type != WINDOW_DESKTOP) && (c->win_workspace == c2->win_workspace) && CLIENT_FLAG_TEST(c2, CLIENT_FLAG_VISIBLE))
                {
                    c->x = test_x;
                    c->y = test_y;
                    frame_x = frameX(c);
                    frame_y = frameY(c);
                    count_overlaps += overlap(frame_x, frame_y, frame_x + frame_width, frame_y + frame_height, frameX(c2), frameY(c2), frameX(c2) + frameWidth(c2), frameY(c2) + frameHeight(c2));
                }
            }
            TRACE("overlaps so far is %u", (unsigned int)count_overlaps);
            if(count_overlaps == 0)
            {
                TRACE("overlaps is 0 so it's the best we can get");
                return;
            }
            else if((count_overlaps < best_overlaps) || (first))
            {
                TRACE("overlaps %u is better than the best we have %u", (unsigned int)count_overlaps, (unsigned int)best_overlaps);
                best_x = test_x;
                best_y = test_y;
                best_overlaps = count_overlaps;
            }
            if(first)
            {
                first = FALSE;
            }
        }
    }
    c->x = best_x;
    c->y = best_y;
    return;
}

void clientConfigure(Client * c, XWindowChanges * wc, int mask, gboolean constrained)
{
    XConfigureEvent ce;

    g_return_if_fail(c != NULL);
    g_return_if_fail(c->window != None);

    TRACE("entering clientConfigure");
    TRACE("configuring client \"%s\" (0x%lx) %s, type %u", c->name, c->window, constrained ? "constrained" : "not contrained", c->type);

    if(mask & CWX)
    {
        if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_MOVING_RESIZING))
        {
#if 0
            if((c->gravity != StaticGravity) && (wc->x == frameX(c)))
            {
                mask &= ~CWX;
            }
            else
            {
                c->x = wc->x;
            }
#else /* 0 */
            c->x = wc->x;
#endif /* 0 */

        }
    }
    if(mask & CWY)
    {
        if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_MOVING_RESIZING))
        {
#if 0
            if((c->gravity != StaticGravity) && (wc->y == frameY(c)))
            {
                mask &= ~CWY;
            }
            else
            {
                c->y = wc->y;
            }
#else /* 0 */
            c->y = wc->y;
#endif /* 0 */

        }
    }
    if(mask & CWWidth)
    {
        clientSetWidth(c, wc->width);
    }
    if(mask & CWHeight)
    {
        clientSetHeight(c, wc->height);
    }
    if(mask & CWBorderWidth)
    {
        c->border_width = wc->border_width;
    }
    if(mask & CWStackMode)
    {
        switch (wc->stack_mode)
        {
            /* Limitation: we don't support sibling... */
        case Above:
        case TopIf:
            TRACE("Above");
            clientRaise(c);
            break;
        case Below:
        case BottomIf:
            TRACE("Below");
            clientLower(c);
            break;
        case Opposite:
        default:
            break;
        }
        mask &= ~(CWStackMode | CWSibling);
    }

    if(constrained && (mask & (CWX | CWY)) && CONSTRAINED_WINDOW(c))
    {
        clientConstraintPos(c, TRUE);
    }
    wc->x = frameX(c);
    wc->y = frameY(c);
    wc->width = frameWidth(c);
    wc->height = frameHeight(c);
    wc->border_width = 0;
    XConfigureWindow(dpy, c->frame, mask, wc);
    wc->x = frameLeft(c);
    wc->y = frameTop(c);
    wc->width = c->width;
    wc->height = c->height;
    XConfigureWindow(dpy, c->window, mask, wc);

    if(mask & (CWWidth | CWHeight))
    {
        frameDraw(c, FALSE, TRUE);
    }
    if(mask)
    {
        ce.type = ConfigureNotify;
        ce.event = c->window;
        ce.window = c->window;
        ce.x = c->x;
        ce.y = c->y;
        ce.width = c->width;
        ce.height = c->height;
        ce.border_width = 0;
        ce.above = c->frame;
        ce.override_redirect = False;
        XSendEvent(dpy, c->window, False, StructureNotifyMask, (XEvent *) & ce);
    }
}

void clientUpdateMWMHints(Client * c)
{
    PropMwmHints *mwm_hints;

    g_return_if_fail(c != NULL);
    g_return_if_fail(c->window != None);
    TRACE("entering clientUpdateMWMHints client \"%s\" (0x%lx)", c->name, c->window);

    mwm_hints = getMotifHints(dpy, c->window);
    if(mwm_hints)
    {
        if(mwm_hints->flags & MWM_HINTS_DECORATIONS)
        {
            if(mwm_hints->decorations & MWM_DECOR_ALL)
            {
                CLIENT_FLAG_SET(c, CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_MENU);
            }
            else
            {
                CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_MENU);
                CLIENT_FLAG_SET(c, (mwm_hints->decorations & (MWM_DECOR_TITLE | MWM_DECOR_BORDER)) ? CLIENT_FLAG_HAS_BORDER : 0);
                CLIENT_FLAG_SET(c, (mwm_hints->decorations & (MWM_DECOR_MENU)) ? CLIENT_FLAG_HAS_MENU : 0);
                /*
                   CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_HIDE);
                   CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_MAXIMIZE);
                   CLIENT_FLAG_SET(c, (mwm_hints->decorations & (MWM_DECOR_MINIMIZE)) ? CLIENT_FLAG_HAS_HIDE : 0);
                   CLIENT_FLAG_SET(c, (mwm_hints->decorations & (MWM_DECOR_MAXIMIZE)) ? CLIENT_FLAG_HAS_MAXIMIZE : 0);
                 */
            }
        }
        /* The following is from Metacity : */
        if(mwm_hints->flags & MWM_HINTS_FUNCTIONS)
        {
            if(!(mwm_hints->functions & MWM_FUNC_ALL))
            {
                CLIENT_FLAG_UNSET(c, CLIENT_FLAG_HAS_CLOSE | CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE | CLIENT_FLAG_HAS_MOVE | CLIENT_FLAG_HAS_RESIZE);
            }
            else
            {
                CLIENT_FLAG_SET(c, CLIENT_FLAG_HAS_CLOSE | CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE | CLIENT_FLAG_HAS_MOVE | CLIENT_FLAG_HAS_RESIZE);
            }

            if(mwm_hints->functions & MWM_FUNC_CLOSE)
            {
                CLIENT_FLAG_TOGGLE(c, CLIENT_FLAG_HAS_CLOSE);
            }
            if(mwm_hints->functions & MWM_FUNC_MINIMIZE)
            {
                CLIENT_FLAG_TOGGLE(c, CLIENT_FLAG_HAS_HIDE);
            }
            if(mwm_hints->functions & MWM_FUNC_MAXIMIZE)
            {
                CLIENT_FLAG_TOGGLE(c, CLIENT_FLAG_HAS_MAXIMIZE);
            }
            if(mwm_hints->functions & MWM_FUNC_RESIZE)
            {
                CLIENT_FLAG_TOGGLE(c, CLIENT_FLAG_HAS_RESIZE);
            }
            if(mwm_hints->functions & MWM_FUNC_MOVE)
            {
                CLIENT_FLAG_TOGGLE(c, CLIENT_FLAG_HAS_MOVE);
            }
        }
        XFree(mwm_hints);
    }
}

static inline void clientFree(Client * c)
{
    g_return_if_fail(c != NULL);

    TRACE("entering clientFree");
    TRACE("freeing client \"%s\" (0x%lx)", c->name, c->window);

    if(c->name)
    {
        free(c->name);
    }
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    if(c->startup_id)
    {
        free(c->startup_id);
    }
#endif
    if(c->size)
    {
        XFree(c->size);
    }
    if(c->wmhints)
    {
        XFree(c->wmhints);
    }
    if(c->ncmap > 0)
    {
        XFree(c->cmap_windows);
    }
    if(c->class.res_name)
    {
        XFree(c->class.res_name);
    }
    if(c->class.res_class)
    {
        XFree(c->class.res_class);
    }

    free(c);
}

static inline void clientApplyInitialNetState(Client * c)
{
    g_return_if_fail(c != NULL);

    TRACE("entering clientApplyInitialNetState");

    /* We check that afterwards to make sure all states are now known */
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED_HORIZ | CLIENT_FLAG_MAXIMIZED_VERT))
    {
        unsigned long mode = 0;

        TRACE("Applying client's initial state: maximized");
        if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED_HORIZ))
        {
            TRACE("initial state: maximized horiz.");
            mode |= WIN_STATE_MAXIMIZED_HORIZ;
        }
        if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED_VERT))
        {
            TRACE("initial state: maximized vert.");
            mode |= WIN_STATE_MAXIMIZED_VERT;
        }
        if(mode & (WIN_STATE_MAXIMIZED_HORIZ | WIN_STATE_MAXIMIZED_VERT))
        {
            TRACE("initial state: fully maximized");
            mode |= WIN_STATE_MAXIMIZED;
            mode &= ~(WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED_HORIZ);
        }
        /* Unset fullscreen mode so that clientToggleMaximized() really change the state */
        CLIENT_FLAG_UNSET(c, CLIENT_FLAG_MAXIMIZED);
        clientToggleMaximized(c, mode);
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE("Applying client's initial state: fullscreen");
        clientToggleFullscreen(c);
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_ABOVE))
    {
        TRACE("Applying client's initial state: above");
        clientToggleAbove(c);
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_BELOW))
    {
        TRACE("Applying client's initial state: below");
        clientToggleBelow(c);
    }
}

void clientClearPixmapCache(Client * c)
{
    g_return_if_fail(c != NULL);

    myPixmapFree(dpy, &c->pm_cache.pm_title[ACTIVE]);
    myPixmapFree(dpy, &c->pm_cache.pm_title[INACTIVE]);
    myPixmapFree(dpy, &c->pm_cache.pm_sides[SIDE_LEFT][ACTIVE]);
    myPixmapFree(dpy, &c->pm_cache.pm_sides[SIDE_LEFT][INACTIVE]);
    myPixmapFree(dpy, &c->pm_cache.pm_sides[SIDE_RIGHT][ACTIVE]);
    myPixmapFree(dpy, &c->pm_cache.pm_sides[SIDE_RIGHT][INACTIVE]);
    myPixmapFree(dpy, &c->pm_cache.pm_sides[SIDE_BOTTOM][ACTIVE]);
    myPixmapFree(dpy, &c->pm_cache.pm_sides[SIDE_BOTTOM][INACTIVE]);
}

void clientFrame(Window w, gboolean initial)
{
    XWindowAttributes attr;
    XWindowChanges wc;
    XSetWindowAttributes attributes;
    Window dummy_root;
    Client *c;
    unsigned long dummy;
    unsigned long valuemask;
    unsigned int dummy_width, dummy_height, dummy_depth, dummy_bw;
    unsigned int wm_protocols_flags = 0;
    int i;
    int dummy_x, dummy_y;

    g_return_if_fail(w != None);
    TRACE("entering clientFrame");
    TRACE("framing client (0x%lx)", w);

    if(w == gnome_win)
    {
        TRACE("Not managing our own window");
        return;
    }

    if(!XGetWindowAttributes(dpy, w, &attr))
    {
        TRACE("Cannot get window attributes");
        return;
    }

    if(attr.override_redirect)
    {
        TRACE("Not managing override_redirect windows");
        return;
    }

    if(!(c = malloc(sizeof(Client))))
    {
        TRACE("Cannot allocate memory for the window structure");
        return;
    }

    c->window = w;
    getWindowName(dpy, c->window, &c->name);
    TRACE("name \"%s\"", c->name);
    getTransientFor(dpy, screen, c->window, &c->transient_for);
    c->size = XAllocSizeHints();
    XGetWMNormalHints(dpy, w, c->size, &dummy);
    wm_protocols_flags = getWMProtocols(dpy, c->window);
    c->x = attr.x;
    c->y = attr.y;
    c->width = attr.width;
    c->height = attr.height;
    c->old_x = c->x;
    c->old_y = c->y;
    c->old_width = c->width;
    c->old_height = c->height;
    c->fullscreen_old_x = c->x;
    c->fullscreen_old_y = c->y;
    c->fullscreen_old_width = c->width;
    c->fullscreen_old_height = c->height;
    c->border_width = attr.border_width;
    c->cmap = attr.colormap;

    /* Initialize pixmap caching */
    myPixmapInit(&c->pm_cache.pm_title[ACTIVE]);
    myPixmapInit(&c->pm_cache.pm_title[INACTIVE]);
    myPixmapInit(&c->pm_cache.pm_sides[SIDE_LEFT][ACTIVE]);
    myPixmapInit(&c->pm_cache.pm_sides[SIDE_LEFT][INACTIVE]);
    myPixmapInit(&c->pm_cache.pm_sides[SIDE_RIGHT][ACTIVE]);
    myPixmapInit(&c->pm_cache.pm_sides[SIDE_RIGHT][INACTIVE]);
    myPixmapInit(&c->pm_cache.pm_sides[SIDE_BOTTOM][ACTIVE]);
    myPixmapInit(&c->pm_cache.pm_sides[SIDE_BOTTOM][INACTIVE]);
    c->pm_cache.previous_width = -1;
    c->pm_cache.previous_height = -1;

    for(i = 0; i < BUTTON_COUNT; i++)
    {
        c->button_pressed[i] = False;
    }

    if(!XGetWMColormapWindows(dpy, c->window, &c->cmap_windows, &c->ncmap))
    {
        c->ncmap = 0;
    }

    c->class.res_name = NULL;
    c->class.res_class = NULL;
    XGetClassHint(dpy, w, &c->class);
    c->wmhints = XGetWMHints(dpy, c->window);
    c->group_leader = None;
    if(c->wmhints)
    {
        c->group_leader = c->wmhints->window_group;
    }
    c->client_leader = None;
    c->client_leader = getClientLeader(dpy, c->window);
#ifdef HAVE_LIBSTARTUP_NOTIFICATION

    c->startup_id = NULL;
    getWindowStartupId(dpy, c->window, &c->startup_id);
#endif

    /* Initialize structure */
    c->client_flag = CLIENT_FLAG_INITIAL_VALUES;

    if (attr.map_state == IsViewable)
    {
        c->ignore_unmap = 1;
        CLIENT_FLAG_SET(c, CLIENT_FLAG_REPARENTING);
    }
    else
    {
        c->ignore_unmap = 0;
    }
    
    c->type = UNSET;
    c->type_atom = None;

    CLIENT_FLAG_SET(c, START_ICONIC(c) ? CLIENT_FLAG_HIDDEN : 0);
    CLIENT_FLAG_SET(c, (wm_protocols_flags & WM_PROTOCOLS_DELETE_WINDOW) ? CLIENT_FLAG_WM_DELETE : 0);
    CLIENT_FLAG_SET(c, ACCEPT_INPUT(c->wmhints) ? CLIENT_FLAG_WM_INPUT : 0);
    CLIENT_FLAG_SET(c, (wm_protocols_flags & WM_PROTOCOLS_TAKE_FOCUS) ? CLIENT_FLAG_WM_TAKEFOCUS : 0);

    if(((c->size->flags & (PMinSize | PMaxSize)) != (PMinSize | PMaxSize)) || (((c->size->flags & (PMinSize | PMaxSize)) == (PMinSize | PMaxSize)) && ((c->size->min_width < c->size->max_width) || (c->size->min_height < c->size->max_height))))
    {
        CLIENT_FLAG_SET(c, CLIENT_FLAG_IS_RESIZABLE);
    }

    clientUpdateMWMHints(c);
    getGnomeHint(dpy, w, win_hints, &c->win_hints);
    getGnomeHint(dpy, w, win_state, &c->win_state);
    if(!getGnomeHint(dpy, w, win_layer, &c->win_layer))
    {
        c->win_layer = WIN_LAYER_NORMAL;
    }

    /* Reload from session */
    if(sessionMatchWinToSM(c))
    {
        CLIENT_FLAG_SET(c, CLIENT_FLAG_SESSION_MANAGED);
    }

    CLIENT_FLAG_SET(c, (c->win_state & (WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED)) ? CLIENT_FLAG_MAXIMIZED_VERT : 0);
    CLIENT_FLAG_SET(c, (c->win_state & (WIN_STATE_MAXIMIZED_HORIZ | WIN_STATE_MAXIMIZED)) ? CLIENT_FLAG_MAXIMIZED_HORIZ : 0);

    /* Beware, order of calls is important here ! */
    sn_client_startup_properties(c);
    clientGetNetState(c);
    clientGetInitialNetWmDesktop(c);
    clientGetNetWmType(c);
    clientGetNetStruts(c);

    /* Once we know the type of window, we can initialize window position */
    if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_SESSION_MANAGED))
    {
        if(attr.map_state != IsViewable)
        {
            clientInitPosition(c);
        }
        else
        {
            clientGravitate(c, APPLY);
        }
    }
    /* We must call clientApplyInitialNetState() after having placed the
       window so that the inital position values are correctly set if the
       inital state is maximize or fullscreen
     */
    clientApplyInitialNetState(c);

    if (!initial)
    {
        MyXGrabServer();
    }
    if(XGetGeometry(dpy, w, &dummy_root, &dummy_x, &dummy_y, &dummy_width, &dummy_height, &dummy_bw, &dummy_depth) == 0)
    {
        clientFree(c);
        if (!initial)
        {
            MyXUngrabServer();
        }
        return;
    }
    valuemask = CWEventMask;
    attributes.event_mask = (FRAME_EVENT_MASK | POINTER_EVENT_MASK);
    c->frame = XCreateWindow(dpy, root, frameX(c), frameY(c), frameWidth(c), frameHeight(c), 0, CopyFromParent, InputOutput, CopyFromParent, valuemask, &attributes);
    TRACE("frame id (0x%lx)", c->frame);
    valuemask = CWEventMask;
    attributes.event_mask = (CLIENT_EVENT_MASK);
    XChangeWindowAttributes(dpy, c->window, valuemask, &attributes);
    if(shape)
    {
        XShapeSelectInput(dpy, c->window, ShapeNotifyMask);
    }
    XSetWindowBorderWidth(dpy, c->window, 0);
    XReparentWindow(dpy, c->window, c->frame, frameLeft(c), frameTop(c));

    clientAddToList(c);
    clientSetNetActions(c);
    clientGrabKeys(c);
    XGrabButton(dpy, AnyButton, AnyModifier, c->window, False, POINTER_EVENT_MASK, GrabModeSync, GrabModeAsync, None, None);
    if (!initial)
    {
        MyXUngrabServer();
    }

    myWindowCreate(dpy, c->frame, &c->sides[SIDE_LEFT], resize_cursor[4 + SIDE_LEFT]);
    myWindowCreate(dpy, c->frame, &c->sides[SIDE_RIGHT], resize_cursor[4 + SIDE_RIGHT]);
    myWindowCreate(dpy, c->frame, &c->sides[SIDE_BOTTOM], resize_cursor[4 + SIDE_BOTTOM]);
    myWindowCreate(dpy, c->frame, &c->corners[CORNER_BOTTOM_LEFT], resize_cursor[CORNER_BOTTOM_LEFT]);
    myWindowCreate(dpy, c->frame, &c->corners[CORNER_BOTTOM_RIGHT], resize_cursor[CORNER_BOTTOM_RIGHT]);
    myWindowCreate(dpy, c->frame, &c->corners[CORNER_TOP_LEFT], resize_cursor[CORNER_TOP_LEFT]);
    myWindowCreate(dpy, c->frame, &c->corners[CORNER_TOP_RIGHT], resize_cursor[CORNER_TOP_RIGHT]);
    myWindowCreate(dpy, c->frame, &c->title, None);
    for(i = 0; i < BUTTON_COUNT; i++)
    {
        myWindowCreate(dpy, c->frame, &c->buttons[i], None);
    }
    TRACE("now calling configure for the new window \"%s\" (0x%lx)", c->name, c->window);
    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    clientConfigure(c, &wc, CWX | CWY | CWHeight | CWWidth, FALSE);
    clientApplyStackList(windows_stack);
    last_raise = c;

    if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_HIDDEN))
    {
        clientShow(c, True);
        if(!initial && params.focus_new && clientAcceptFocus(c))
        {
            /* We set the draw_active value to the wrong value to force a draw */
            c->draw_active = FALSE;
            clientSetFocus(c, True);
        }
        else
        {
            /* We set the draw_active value to the wrong value to force a draw */
            c->draw_active = TRUE;
            frameDraw(c, FALSE, FALSE);
        }
    }
    else
    {
        /* We set the draw_active value to the wrong value to force a draw */
        c->draw_active = TRUE;
        frameDraw(c, FALSE, FALSE);
        setWMState(dpy, c->window, IconicState);
        clientSetNetState(c);
    }

    TRACE("client_count=%d", client_count);
}

void clientUnframe(Client * c, gboolean remap)
{
    int i;

    TRACE("entering clientUnframe");
    TRACE("unframing client \"%s\" (0x%lx)", c->name, c->window);

    g_return_if_fail(c != NULL);
    if(client_focus == c)
    {
        client_focus = NULL;
    }
    if(last_raise == c)
    {
        last_raise = NULL;
    }
    clientGravitate(c, REMOVE);
    clientUngrabKeys(c);
    XUngrabButton(dpy, AnyButton, AnyModifier, c->window);
    XSetWindowBorderWidth(dpy, c->window, c->border_width);
    if(remap)
    {
        XMapWindow(dpy, c->window);
    }
    else
    {
        setWMState(dpy, c->window, WithdrawnState);
    }
    myWindowDelete(&c->title);
    myWindowDelete(&c->sides[SIDE_LEFT]);
    myWindowDelete(&c->sides[SIDE_RIGHT]);
    myWindowDelete(&c->sides[SIDE_BOTTOM]);
    myWindowDelete(&c->sides[CORNER_BOTTOM_LEFT]);
    myWindowDelete(&c->sides[CORNER_BOTTOM_RIGHT]);
    myWindowDelete(&c->sides[CORNER_TOP_LEFT]);
    myWindowDelete(&c->sides[CORNER_TOP_RIGHT]);
    for(i = 0; i < BUTTON_COUNT; i++)
    {
        myWindowDelete(&c->buttons[i]);
    }
    XReparentWindow(dpy, c->window, root, c->x, c->y);
    XDestroyWindow(dpy, c->frame);
    clientClearPixmapCache(c);
    clientRemoveFromList(c);
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_STRUTS))
    {
        workspaceUpdateArea(margins, gnome_margins);
    }
    clientFree(c);
}

void clientFrameAll()
{
    unsigned int count, i;
    Client *new_focus;
    Window w1, w2, *wins = NULL;
    XWindowAttributes attr;

    TRACE("entering clientFrameAll");

    /* Since this fn is called at startup, it's safe to initialize some vars here */
    client_count = 0;
    clients = NULL;
    windows = NULL;
    windows_stack = NULL;
    client_focus = NULL;

    clientSetFocus(NULL, FALSE);
    XSync(dpy, FALSE);
    MyXGrabServer();
    XQueryTree(dpy, root, &w1, &w2, &wins, &count);
    for(i = 0; i < count; i++)
    {
        XGetWindowAttributes(dpy, wins[i], &attr);
        if((!(attr.override_redirect)) && (attr.map_state == IsViewable))
        {
            clientFrame(wins[i], TRUE);
        }
    }
    MyXUngrabServer();
    if(wins)
    {
        XFree(wins);
    }
    new_focus = clientGetTopMostFocusable(WIN_LAYER_NORMAL, NULL);
    clientSetFocus(new_focus, TRUE);
}

void clientUnframeAll()
{
    Client *c;
    unsigned int count, i;
    Window w1, w2, *wins = NULL;

    TRACE("entering clientUnframeAll");

    MyXGrabServer();
    clientSetFocus(NULL, FALSE);
    XQueryTree(dpy, root, &w1, &w2, &wins, &count);
    for(i = 0; i < count; i++)
    {
        c = clientGetFromWindow(wins[i], FRAME);
        if(c)
        {
            clientUnframe(c, True);
        }
    }
    MyXUngrabServer();
    if(wins)
    {
        XFree(wins);
    }
}

Client *clientGetFromWindow(Window w, int mode)
{
    Client *c;
    int i;

    g_return_val_if_fail(w != None, NULL);
    TRACE("entering clientGetFromWindow");
    TRACE("looking for (0x%lx)", w);

    for(c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        switch (mode)
        {
        case WINDOW:
            if(c->window == w)
            {
                TRACE("found \"%s\" (mode WINDOW)", c->name);
                return (c);
            }
            break;
        case FRAME:
            if(c->frame == w)
            {
                TRACE("found \"%s\" (mode FRAME)", c->name);
                return (c);
            }
            break;
        case ANY:
        default:
            if((c->frame == w) || (c->window == w))
            {
                TRACE("found \"%s\" (mode ANY)", c->name);
                return (c);
            }
            break;
        }
    }
    TRACE("no client found");

    return NULL;
}

static inline gboolean clientSelectMask(Client * c, int mask)
{
    gboolean okay = TRUE;

    TRACE("entering clientSelectMask");
    if((!clientAcceptFocus(c)) && !(mask & INCLUDE_SKIP_FOCUS))
    {
        okay = FALSE;
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_HIDDEN) && !(mask & INCLUDE_HIDDEN))
    {
        okay = FALSE;
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_SKIP_PAGER) && !(mask & INCLUDE_SKIP_PAGER))
    {
        okay = FALSE;
    }
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_SKIP_TASKBAR) && !(mask & INCLUDE_SKIP_TASKBAR))
    {
        okay = FALSE;
    }
    if((c->win_workspace != workspace) && !(mask & INCLUDE_ALL_WORKSPACES))
    {
        okay = FALSE;
    }

    return okay;
}

Client *clientGetNext(Client * c, int mask)
{
    Client *c2;
    unsigned int i;

    TRACE("entering clientGetNext");

    if(c)
    {
        for(c2 = c->next, i = 0; (c2) && (i < client_count); c2 = c2->next, i++)
        {
            if((c2->type == WINDOW_SPLASHSCREEN) || (c2->type == WINDOW_DESKTOP))
            {
                continue;
            }
            if(clientSelectMask(c2, mask))
            {
                return c2;
            }
        }
    }
    return NULL;
}

/* Build a GSList of clients that have a transient relationship */
static GSList *clientListTransients(Client *c)
{
    GSList *transients = NULL;
    GSList *index1, *index2;
    Client *c2, *c3;

    g_return_val_if_fail(c != NULL, NULL);
    
    transients = g_slist_append(transients, c);
    for(index1 = windows_stack; index1; index1 = g_slist_next(index1))
    {
        c2 = (Client *) index1->data;
        if(c2 != c)
        {
            if(c2->transient_for == c->window)
            {
                transients = g_slist_append(transients, c2);
            }
            else
            {
                for(index2 = transients; index2; index2 = g_slist_next(index2))
                {
                    c3 = (Client *) index2->data;
                    if((c3 != c2) && (c2->transient_for == c3->window))
                    {
                        transients = g_slist_append(transients, c2);
                        break;
                    }
                }
            }
        }
    }
    return transients;
}

void clientPassFocus(Client * c)
{
    GSList *list_of_windows = NULL;
    Client *new_focus = NULL;
    Client *c2;
    unsigned int i;

    TRACE("entering clientPassFocus");

    if ((!c) || (c != client_focus))
    {
        return;
    }

    list_of_windows = clientListTransients(c);
    for(c2 = c->next, i = 0; (c2) && (i < client_count); c2 = c2->next, i++)
    {
        if (clientSelectMask(c2, 0) && !g_slist_find(list_of_windows, (gconstpointer) c2))
        {
            new_focus = c2;
            break;
        }
    }
    g_slist_free(list_of_windows);
    clientSetFocus(new_focus, TRUE);
}

void clientShow(Client * c, gboolean change_state)
{
    GSList *list_of_windows = NULL;
    GSList *index;
    Client *c2;

    g_return_if_fail(c != NULL);
    TRACE("entering clientShow");

    list_of_windows = clientListTransients(c);
    list_of_windows = g_slist_reverse(list_of_windows);
    
    for(index = list_of_windows; index; index = g_slist_next(index))
    {
        c2 = (Client *) index->data;
        clientSetWorkspaceSingle(c2, c->win_workspace);
        if ((c->win_workspace == workspace) || CLIENT_FLAG_TEST(c, CLIENT_FLAG_STICKY))
        {
            TRACE("showing client \"%s\" (0x%lx)", c2->name, c2->window);
            CLIENT_FLAG_SET(c2, CLIENT_FLAG_VISIBLE);
            XMapWindow(dpy, c2->window);
            XMapWindow(dpy, c2->frame);
        }
        if(change_state)
        {
            CLIENT_FLAG_UNSET(c2, CLIENT_FLAG_HIDDEN);
            setWMState(dpy, c2->window, NormalState);
            workspaceUpdateArea(margins, gnome_margins);
        }
        clientSetNetState(c2);
    }
    g_slist_free(list_of_windows);
}

void clientHide(Client * c, gboolean change_state)
{
    GSList *list_of_windows = NULL;
    GSList *index;
    Client *c2;

    g_return_if_fail(c != NULL);
    TRACE("entering clientHide");

    list_of_windows = clientListTransients(c);
    for(index = list_of_windows; index; index = g_slist_next(index))
    {
        c2 = (Client *) index->data;
        TRACE("hiding client \"%s\" (0x%lx)", c2->name, c2->window);

        XUnmapWindow(dpy, c2->window);
        XUnmapWindow(dpy, c2->frame);
        CLIENT_FLAG_UNSET(c2, CLIENT_FLAG_VISIBLE);
        if(change_state)
        {
            CLIENT_FLAG_SET(c2, CLIENT_FLAG_HIDDEN);
            setWMState(dpy, c2->window, IconicState);
            workspaceUpdateArea(margins, gnome_margins);
        }
        c2->ignore_unmap++;
        clientSetNetState(c2);
    }
    g_slist_free(list_of_windows);
}

void clientHideAll(Client * c)
{
    Client *c2;
    int i;

    TRACE("entering clientHideAll");

    for(c2 = c->next, i = 0; (c2) && (i < client_count); c2 = c2->next, i++)
    {
        if(CLIENT_CAN_HIDE_WINDOW(c2) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_BORDER) && !(c2->transient_for) && (c2 != c))
        {
            if((!c) || ((c->transient_for != c2->window) && (c2->win_workspace == c->win_workspace)))
            {
                clientHide(c2, True);
            }
        }
    }
}

void clientClose(Client * c)
{
    g_return_if_fail(c != NULL);
    TRACE("entering clientClose");
    TRACE("closing client \"%s\" (0x%lx)", c->name, c->window);

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_WM_DELETE))
    {
        sendClientMessage(c->window, wm_protocols, wm_delete_window, NoEventMask);
    }
    else
    {
        clientKill(c);
    }
}

void clientKill(Client * c)
{
    g_return_if_fail(c != NULL);
    TRACE("entering clientKill");
    TRACE("killing client \"%s\" (0x%lx)", c->name, c->window);

    XKillClient(dpy, c->window);
}

void clientRaise(Client * c)
{
    g_return_if_fail(c != NULL);
    TRACE("entering clientRaise");

    if(c == last_raise)
    {
        TRACE("client \"%s\" (0x%lx) already raised", c->name, c->window);
        return;
    }
    TRACE("raising client \"%s\" (0x%lx)", c->name, c->window);

    if(g_slist_length(windows_stack) < 1)
    {
        return;
    }

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MANAGED))
    {
        Client *c2, *c3;
        Client *client_sibling = NULL;
        GSList *transients = NULL;
        GSList *sibling = NULL;
        GSList *index1, *index2;
        GSList *windows_stack_copy;

        /* Copy the existing window stack temporarily as reference */
        windows_stack_copy = g_slist_copy(windows_stack);
        /* Search for the window that will be just on top of the raised window (layers...) */
        client_sibling = clientGetNextTopMost(c->win_layer, c);
        windows_stack = g_slist_remove(windows_stack, (gconstpointer) c);
        if(client_sibling)
        {
            /* If there is one, look for its place in the list */
            sibling = g_slist_find(windows_stack, (gconstpointer) client_sibling);
            /* Place the raised window just before it */
            windows_stack = g_slist_insert_before(windows_stack, sibling, c);
        }
        else
        {
            /* There will be no window on top of the raised window, so place it at the end of list */
            windows_stack = g_slist_append(windows_stack, c);
        }
        /* Now, look for transients, transients of transients, etc. */
        for(index1 = windows_stack_copy; index1; index1 = g_slist_next(index1))
        {
            c2 = (Client *) index1->data;
            if(c2)
            {
                if((c2 != c) && (c2->transient_for == c->window))
                {
                    transients = g_slist_append(transients, c2);
                    if(sibling)
                    {
                        /* Place the transient window just before sibling */
                        windows_stack = g_slist_remove(windows_stack, (gconstpointer) c2);
                        windows_stack = g_slist_insert_before(windows_stack, sibling, c2);
                    }
                    else
                    {
                        /* There will be no window on top of the transient window, so place it at the end of list */
                        windows_stack = g_slist_remove(windows_stack, (gconstpointer) c2);
                        windows_stack = g_slist_append(windows_stack, c2);
                    }
                }
                else
                {
                    for(index2 = transients; index2; index2 = g_slist_next(index2))
                    {
                        c3 = (Client *) index2->data;
                        if((c3 != c2) && (c2->transient_for == c3->window))
                        {
                            transients = g_slist_append(transients, c2);
                            if(sibling)
                            {
                                /* Place the transient window just before sibling */
                                windows_stack = g_slist_remove(windows_stack, (gconstpointer) c2);
                                windows_stack = g_slist_insert_before(windows_stack, sibling, c2);
                            }
                            else
                            {
                                /* There will be no window on top of the transient window, so place it at the end of list */
                                windows_stack = g_slist_remove(windows_stack, (gconstpointer) c2);
                                windows_stack = g_slist_append(windows_stack, c2);
                            }
                            break;
                        }
                    }
                }
            }
        }
        if(transients)
        {
            g_slist_free(transients);
        }
        if(windows_stack_copy)
        {
            g_slist_free(windows_stack_copy);
        }
        /* Now, windows_stack contains the correct window stack
           We still need to tell the X Server to reflect the changes 
         */
        clientApplyStackList(windows_stack);
        clientSetNetClientList(net_client_list_stacking, windows_stack);
        last_raise = c;
    }
}

void clientLower(Client * c)
{
    g_return_if_fail(c != NULL);
    TRACE("entering clientLower");
    TRACE("lowering client \"%s\" (0x%lx)", c->name, c->window);

    if(g_slist_length(windows_stack) < 1)
    {
        return;
    }

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MANAGED))
    {
        Client *client_sibling = NULL;

        if(c->transient_for)
        {
            client_sibling = clientGetFromWindow(c->transient_for, WINDOW);
        }
        if(!client_sibling)
        {
            client_sibling = clientGetBottomMost(c->win_layer, c);
        }
        windows_stack = g_slist_remove(windows_stack, (gconstpointer) c);
        if(client_sibling)
        {
            GSList *sibling = g_slist_find(windows_stack, (gconstpointer) client_sibling);
            gint position = g_slist_position(windows_stack, sibling) + 1;
            windows_stack = g_slist_insert(windows_stack, c, position);
            TRACE("lowest client is \"%s\" (0x%lx) at position %i", client_sibling->name, client_sibling->window, position);
        }
        else
        {
            windows_stack = g_slist_prepend(windows_stack, c);
        }
        /* Now, windows_stack contains the correct window stack
           We still need to tell the X Server to reflect the changes 
         */
        clientApplyStackList(windows_stack);
        clientSetNetClientList(net_client_list_stacking, windows_stack);
        if(last_raise == c)
        {
            last_raise = NULL;
        }
    }
}

void clientSetLayer(Client * c, int l)
{
    GSList *list_of_windows = NULL;
    GSList *index;
    Client *c2;

    g_return_if_fail(c != NULL);
    TRACE("entering clientSetLayer");

    list_of_windows = clientListTransients(c);
    for(index = list_of_windows; index; index = g_slist_next(index))
    {
        c2 = (Client *) index->data;
        if(c2->win_layer != l)
        {
            TRACE("setting client \"%s\" (0x%lx) layer to %d", c2->name, c2->window, l);
            c2->win_layer = l;
            setGnomeHint(dpy, c2->window, win_layer, l);
        }
    }
    g_slist_free(list_of_windows);
    if(last_raise == c)
    {
        last_raise = NULL;
    }
    clientRaise(c);
}

static inline void clientSetWorkspaceSingle(Client * c, int ws)
{
    g_return_if_fail(c != NULL);
    
    TRACE("entering clientSetWorkspaceSingle");

    if(ws > params.workspace_count - 1)
    {
        ws = params.workspace_count - 1;
        TRACE("value off limits, using %i instead", ws);
    }

    if(c->win_workspace != ws)
    {
        TRACE("setting client \"%s\" (0x%lx) to workspace %d", c->name, c->window, ws);
        c->win_workspace = ws;
        setGnomeHint(dpy, c->window, win_workspace, ws);
        setNetHint(dpy, c->window, net_wm_desktop, (unsigned long)c->win_workspace);
        CLIENT_FLAG_SET(c, CLIENT_FLAG_WORKSPACE_SET);
    }
}

void clientSetWorkspace(Client * c, int ws, gboolean manage_mapping)
{
    GSList *list_of_windows = NULL;
    GSList *index;
    Client *c2;

    g_return_if_fail(c != NULL);
    
    TRACE("entering clientSetWorkspace");

    list_of_windows = clientListTransients(c);
    for(index = list_of_windows; index; index = g_slist_next(index))
    {
        c2 = (Client *) index->data;
        if(c2->win_workspace != ws)
        {
            clientSetWorkspaceSingle(c2, ws);
            if(manage_mapping && !(c2->transient_for) && !CLIENT_FLAG_TEST(c2, CLIENT_FLAG_HIDDEN))
            {
                if(CLIENT_FLAG_TEST(c2, CLIENT_FLAG_STICKY))
                {
                    clientShow(c2, False);
                }
                else
                {
                    if(ws == workspace)
                    {
                        clientShow(c2, False);
                    }
                    else
                    {
                        clientHide(c2, False);
                    }
                }
            }
        }
    }
    g_slist_free(list_of_windows);
}

void clientShade(Client * c)
{
    XWindowChanges wc;

    g_return_if_fail(c != NULL);
    TRACE("entering clientToggleShaded");
    TRACE("shading client \"%s\" (0x%lx)", c->name, c->window);

    if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_HAS_BORDER) || CLIENT_FLAG_TEST(c, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE("cowardly refusing to shade \"%s\" (0x%lx) because it has no border", c->name, c->window);
        return;
    }
    else if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_SHADED))
    {
        TRACE("\"%s\" (0x%lx) is already shaded", c->name, c->window);
        return;
    }

    c->win_state |= WIN_STATE_SHADED;
    CLIENT_FLAG_SET(c, CLIENT_FLAG_SHADED);
    clientSetNetState(c);
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MANAGED))
    {
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure(c, &wc, CWWidth | CWHeight, FALSE);
    }
}

void clientUnshade(Client * c)
{
    XWindowChanges wc;

    g_return_if_fail(c != NULL);
    TRACE("entering clientToggleShaded");
    TRACE("shading/unshading client \"%s\" (0x%lx)", c->name, c->window);

    if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_SHADED))
    {
        TRACE("\"%s\" (0x%lx) is not shaded", c->name, c->window);
        return;
    }
    c->win_state &= ~WIN_STATE_SHADED;
    CLIENT_FLAG_UNSET(c, CLIENT_FLAG_SHADED);
    clientSetNetState(c);
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MANAGED))
    {
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure(c, &wc, CWWidth | CWHeight, FALSE);
    }
}

void clientToggleShaded(Client * c)
{
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_SHADED))
    {
        clientUnshade(c);
    }
    else
    {
        clientShade(c);
    }
}

void clientStick(Client * c, gboolean include_transients)
{
    GSList *list_of_windows = NULL;
    GSList *index;
    Client *c2 = NULL;

    g_return_if_fail(c != NULL);
    TRACE("entering clientStick");
    TRACE("sticking client \"%s\" (0x%lx)", c->name, c->window);

    if(include_transients)
    {
        list_of_windows = clientListTransients(c);
        for(index = list_of_windows; index; index = g_slist_next(index))
        {
            c2 = (Client *) index->data;
            c2->win_state |= WIN_STATE_STICKY;
            CLIENT_FLAG_SET(c2, CLIENT_FLAG_STICKY);
            clientSetNetState(c2);
        }
        clientSetWorkspace(c, workspace, TRUE);
        g_slist_free(list_of_windows);
    }
    else
    {
        c->win_state |= WIN_STATE_STICKY;
        CLIENT_FLAG_SET(c, CLIENT_FLAG_STICKY);
        clientSetNetState(c);
        clientSetWorkspace(c, workspace, TRUE);
    }
}

void clientUnstick(Client * c, gboolean include_transients)
{
    GSList *list_of_windows = NULL;
    GSList *index;
    Client *c2 = NULL;

    g_return_if_fail(c != NULL);
    TRACE("entering clientUnstick");
    TRACE("unsticking client \"%s\" (0x%lx)", c->name, c->window);

    if(include_transients)
    {
        list_of_windows = clientListTransients(c);
        for(index = list_of_windows; index; index = g_slist_next(index))
        {
            c2 = (Client *) index->data;
            c2->win_state &= ~WIN_STATE_STICKY;
            CLIENT_FLAG_UNSET(c2, CLIENT_FLAG_STICKY);
            clientSetNetState(c2);
        }
        clientSetWorkspace(c, workspace, TRUE);
        g_slist_free(list_of_windows);
    }
    else
    {
        c->win_state &= ~WIN_STATE_STICKY;
        CLIENT_FLAG_UNSET(c, CLIENT_FLAG_STICKY);
        clientSetNetState(c);
        clientSetWorkspace(c, workspace, TRUE);
    }
}

void clientToggleSticky(Client * c, gboolean include_transients)
{
    g_return_if_fail(c != NULL);
    TRACE("entering clientToggleSticky");
    TRACE("sticking/unsticking client \"%s\" (0x%lx)", c->name, c->window);

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_STICKY))
    {
        clientUnstick(c, include_transients);
    }
    else
    {
        clientStick(c, include_transients);
    }
}

inline void clientRemoveMaximizeFlag(Client * c)
{
    g_return_if_fail(c != NULL);
    TRACE("entering clientRemoveMaximizeFlag");
    TRACE("Removing maximize flag on client \"%s\" (0x%lx)", c->name, c->window);

    c->win_state &= ~(WIN_STATE_MAXIMIZED | WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED_HORIZ);
    CLIENT_FLAG_UNSET(c, CLIENT_FLAG_MAXIMIZED);
    frameDraw(c, FALSE, FALSE);
    clientSetNetState(c);
}

void clientToggleMaximized(Client * c, int mode)
{
    XWindowChanges wc;

    g_return_if_fail(c != NULL);
    TRACE("entering clientToggleMaximized");
    TRACE("maximzing/unmaximizing client \"%s\" (0x%lx)", c->name, c->window);

    if(!CLIENT_CAN_MAXIMIZE_WINDOW(c))
    {
        return;
    }

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED))
    {
        wc.width = c->old_width;
        wc.height = c->old_height;
        wc.x = c->old_x;
        wc.y = c->old_y;
        c->win_state &= ~WIN_STATE_MAXIMIZED;
        CLIENT_FLAG_UNSET(c, CLIENT_FLAG_MAXIMIZED);
    }
    else
    {
        int cx, cy, left, right, top, bottom;

        c->old_x = c->x;
        c->old_y = c->y;
        c->old_width = c->width;
        c->old_height = c->height;

        cx = frameX(c) + (frameWidth(c) >> 1);
        cy = frameY(c) + (frameHeight(c) >> 1);

        left = (isLeftMostHead(dpy, screen, cx, cy) ? MAX((int)margins[MARGIN_LEFT], params.xfwm_margins[MARGIN_LEFT]) : 0);
        right = (isRightMostHead(dpy, screen, cx, cy) ? MAX((int)margins[MARGIN_RIGHT], params.xfwm_margins[MARGIN_RIGHT]) : 0);
        top = (isTopMostHead(dpy, screen, cx, cy) ? MAX((int)margins[MARGIN_TOP], params.xfwm_margins[MARGIN_TOP]) : 0);
        bottom = (isBottomMostHead(dpy, screen, cx, cy) ? MAX((int)margins[MARGIN_BOTTOM], params.xfwm_margins[MARGIN_BOTTOM]) : 0);

        if(mode != WIN_STATE_MAXIMIZED_HORIZ)
        {
            wc.x = MyDisplayX(cx, cy) + frameLeft(c) + left;
            wc.width = MyDisplayWidth(dpy, screen, cx, cy) - frameLeft(c) - frameRight(c) - left - right;
            c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
            CLIENT_FLAG_SET(c, CLIENT_FLAG_MAXIMIZED_HORIZ);
        }
        else
        {
            wc.x = c->x;
            wc.width = c->width;
        }
        if(mode != WIN_STATE_MAXIMIZED_VERT)
        {
            wc.y = MyDisplayY(cx, cy) + frameTop(c) + top;
            wc.height = MyDisplayHeight(dpy, screen, cx, cy) - frameTop(c) - frameBottom(c) - top - bottom;
            c->win_state |= WIN_STATE_MAXIMIZED_VERT;
            CLIENT_FLAG_SET(c, CLIENT_FLAG_MAXIMIZED_VERT);
        }
        else
        {
            wc.y = c->y;
            wc.height = c->height;
        }
    }
    clientSetNetState(c);
    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MANAGED))
    {
        clientConfigure(c, &wc, CWX | CWY | CWWidth | CWHeight, FALSE);
    }
    else
    {
        c->x = wc.x;
        c->y = wc.y;
        c->height = wc.height;
        c->width = wc.width;
    }
}

inline gboolean clientAcceptFocus(Client * c)
{
    g_return_val_if_fail(c != NULL, FALSE);

    TRACE("entering clientAcceptFocus");

    /* First check GNOME protocol */
    if(c->win_hints & WIN_HINTS_SKIP_FOCUS)
    {
        return FALSE;
    }
    /* then try ICCCM */
    else if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_WM_TAKEFOCUS))
    {
        return TRUE;
    }
    /* At last, use wmhints */
    return (CLIENT_FLAG_TEST(c, CLIENT_FLAG_WM_INPUT) ? TRUE : FALSE);
}

void clientUpdateFocus(Client * c)
{
    Client *c2 = ((client_focus != c) ? client_focus : NULL);
    unsigned long data[2];

    TRACE("entering clientUpdateFocus");

    if((c) && !clientAcceptFocus(c))
    {
        TRACE("SKIP_FOCUS set for client \"%s\" (0x%lx)", c->name, c->window);
        return;
    }
    if(c == client_focus)
    {
        TRACE("client \"%s\" (0x%lx) is already focused, ignoring request", c->name, c->window);
        return;
    }
    client_focus = c;
    clientInstallColormaps(c);
    if(c)
    {
        data[0] = c->window;
    }
    else
    {
        data[0] = None;
    }
    if(c2)
    {
        TRACE("redrawing previous focus client \"%s\" (0x%lx)", c2->name, c2->window);
        frameDraw(c2, FALSE, FALSE);
    }
    data[1] = None;
    XChangeProperty(dpy, root, net_active_window, XA_WINDOW, 32, PropModeReplace, (unsigned char *)data, 2);
}

void clientSetFocus(Client * c, gboolean sort)
{
    Client *tmp;
    Client *c2 = ((client_focus != c) ? client_focus : NULL);
    unsigned long data[2];

    TRACE("entering clientSetFocus");

    if((c) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_VISIBLE))
    {
        TRACE("setting focus to client \"%s\" (0x%lx)", c->name, c->window);
        if(!clientAcceptFocus(c))
        {
            TRACE("SKIP_FOCUS set for client \"%s\" (0x%lx)", c->name, c->window);
            return;
        }
        if(c == client_focus)
        {
            TRACE("client \"%s\" (0x%lx) is already focused, ignoring request", c->name, c->window);
            return;
        }
        client_focus = c;
        clientInstallColormaps(c);
        if(sort)
        {
            TRACE("Sorting...");
            if(client_count > 2 && c != clients)
            {
                tmp = c;
                c->prev->next = c->next;
                c->next->prev = c->prev;

                c->prev = clients->prev;
                c->next = clients;
                clients->prev->next = c;
                clients->prev = c;
            }
            clients = c;
        }
        XSetInputFocus(dpy, c->window, RevertToNone, CurrentTime);
        data[0] = c->window;
    }
    else
    {
        TRACE("setting focus to none");
        client_focus = NULL;
        XSetInputFocus(dpy, gnome_win, RevertToNone, CurrentTime);
        data[0] = None;
    }
    if(c2)
    {
        TRACE("redrawing previous focus client \"%s\" (0x%lx)", c2->name, c2->window);
        frameDraw(c2, FALSE, FALSE);
    }
    data[1] = None;
    XChangeProperty(dpy, root, net_active_window, XA_WINDOW, 32, PropModeReplace, (unsigned char *)data, 2);
}

Client *clientGetFocus(void)
{
    return (client_focus);
}

void clientDrawOutline(Client * c)
{
    TRACE("entering clientDrawOutline");

    XDrawRectangle(dpy, root, params.box_gc, frameX(c), frameY(c), frameWidth(c) - 1, frameHeight(c) - 1);
    if(CLIENT_FLAG_TEST_AND_NOT(c, CLIENT_FLAG_HAS_BORDER, CLIENT_FLAG_FULLSCREEN | CLIENT_FLAG_SHADED))
    {
        XDrawRectangle(dpy, root, params.box_gc, c->x, c->y, c->width - 1, c->height - 1);
    }
}

static inline void clientSnapPosition(Client * c)
{
    int cx, cy, left, right, top, bottom;
    int disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_x, frame_y, frame_height, frame_width;
    int frame_top, frame_left, frame_right, frame_bottom;

    g_return_if_fail(c != NULL);
    TRACE("entering clientSnapPosition");
    TRACE("Snapping client \"%s\" (0x%lx)", c->name, c->window);

    frame_x = frameX(c);
    frame_y = frameY(c);
    frame_height = frameHeight(c);
    frame_width = frameWidth(c);
    frame_top = frameTop(c);
    frame_left = frameLeft(c);
    frame_right = frameRight(c);
    frame_bottom = frameBottom(c);

    cx = frame_x + (frame_width >> 1);
    cy = frame_y + (frame_height >> 1);

    left = (isLeftMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_LEFT] : 0);
    right = (isRightMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_RIGHT] : 0);
    top = (isTopMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_TOP] : 0);
    bottom = (isBottomMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_BOTTOM] : 0);

    if (!(params.snap_to_border) && !(params.snap_to_windows))
    {
        if((frame_y + frame_top > 0) && (frame_y < top))
        {
            c->y = frame_top + top;
        }
    }
    else
    {
        int i;
        int frame_x2 = frame_x + frame_width;
        int frame_y2 = frame_y + frame_height;
        int best_frame_x = frame_x;
        int best_frame_y = frame_y;
        int best_delta_x = params.snap_width + 1;
        int best_delta_y = params.snap_width + 1;
        int c_frame_x1, c_frame_x2, c_frame_y1, c_frame_y2;
        int delta;
        Client * c2;

        disp_x = MyDisplayX(cx, cy);
        disp_y = MyDisplayY(cx, cy);
        disp_max_x = MyDisplayMaxX(dpy, screen, cx, cy);
        disp_max_y = MyDisplayMaxY(dpy, screen, cx, cy);

        if (params.snap_to_border)
        {
            if (abs(disp_x + left - frame_x) < abs(disp_max_x - right - frame_x2))
            {
                best_delta_x = abs(disp_x + left - frame_x);
                best_frame_x = disp_x + left;
            }
            else
            {
                best_delta_x = abs(disp_max_x - right - frame_x2);
                best_frame_x = disp_max_x - right - frame_width;
            }

            if (abs(disp_y + top - frame_y) < abs(disp_max_y - bottom - frame_y2))
            {
                best_delta_y = abs(disp_y + top - frame_y);
                best_frame_y = disp_y + top;
            }
            else
            {
                best_delta_y = abs(disp_max_y - bottom - frame_y2);
                best_frame_y = disp_max_y - bottom - frame_height;
            }
        }

        if(params.snap_to_windows)
        {
            for(c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
            {
                if (CLIENT_FLAG_TEST(c2, CLIENT_FLAG_VISIBLE) && (c2 != c))
                {
                    c_frame_x1 = frameX(c2);
                    c_frame_x2 = c_frame_x1 + frameWidth(c2);
                    c_frame_y1 = frameY(c2);
                    c_frame_y2 = c_frame_y1 + frameHeight(c2);

                    if ((c_frame_y1 <= frame_y2) && (c_frame_y2 >= frame_y))
                    {
                        delta = abs(c_frame_x2 - frame_x);
                        if (delta < best_delta_x)
                        {
                            best_delta_x = delta;
                            best_frame_x = c_frame_x2;
                        }

                        delta = abs(c_frame_x1 - frame_x2);
                        if (delta < best_delta_x)
                        {
                            best_delta_x = delta;
                            best_frame_x = c_frame_x1 - frame_width;
                        }
                    }

                    if ((c_frame_x1 <= frame_x2) && (c_frame_x2 >= frame_x))
                    {
                        delta = abs(c_frame_y2 - frame_y);
                        if (delta < best_delta_y)
                        {
                            best_delta_y = delta;
                            best_frame_y = c_frame_y2;
                        }

                        delta = abs(c_frame_y1 - frame_y2);
                        if (delta < best_delta_y)
                        {
                            best_delta_y = delta;
                            best_frame_y = c_frame_y1 - frame_height;
                        }
                    }
                }
            }
        }

        if (best_delta_x <= params.snap_width)
        {
            c->x = best_frame_x + frame_left;
        }
        if (best_delta_y <= params.snap_width)
        {
            c->y = best_frame_y + frame_top;
        }
        if ((frame_y + frame_top > 0) && (frame_y < top))
        {
            c->y = frame_top + top;
        }
    }
}

static GtkToXEventFilterStatus clientMove_event_filter(XEvent * xevent, gpointer data)
{
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    MoveResizeData *passdata = (MoveResizeData *) data;
    Client *c = passdata->c;
    gboolean moving = TRUE;
    XWindowChanges wc;

    TRACE("entering clientMove_event_filter");

    if(xevent->type == KeyPress)
    {
        if(!passdata->grab && params.box_move)
        {
            MyXGrabServer();
            passdata->grab = TRUE;
            clientDrawOutline(c);
        }
        if(params.box_move)
        {
            clientDrawOutline(c);
        }
        if(xevent->xkey.keycode == params.keys[KEY_MOVE_LEFT].keycode)
        {
            c->x = c->x - 16;
        }
        if(xevent->xkey.keycode == params.keys[KEY_MOVE_RIGHT].keycode)
        {
            c->x = c->x + 16;
        }
        if(xevent->xkey.keycode == params.keys[KEY_MOVE_UP].keycode)
        {
            c->y = c->y - 16;
        }
        if(xevent->xkey.keycode == params.keys[KEY_MOVE_DOWN].keycode)
        {
            c->y = c->y + 16;
        }
        if(CONSTRAINED_WINDOW(c))
        {
            clientConstraintPos(c, FALSE);
        }
        if(params.box_move)
        {
            clientDrawOutline(c);
        }
        else
        {
            wc.x = c->x;
            wc.y = c->y;
            clientConfigure(c, &wc, CWX | CWY, FALSE);
        }
    }
    else if(passdata->use_keys && xevent->type == KeyRelease)
    {
        if(IsModifierKey(XKeycodeToKeysym(dpy, xevent->xkey.keycode, 0)))
        {
            moving = FALSE;
        }
    }
    else if(xevent->type == MotionNotify)
    {
        while(XCheckMaskEvent(dpy, ButtonMotionMask | PointerMotionMask | PointerMotionHintMask, xevent))
            ;

        if(xevent->type == ButtonRelease)
        {
            moving = FALSE;
        }

        if(!passdata->grab && params.box_move)
        {
            MyXGrabServer();
            passdata->grab = TRUE;
            clientDrawOutline(c);
        }
        if(params.box_move)
        {
            clientDrawOutline(c);
        }

        if((params.workspace_count > 1) && !(c->transient_for))
        {
            static gboolean wrapped = FALSE;
            int msx, msy;

            msx = xevent->xmotion.x_root;
            msy = xevent->xmotion.y_root;

            if((msx == 0) && params.wrap_workspaces && !wrapped)
            {
                XWarpPointer(dpy, None, root, 0, 0, 0, 0, gdk_screen_width() - 11, msy);
                msx = xevent->xmotion.x_root = gdk_screen_width() - 11;
                workspaceSwitch(workspace - 1, c);
                wrapped = TRUE;
            }
            else if((msx == gdk_screen_width() - 1) && params.wrap_workspaces && !wrapped)
            {
                XWarpPointer(dpy, None, root, 0, 0, 0, 0, 10, msy);
                msx = xevent->xmotion.x_root = 10;
                workspaceSwitch(workspace + 1, c);
                wrapped = TRUE;
            }
            else if (wrapped)
            {
                wrapped = FALSE;
            }
        }

        c->x = passdata->ox + (xevent->xmotion.x_root - passdata->mx);
        c->y = passdata->oy + (xevent->xmotion.y_root - passdata->my);

        clientSnapPosition(c);

        if(CONSTRAINED_WINDOW(c))
        {
            clientConstraintPos(c, FALSE);
        }
        if(params.box_move)
        {
            clientDrawOutline(c);
        }
        else
        {
            wc.x = c->x;
            wc.y = c->y;
            clientConfigure(c, &wc, CWX | CWY, FALSE);
        }
    }
    else if(!passdata->use_keys && xevent->type == ButtonRelease)
    {
        moving = FALSE;
    }
    else if(xevent->type == UnmapNotify && xevent->xunmap.window == c->window)
    {
        moving = FALSE;
    }
    else if((xevent->type == EnterNotify) || (xevent->type == LeaveNotify))
    {
        /* Ignore enter/leave events */
    }
    else
    {
        status = XEV_FILTER_CONTINUE;
    }

    TRACE("leaving clientMove_event_filter");

    if(!moving)
    {
        TRACE("event loop now finished");
        gtk_main_quit();
    }

    return status;
}

void clientMove(Client * c, XEvent * e)
{
    XWindowChanges wc;
    Time timestamp;
    MoveResizeData passdata;
    int g1 = GrabSuccess, g2 = GrabSuccess;

    g_return_if_fail(c != NULL);
    TRACE("entering clientDoMove");
    TRACE("moving client \"%s\" (0x%lx)", c->name, c->window);

    passdata.ox = c->x;
    passdata.oy = c->y;
    passdata.c = c;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    /*
       The following trick is experimental, based on a patch for Kwin 3.1alpha1 by aviv bergman

       It is supposed to reduce latency during move/resize by mapping a screen large window that
       receives all pointer events.

       Original mail message is available here :

       http://www.xfree86.org/pipermail/xpert/2002-July/019434.html

       Note:

       I'm note sure it makes any difference, but who knows... It doesn' t hurt.
     */

    passdata.tmp_event_window = setTmpEventWin(ButtonMotionMask | ButtonReleaseMask);

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED))
    {
        clientRemoveMaximizeFlag(c);
    }

    if(e->type == KeyPress)
    {
        passdata.use_keys = True;
        timestamp = e->xkey.time;
        passdata.mx = e->xkey.x_root;
        passdata.my = e->xkey.y_root;
        g1 = XGrabKeyboard(dpy, passdata.tmp_event_window, False, GrabModeAsync, GrabModeAsync, timestamp);
        g2 = XGrabPointer(dpy, passdata.tmp_event_window, False, ButtonMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, move_cursor, timestamp);
    }
    else if(e->type == ButtonPress)
    {
        timestamp = e->xbutton.time;
        passdata.mx = e->xbutton.x_root;
        passdata.my = e->xbutton.y_root;
        g2 = XGrabPointer(dpy, passdata.tmp_event_window, False, ButtonMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, timestamp);
    }
    else
    {
        timestamp = CurrentTime;
        getMouseXY(root, &passdata.mx, &passdata.my);
        g2 = XGrabPointer(dpy, passdata.tmp_event_window, False, ButtonMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, move_cursor, timestamp);
    }

    if(((passdata.use_keys) && (g1 != GrabSuccess)) || (g2 != GrabSuccess))
    {
        TRACE("grab failed in clientMove");
        gdk_beep();
        if((passdata.use_keys) && (g1 == GrabSuccess))
        {
            XUngrabKeyboard(dpy, timestamp);
        }
        if(g2 == GrabSuccess)
        {
            XUngrabPointer(dpy, timestamp);
        }
        removeTmpEventWin(passdata.tmp_event_window);
        return;
    }

    if(passdata.use_keys)
    {
        XPutBackEvent(dpy, e);
    }

    CLIENT_FLAG_SET(c, CLIENT_FLAG_MOVING_RESIZING);
    TRACE("entering move loop");
    pushEventFilter(clientMove_event_filter, &passdata);
    gtk_main();
    popEventFilter();
    TRACE("leaving move loop");
    CLIENT_FLAG_UNSET(c, CLIENT_FLAG_MOVING_RESIZING);

    if(passdata.use_keys)
    {
        XUngrabKeyboard(dpy, CurrentTime);
    }
    XUngrabPointer(dpy, CurrentTime);
    removeTmpEventWin(passdata.tmp_event_window);

    if(passdata.grab && params.box_move)
    {
        clientDrawOutline(c);
    }
    wc.x = c->x;
    wc.y = c->y;
    clientConfigure(c, &wc, CWX | CWY, FALSE);

    if(passdata.grab && params.box_move)
    {
        MyXUngrabServer();
    }
}

static GtkToXEventFilterStatus clientResize_event_filter(XEvent * xevent, gpointer data)
{
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    MoveResizeData *passdata = (MoveResizeData *) data;
    Client *c = passdata->c;
    gboolean resizing = TRUE;
    XWindowChanges wc;
    int prev_y = 0, prev_x = 0;
    int prev_height = 0, prev_width = 0;
    int cx, cy, left, right, top, bottom;
    int disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_x, frame_y, frame_height, frame_width;
    int frame_top, frame_left, frame_right, frame_bottom;

    TRACE("entering clientResize_event_filter");

    frame_x = frameX(c);
    frame_y = frameY(c);
    frame_height = frameHeight(c);
    frame_width = frameWidth(c);
    frame_top = frameTop(c);
    frame_left = frameLeft(c);
    frame_right = frameRight(c);
    frame_bottom = frameBottom(c);

    cx = frame_x + (frame_width >> 1);
    cy = frame_y + (frame_height >> 1);

    left = (isLeftMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_LEFT] : 0);
    right = (isRightMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_RIGHT] : 0);
    top = (isTopMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_TOP] : 0);
    bottom = (isBottomMostHead(dpy, screen, cx, cy) ? (int)margins[MARGIN_BOTTOM] : 0);

    disp_x = MyDisplayX(cx, cy);
    disp_y = MyDisplayY(cx, cy);
    disp_max_x = MyDisplayMaxX(dpy, screen, cx, cy);
    disp_max_y = MyDisplayMaxY(dpy, screen, cx, cy);

    if(xevent->type == KeyPress)
    {
        if(!passdata->grab && params.box_resize)
        {
            MyXGrabServer();
            passdata->grab = TRUE;
            clientDrawOutline(c);
        }
        if(params.box_resize)
        {
            clientDrawOutline(c);
        }
        /* Store previous height in case the resize hides the window behind the curtain */
        prev_width = c->width;
        prev_height = c->height;

        if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_SHADED) && (xevent->xkey.keycode == params.keys[KEY_MOVE_UP].keycode))
        {
            c->height = c->height - (clientGetHeightInc(c) < 10 ? 10 : clientGetHeightInc(c));
        }
        else if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_SHADED) && (xevent->xkey.keycode == params.keys[KEY_MOVE_DOWN].keycode))
        {
            c->height = c->height + (clientGetHeightInc(c) < 10 ? 10 : clientGetHeightInc(c));
        }
        else if(xevent->xkey.keycode == params.keys[KEY_MOVE_LEFT].keycode)
        {
            c->width = c->width - (clientGetWidthInc(c) < 10 ? 10 : clientGetWidthInc(c));
        }
        else if(xevent->xkey.keycode == params.keys[KEY_MOVE_RIGHT].keycode)
        {
            c->width = c->width + (clientGetWidthInc(c) < 10 ? 10 : clientGetWidthInc(c));
        }
        if(c->x + c->width < disp_x + left + CLIENT_MIN_VISIBLE)
        {
            c->width = prev_width;
        }
        if(c->y + c->height < disp_y + top + CLIENT_MIN_VISIBLE)
        {
            c->height = prev_height;
        }
        if(params.box_resize)
        {
            clientDrawOutline(c);
        }
        else
        {
            wc.x = c->x;
            wc.y = c->y;
            wc.width = c->width;
            wc.height = c->height;
            clientConfigure(c, &wc, CWX | CWY | CWWidth | CWHeight, FALSE);
        }
    }
    else if((passdata->use_keys) && (xevent->type == KeyRelease))
    {
        if(IsModifierKey(XKeycodeToKeysym(dpy, xevent->xkey.keycode, 0)))
        {
            resizing = FALSE;
        }
    }
    else if(xevent->type == MotionNotify)
    {
        while(XCheckMaskEvent(dpy, ButtonMotionMask | PointerMotionMask | PointerMotionHintMask, xevent))
            ;

        if(xevent->type == ButtonRelease)
        {
            resizing = FALSE;
        }

        if(!passdata->grab && params.box_resize)
        {
            MyXGrabServer();
            passdata->grab = TRUE;
            clientDrawOutline(c);
        }
        if(params.box_resize)
        {
            clientDrawOutline(c);
        }
        passdata->oldw = c->width;
        passdata->oldh = c->height;
        /* Store previous values in case the resize puts the window title off bounds */
        prev_x = c->x;
        prev_y = c->y;
        prev_width = c->width;
        prev_height = c->height;

        if((passdata->corner == CORNER_TOP_LEFT) || (passdata->corner == CORNER_BOTTOM_LEFT) || (passdata->corner == 4 + SIDE_LEFT))
        {
            c->width = (c->x + c->width) - xevent->xmotion.x_root + passdata->mx - frame_left;
        }
        if((passdata->corner == CORNER_BOTTOM_RIGHT) || (passdata->corner == CORNER_TOP_RIGHT) || (passdata->corner == 4 + SIDE_RIGHT))
        {
            c->width = (xevent->xmotion.x_root - c->x) + passdata->mx - frame_right;
        }
        if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_SHADED))
        {
            if((passdata->corner == CORNER_TOP_LEFT) || (passdata->corner == CORNER_TOP_RIGHT))
            {
                c->height = (c->y + c->height) - xevent->xmotion.y_root + passdata->my - frame_top;
            }
            if((passdata->corner == CORNER_BOTTOM_RIGHT) || (passdata->corner == CORNER_BOTTOM_LEFT) || (passdata->corner == 4 + SIDE_BOTTOM))
            {
                c->height = (xevent->xmotion.y_root - c->y) + passdata->my - frame_bottom;
            }
        }
        clientSetWidth(c, c->width);
        clientSetHeight(c, c->height);
        if((passdata->corner == CORNER_TOP_LEFT) || (passdata->corner == CORNER_BOTTOM_LEFT) || (passdata->corner == 4 + SIDE_LEFT))
        {
            c->x = c->x - (c->width - passdata->oldw);
            frame_x = frameX(c);
        }
        if(!CLIENT_FLAG_TEST(c, CLIENT_FLAG_SHADED) && (passdata->corner == CORNER_TOP_LEFT || passdata->corner == CORNER_TOP_RIGHT))
        {
            c->y = c->y - (c->height - passdata->oldh);
            frame_y = frameY(c);
        }
        if((passdata->corner == CORNER_TOP_LEFT) || (passdata->corner == CORNER_TOP_RIGHT))
        {
            if((frame_y < disp_y + top) || (c->y > disp_max_y - bottom - CLIENT_MIN_VISIBLE))
            {
                c->y = prev_y;
                c->height = prev_height;
            }
        }
        else if((passdata->corner == CORNER_BOTTOM_LEFT) || (passdata->corner == CORNER_BOTTOM_RIGHT) || (passdata->corner == 4 + SIDE_BOTTOM))
        {
            if(c->y + c->height < disp_y + top + CLIENT_MIN_VISIBLE)
            {
                c->height = prev_height;
            }
        }
        if((passdata->corner == CORNER_TOP_LEFT) || (passdata->corner == CORNER_BOTTOM_LEFT) || (passdata->corner == 4 + SIDE_LEFT))
        {
            if(c->x > disp_max_x - right - CLIENT_MIN_VISIBLE)
            {
                c->x = prev_x;
                c->width = prev_width;
            }
        }
        else if((passdata->corner == CORNER_TOP_RIGHT) || (passdata->corner == CORNER_BOTTOM_RIGHT) || (passdata->corner == 4 + SIDE_RIGHT))
        {
            if(c->x + c->width < disp_x + left + CLIENT_MIN_VISIBLE)
            {
                c->width = prev_width;
            }
        }
        if(params.box_resize)
        {
            clientDrawOutline(c);
        }
        else
        {
            wc.x = c->x;
            wc.y = c->y;
            wc.width = c->width;
            wc.height = c->height;
            clientConfigure(c, &wc, CWX | CWY | CWWidth | CWHeight, FALSE);
        }
    }
    else if(xevent->type == ButtonRelease)
    {
        resizing = FALSE;
    }
    else if((xevent->type == UnmapNotify) && (xevent->xunmap.window == c->window))
    {
        resizing = FALSE;
    }
    else if((xevent->type == EnterNotify) || (xevent->type == LeaveNotify))
    {
        /* Ignore enter/leave events */
    }
    else
    {
        status = XEV_FILTER_CONTINUE;
    }

    TRACE("leaving clientResize_event_filter");

    if(!resizing)
    {
        TRACE("event loop now finished");
        gtk_main_quit();
    }

    return status;
}

void clientResize(Client * c, int corner, XEvent * e)
{
    XWindowChanges wc;
    Time timestamp;
    MoveResizeData passdata;
    int g1 = GrabSuccess, g2 = GrabSuccess;

    g_return_if_fail(c != NULL);
    TRACE("entering clientResize");
    TRACE("resizing client \"%s\" (0x%lx)", c->name, c->window);

    passdata.c = c;
    passdata.corner = CORNER_BOTTOM_RIGHT;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    passdata.corner = corner;
    passdata.tmp_event_window = setTmpEventWin(ButtonMotionMask | ButtonReleaseMask);

    if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_MAXIMIZED))
    {
        clientRemoveMaximizeFlag(c);
    }

    if(e->type == KeyPress)
    {
        passdata.use_keys = True;
        timestamp = e->xkey.time;
        passdata.mx = e->xkey.x;
        passdata.my = e->xkey.y;
        g1 = XGrabKeyboard(dpy, passdata.tmp_event_window, False, GrabModeAsync, GrabModeAsync, timestamp);
    }
    else if(e->type == ButtonPress)
    {
        timestamp = e->xbutton.time;
        passdata.mx = e->xbutton.x;
        passdata.my = e->xbutton.y;
    }
    else
    {
        timestamp = CurrentTime;
        getMouseXY(c->frame, &passdata.mx, &passdata.my);
    }
    g2 = XGrabPointer(dpy, passdata.tmp_event_window, False, ButtonMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, resize_cursor[passdata.corner], timestamp);

    if(((passdata.use_keys) && (g1 != GrabSuccess)) || (g2 != GrabSuccess))
    {
        TRACE("grab failed in clientResize");
        gdk_beep();
        if((passdata.use_keys) && (g1 == GrabSuccess))
        {
            XUngrabKeyboard(dpy, timestamp);
        }
        if(g2 == GrabSuccess)
        {
            XUngrabPointer(dpy, timestamp);
        }
        removeTmpEventWin(passdata.tmp_event_window);
        return;
    }

    if(passdata.use_keys)
    {
        XPutBackEvent(dpy, e);
    }
    if((passdata.corner == CORNER_TOP_RIGHT) || (passdata.corner == CORNER_BOTTOM_RIGHT) || (passdata.corner == 4 + SIDE_RIGHT))
    {
        passdata.mx = frameWidth(c) - passdata.mx;
    }
    if((passdata.corner == CORNER_BOTTOM_LEFT) || (passdata.corner == CORNER_BOTTOM_RIGHT) || (passdata.corner == 4 + SIDE_BOTTOM))
    {
        passdata.my = frameHeight(c) - passdata.my;
    }

    CLIENT_FLAG_SET(c, CLIENT_FLAG_MOVING_RESIZING);
    TRACE("entering resize loop");
    pushEventFilter(clientResize_event_filter, &passdata);
    gtk_main();
    popEventFilter();
    TRACE("leaving resize loop");
    CLIENT_FLAG_UNSET(c, CLIENT_FLAG_MOVING_RESIZING);

    if(passdata.use_keys)
    {
        XUngrabKeyboard(dpy, CurrentTime);
    }
    XUngrabPointer(dpy, CurrentTime);
    removeTmpEventWin(passdata.tmp_event_window);

    if(passdata.grab && params.box_resize)
    {
        clientDrawOutline(c);
    }

    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    clientConfigure(c, &wc, CWX | CWY | CWHeight | CWWidth, FALSE);

    if(passdata.grab && params.box_resize)
    {
        MyXUngrabServer();
    }
}

static GtkToXEventFilterStatus clientCycle_event_filter(XEvent * xevent, gpointer data)
{
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    gboolean cycling = TRUE;
    gboolean gone = FALSE;
    ClientCycleData *passdata = (ClientCycleData *) data;

    TRACE("entering clientCycle_event_filter");

    switch (xevent->type)
    {
    case DestroyNotify:
        gone |= (passdata->c == clientGetFromWindow(((XDestroyWindowEvent *) xevent)->window, WINDOW));
        status = XEV_FILTER_CONTINUE;
    case UnmapNotify:
        gone |= (passdata->c == clientGetFromWindow(((XUnmapEvent *) xevent)->window, WINDOW));
        status = XEV_FILTER_CONTINUE;
    case KeyPress:
        if(gone || (xevent->xkey.keycode == params.keys[KEY_CYCLE_WINDOWS].keycode))
        {
            passdata->c = clientGetNext(passdata->c, INCLUDE_HIDDEN | INCLUDE_SKIP_TASKBAR | INCLUDE_SKIP_PAGER);
            if(passdata->c)
            {
                tabwinSetLabel(passdata->tabwin, passdata->c->name);
            }
            else
            {
                cycling = FALSE;
            }
        }
        break;
    case KeyRelease:
        if(IsModifierKey(XKeycodeToKeysym(dpy, xevent->xkey.keycode, 0)))
        {
            cycling = FALSE;
        }
        break;
    case ButtonPress:
    case ButtonRelease:
    case EnterNotify:
    case LeaveNotify:
    case MotionNotify:
        break;
    default:
        status = XEV_FILTER_CONTINUE;
        break;
    }

    if(!cycling)
    {
        TRACE("event loop now finished");
        gtk_main_quit();
    }

    return status;
}

void clientCycle(Client * c)
{
    ClientCycleData passdata;
    int g1, g2;

    g_return_if_fail(c != NULL);
    TRACE("entering clientCycle");

    g1 = XGrabKeyboard(dpy, gnome_win, False, GrabModeAsync, GrabModeAsync, CurrentTime);
    g2 = XGrabPointer(dpy, gnome_win, False, NoEventMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    if((g1 != GrabSuccess) || (g2 != GrabSuccess))
    {
        TRACE("grab failed in clientCycle");
        gdk_beep();
        if(g1 == GrabSuccess)
        {
            XUngrabKeyboard(dpy, CurrentTime);
        }
        if(g2 == GrabSuccess)
        {
            XUngrabPointer(dpy, CurrentTime);
        }
        return;
    }

    passdata.c = clientGetNext(c, INCLUDE_HIDDEN | INCLUDE_SKIP_TASKBAR | INCLUDE_SKIP_PAGER);
    if(passdata.c)
    {
        passdata.tabwin = tabwinCreate(passdata.c->name);
        TRACE("entering cycle loop");
        pushEventFilter(clientCycle_event_filter, &passdata);
        gtk_main();
        popEventFilter();
        TRACE("leaving cycle loop");
        tabwinDestroy(passdata.tabwin);
        g_free(passdata.tabwin);
    }

    XUngrabKeyboard(dpy, CurrentTime);
    XUngrabPointer(dpy, CurrentTime);

    if(passdata.c)
    {
        clientShow(passdata.c, True);
        clientRaise(passdata.c);
        clientSetFocus(passdata.c, True);
    }
}

static GtkToXEventFilterStatus clientButtonPress_event_filter(XEvent * xevent, gpointer data)
{
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    gboolean pressed = TRUE;
    Client *c = ((ButtonPressData *) data)->c;
    int b = ((ButtonPressData *) data)->b;

    if(xevent->type == EnterNotify)
    {
        c->button_pressed[b] = True;
        frameDraw(c, FALSE, FALSE);
    }
    else if(xevent->type == LeaveNotify)
    {
        c->button_pressed[b] = False;
        frameDraw(c, FALSE, FALSE);
    }
    else if(xevent->type == ButtonRelease)
    {
        pressed = False;
    }
    else if((xevent->type == UnmapNotify) && (xevent->xunmap.window == c->window))
    {
        pressed = False;
        c->button_pressed[b] = False;
    }
    else if((xevent->type == KeyPress) || (xevent->type == KeyRelease))
    {}
    else
    {
        status = XEV_FILTER_CONTINUE;
    }

    if(!pressed)
    {
        TRACE("event loop now finished");
        gtk_main_quit();
    }

    return status;
}

void clientButtonPress(Client * c, Window w, XButtonEvent * bev)
{
    int b, g1;
    ButtonPressData passdata;

    g_return_if_fail(c != NULL);
    TRACE("entering clientButtonPress");

    for(b = 0; b < BUTTON_COUNT; b++)
    {
        if(MYWINDOW_XWINDOW(c->buttons[b]) == w)
        {
            break;
        }
    }

    g1 = XGrabPointer(dpy, w, False, ButtonReleaseMask | EnterWindowMask | LeaveWindowMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

    if(g1 != GrabSuccess)
    {
        TRACE("grab failed in clientButtonPress");
        gdk_beep();
        if(g1 == GrabSuccess)
        {
            XUngrabKeyboard(dpy, CurrentTime);
        }
        return;
    }

    passdata.c = c;
    passdata.b = b;

    c->button_pressed[b] = True;
    frameDraw(c, FALSE, FALSE);

    TRACE("entering button press loop");
    pushEventFilter(clientButtonPress_event_filter, &passdata);
    gtk_main();
    popEventFilter();
    TRACE("leaving button press loop");

    XUngrabPointer(dpy, CurrentTime);

    if(c->button_pressed[b])
    {
        c->button_pressed[b] = False;
        switch (b)
        {
        case HIDE_BUTTON:
            if(CLIENT_CAN_HIDE_WINDOW(c))
            {
                clientHide(c, True);
            }
            break;
        case CLOSE_BUTTON:
            if(bev->button == Button3)
            {
                clientKill(c);
            }
            else
            {
                clientClose(c);
            }
            break;
        case MAXIMIZE_BUTTON:
            if(CLIENT_CAN_MAXIMIZE_WINDOW(c))
            {
                if(bev->button == Button1)
                {
                    clientToggleMaximized(c, WIN_STATE_MAXIMIZED);
                }
                else if(bev->button == Button2)
                {
                    clientToggleMaximized(c, WIN_STATE_MAXIMIZED_VERT);
                }
                else if(bev->button == Button3)
                {
                    clientToggleMaximized(c, WIN_STATE_MAXIMIZED_HORIZ);
                }
            }
            break;
        case SHADE_BUTTON:
            clientToggleShaded(c);
            break;
        case STICK_BUTTON:
            clientToggleSticky(c, TRUE);
            break;
        default:
            break;
        }
        frameDraw(c, FALSE, FALSE);
    }
}

Client *clientGetLeader(Client * c)
{
    Client *c2 = NULL;

    TRACE("entering clientGetLeader");
    g_return_val_if_fail(c != NULL, NULL);

    if(c->group_leader != None)
    {
        c2 = clientGetFromWindow(c->group_leader, WINDOW);
    }
    else if(c->client_leader != None)
    {
        c2 = clientGetFromWindow(c->client_leader, WINDOW);
    }
    return c2;
}

GSList *clientGetStackList(void)
{
    GSList *windows_stack_copy = NULL;
    if (windows_stack)
    {
        windows_stack_copy = g_slist_copy(windows_stack);
    }
    return windows_stack_copy;
}

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
char *clientGetStartupId(Client * c)
{
    TRACE("entering clientStartupId");
    g_return_val_if_fail(c != NULL, NULL);

    if(c->startup_id)
    {
        return c->startup_id;
    }
    else
    {
        Client *c2 = NULL;

        c2 = clientGetLeader(c);
        if(c2)
        {
            return c2->startup_id;
        }
    }
    return NULL;
}
#endif
