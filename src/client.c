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
#include "misc.h"
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

#define POINTER_EVENT_MASK \
    ButtonPressMask|\
    ButtonReleaseMask

#define FRAME_EVENT_MASK \
    SubstructureNotifyMask|\
    SubstructureRedirectMask|\
    EnterWindowMask

#define CLIENT_EVENT_MASK \
    StructureNotifyMask|\
    FocusChangeMask|\
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
     !clientIsTransientOrModal(c))

#define CONSTRAINED_WINDOW(c) \
    ((c->win_layer > WIN_LAYER_DESKTOP) && \
     (c->win_layer < WIN_LAYER_ABOVE_DOCK) && \
     !(c->type & (WINDOW_DESKTOP | WINDOW_DOCK)) && \
     !(c->legacy_fullscreen))

#define WINDOW_TYPE_DONT_PLACE \
    WINDOW_DESKTOP | \
    WINDOW_DOCK | \
    WINDOW_SPLASHSCREEN
    
#define WINDOW_TYPE_DIALOG \
    WINDOW_DIALOG | \
    WINDOW_MODAL_DIALOG

/* You don't like that ? Me either, but, hell, it's the way glib lists are designed */
#define XWINDOW_TO_GPOINTER(w)  ((gpointer) (Window) (w))
#define GPOINTER_TO_XWINDOW(p)  ((Window) (p))

Client *clients = NULL;
unsigned int client_count = 0;
unsigned long client_serial = 0;

static GList *windows       = NULL;
static GList *windows_stack = NULL;
static Client *client_focus = NULL;
static Client *last_raise   = NULL;
static Client *last_ungrab  = NULL;

/* Forward decl */
static void clientUpdateFullscreenState (Client * c);
static void clientUpdateAboveState (Client * c);
static void clientUpdateBelowState (Client * c);
static void clientGetNetState (Client * c);
static void clientGetInitialNetWmDesktop (Client * c);
static void clientSetNetClientList (Atom a, GList * list);
static void clientSetNetActions (Client * c);
static void clientWindowType (Client * c);
static void clientGrabButton1 (Client * c);
static void clientUngrabButton1 (Client * c);
static void clientAddToList (Client * c);
static void clientRemoveFromList (Client * c);
static void clientSetWidth (Client * c, int w1);
static void clientSetHeight (Client * c, int h1);
static inline void clientApplyStackList (GList * list);
static inline gboolean clientTransientOrModalHasAncestor (Client * c, int ws);
static inline Client *clientGetLowestTransient (Client * c);
static inline Client *clientGetHighestTransientOrModal (Client * c);
static inline Client *clientGetHighestTransientOrModalFor (Client * c);
static inline Client *clientGetTopMostForGroup (Client * c);
static inline gboolean clientVisibleForGroup (Client * c, int workspace);
static inline Client *clientGetNextTopMost (int layer, Client * exclude);
static inline Client *clientGetTopMostFocusable (int layer, Client * exclude);
static inline Client *clientGetBottomMost (int layer, Client * exclude);
static inline Client *clientGetModalFor (Client * c);
static inline void clientConstrainRatio (Client * c, int w1, int h1, int corner);
static inline void clientConstrainPos (Client * c, gboolean show_full);
static inline void clientKeepVisible (Client * c);
static inline unsigned long overlap (int x0, int y0, int x1, int y1, int tx0,
    int ty0, int tx1, int ty1);
static void clientInitPosition (Client * c);
static inline void clientFree (Client * c);
static inline void clientGetWinState (Client * c);
static inline void clientApplyInitialState (Client * c);
static inline gboolean clientSelectMask (Client * c, int mask);
static GList *clientListTransient (Client * c);
static GList *clientListTransientOrModal (Client * c);
static inline void clientShowSingle (Client * c, gboolean change_state);
static inline void clientHideSingle (Client * c, int ws, gboolean change_state);
static inline void clientSetWorkspaceSingle (Client * c, int ws);
static inline void clientSortRing(Client *c);
static inline void clientSnapPosition (Client * c);
static GtkToXEventFilterStatus clientMove_event_filter (XEvent * xevent,
    gpointer data);
static GtkToXEventFilterStatus clientResize_event_filter (XEvent * xevent,
    gpointer data);
static GtkToXEventFilterStatus clientCycle_event_filter (XEvent * xevent,
    gpointer data);
static GtkToXEventFilterStatus clientButtonPress_event_filter (XEvent *
    xevent, gpointer data);

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
    int cycle_range;
};

typedef struct _ButtonPressData ButtonPressData;
struct _ButtonPressData
{
    int b;
    Client *c;
};

inline Client *
clientGetTransient (Client * c)
{
    Client *c2 = NULL;

    g_return_val_if_fail (c != NULL, NULL);

    TRACE ("entering clientGetTransient");

    if ((c->transient_for) && (c->transient_for != root))
    {
        c2 = clientGetFromWindow (c->transient_for, WINDOW);
        return c2;
    }
    return NULL;
}

inline gboolean
clientIsTransient (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientIsTransient");

    return (((c->transient_for != root) && (c->transient_for != None)) || 
            ((c->transient_for == root) && (c->group_leader != None)));
}

inline gboolean
clientIsModal (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientIsModal");

    return (FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL) && 
            (((c->transient_for != root) && (c->transient_for != None)) ||
             (c->group_leader != None)));
}

inline gboolean
clientIsTransientOrModal (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientIsTransientOrModal");

    return (clientIsTransient(c) || clientIsModal(c));
}

inline gboolean
clientSameGroup (Client * c1, Client * c2)
{
    g_return_val_if_fail (c1 != NULL, FALSE);
    g_return_val_if_fail (c2 != NULL, FALSE);

    TRACE ("entering clientSameGroup");

    return ((c1 != c2) && 
            (((c1->group_leader != None) && 
              (c1->group_leader == c2->group_leader)) ||
             (c1->group_leader == c2->window) ||
             (c2->group_leader == c1->window)));
}

inline gboolean
clientIsTransientFor (Client * c1, Client * c2)
{
    g_return_val_if_fail (c1 != NULL, FALSE);
    g_return_val_if_fail (c2 != NULL, FALSE);

    TRACE ("entering clientIsTransientFor");

    if (c1->transient_for)
    {
        if (c1->transient_for != root)
        {
            return (c1->transient_for == c2->window);
        }
        else
        {
            return (clientSameGroup (c1, c2) && (c1->serial >= c2->serial));
        }
    }
    return FALSE;
}

inline gboolean
clientIsModalFor (Client * c1, Client * c2)
{
    g_return_val_if_fail (c1 != NULL, FALSE);
    g_return_val_if_fail (c2 != NULL, FALSE);

    TRACE ("entering clientIsModalFor");

    if (FLAG_TEST (c1->flags, CLIENT_FLAG_STATE_MODAL))
    {
        return (clientIsTransientFor (c1, c2) || clientSameGroup (c1, c2));
    }
    return FALSE;
}

inline gboolean
clientIsTransientOrModalFor (Client * c1, Client * c2)
{
    g_return_val_if_fail (c1 != NULL, FALSE);
    g_return_val_if_fail (c2 != NULL, FALSE);

    TRACE ("entering clientIsTransientOrModalFor");

    return (clientIsTransientFor(c1, c2) || clientIsModalFor(c1, c2));
}

inline gboolean
clientIsTransientForGroup (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientIsTransientForGroup");

    return ((c->transient_for == root) && (c->group_leader != None));
}

inline gboolean
clientIsModalForGroup (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientIsModalForGroup");

    return (FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL) && 
            !clientIsTransient(c) && (c->group_leader != None));
}

inline gboolean
clientIsTransientOrModalForGroup (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientIsTransientOrModalForGroup");

    return (clientIsTransientForGroup(c) || clientIsModalForGroup(c));
}

void
clientSetNetState (Client * c)
{
    int i;
    Atom data[16];

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetNetState");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    i = 0;
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        TRACE ("clientSetNetState : shaded");
        data[i++] = net_wm_state_shaded;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        TRACE ("clientSetNetState : sticky");
        data[i++] = net_wm_state_sticky;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL))
    {
        TRACE ("clientSetNetState : modal");
        data[i++] = net_wm_state_modal;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_PAGER))
    {
        TRACE ("clientSetNetState : skip_pager");
        data[i++] = net_wm_state_skip_pager;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_TASKBAR))
    {
        TRACE ("clientSetNetState : skip_taskbar");
        data[i++] = net_wm_state_skip_taskbar;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
    {
        TRACE ("clientSetNetState : maximize vert + horiz");
        data[i++] = net_wm_state_maximized_horz;
        data[i++] = net_wm_state_maximized_vert;
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ))
    {
        TRACE ("clientSetNetState : maximize horiz");
        data[i++] = net_wm_state_maximized_horz;
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT))
    {
        TRACE ("clientSetNetState : vert");
        data[i++] = net_wm_state_maximized_vert;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE ("clientSetNetState : fullscreen");
        data[i++] = net_wm_state_fullscreen;
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
    {
        TRACE ("clientSetNetState : above");
        data[i++] = net_wm_state_above;
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
    {
        TRACE ("clientSetNetState : below");
        data[i++] = net_wm_state_below;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_HIDDEN))
    {
        TRACE ("clientSetNetState : hidden");
        data[i++] = net_wm_state_hidden;
    }
    XChangeProperty (dpy, c->window, net_wm_state, XA_ATOM, 32,
        PropModeReplace, (unsigned char *) data, i);
    /*
       We also set GNOME hint here for consistency and convenience, 
       although the meaning of net_wm_state and win_state aren't the same.
     */
    setHint (dpy, c->window, win_state, c->win_state);
}

static void
clientGetNetState (Client * c)
{
    int n_atoms = 0;
    Atom *atoms = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGetNetState");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    if (get_atom_list (dpy, c->window, net_wm_state, &atoms, &n_atoms))
    {
        int i;
        TRACE ("clientGetNetState: %i atoms detected", n_atoms);

        i = 0;
        while (i < n_atoms)
        {
            if ((atoms[i] == net_wm_state_shaded)
                || (FLAG_TEST_ALL (c->flags,
                        CLIENT_FLAG_SESSION_MANAGED | CLIENT_FLAG_SHADED)))
            {
                TRACE ("clientGetNetState : shaded");
                c->win_state |= WIN_STATE_SHADED;
                FLAG_SET (c->flags, CLIENT_FLAG_SHADED);
            }
            else if (atoms[i] == net_wm_state_hidden)
            {
                TRACE ("clientGetNetState : hidden");
                FLAG_SET (c->flags, CLIENT_FLAG_HIDDEN);
            }
            else if ((atoms[i] == net_wm_state_sticky)
                || (FLAG_TEST_ALL (c->flags,
                        CLIENT_FLAG_SESSION_MANAGED | CLIENT_FLAG_STICKY)))
            {
                TRACE ("clientGetNetState : sticky");
                c->win_state |= WIN_STATE_STICKY;
                FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
            }
            else if ((atoms[i] == net_wm_state_maximized_horz)
                || (FLAG_TEST_ALL (c->flags,
                        CLIENT_FLAG_SESSION_MANAGED |
                        CLIENT_FLAG_MAXIMIZED_HORIZ)))
            {
                TRACE ("clientGetNetState : maximized horiz");
                c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
                FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
            }
            else if ((atoms[i] == net_wm_state_maximized_vert)
                || (FLAG_TEST_ALL (c->flags,
                        CLIENT_FLAG_SESSION_MANAGED |
                        CLIENT_FLAG_MAXIMIZED_VERT)))
            {
                TRACE ("clientGetNetState : maximized vert");
                c->win_state |= WIN_STATE_MAXIMIZED_VERT;
                FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
            }
            else if ((atoms[i] == net_wm_state_fullscreen)
                && !FLAG_TEST_ALL (c->flags,
                    CLIENT_FLAG_ABOVE | CLIENT_FLAG_BELOW))
            {
                TRACE ("clientGetNetState : fullscreen");
                FLAG_SET (c->flags, CLIENT_FLAG_FULLSCREEN);
            }
            else if ((atoms[i] == net_wm_state_above)
                && !FLAG_TEST_ALL (c->flags,
                    CLIENT_FLAG_FULLSCREEN | CLIENT_FLAG_BELOW))
            {
                TRACE ("clientGetNetState : above");
                FLAG_SET (c->flags, CLIENT_FLAG_ABOVE);
            }
            else if ((atoms[i] == net_wm_state_below)
                && !FLAG_TEST_ALL (c->flags,
                    CLIENT_FLAG_ABOVE | CLIENT_FLAG_FULLSCREEN))
            {
                TRACE ("clientGetNetState : below");
                FLAG_SET (c->flags, CLIENT_FLAG_BELOW);
            }
            else if (atoms[i] == net_wm_state_modal)
            {
                TRACE ("clientGetNetState : modal");
                FLAG_SET (c->flags, CLIENT_FLAG_STATE_MODAL);
            }
            else if (atoms[i] == net_wm_state_skip_pager)
            {
                TRACE ("clientGetNetState : skip_pager");
                FLAG_SET (c->flags, CLIENT_FLAG_SKIP_PAGER);
            }
            else if (atoms[i] == net_wm_state_skip_taskbar)
            {
                TRACE ("clientGetNetState : skip_taskbar");
                FLAG_SET (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            }
            else
            {
                g_message (_("%s: Unmanaged net_wm_state (window 0x%lx)"),
                    g_get_prgname (), c->window);
            }

            ++i;
        }
        if (atoms)
        {
            XFree (atoms);
        }
    }
}

void
clientUpdateWinState (Client * c, XClientMessageEvent * ev)
{
    unsigned long action;
    Atom add_remove;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateWinState");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    action = ((XEvent *) ev)->xclient.data.l[0];
    add_remove = ((XEvent *) ev)->xclient.data.l[1];

    if (action & WIN_STATE_SHADED)
    {
        TRACE ("client \"%s\" (0x%lx) has received a win_state/shade event",
            c->name, c->window);
        if (add_remove == WIN_STATE_SHADED)
        {
            clientShade (c);
        }
        else
        {
            clientUnshade (c);
        }
    }
    else if ((action & WIN_STATE_STICKY)
        && FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STICK))
    {
        TRACE ("client \"%s\" (0x%lx) has received a win_state/stick event",
            c->name, c->window);
        if (!clientIsTransientOrModal (c))
        {
            if (add_remove == WIN_STATE_STICKY)
            {
                clientStick (c, TRUE);
            }
            else
            {
                clientUnstick (c, TRUE);
            }
            frameDraw (c, FALSE, FALSE);
        }
    }
    else if ((action & WIN_STATE_MAXIMIZED)
        && FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
    {
        TRACE
            ("client \"%s\" (0x%lx) has received a win_state/maximize event",
            c->name, c->window);
        clientToggleMaximized (c, add_remove);
    }
}

void
clientUpdateNetState (Client * c, XClientMessageEvent * ev)
{
    unsigned long action;
    Atom first;
    Atom second;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateNetState");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    action = ((XEvent *) ev)->xclient.data.l[0];
    first = ((XEvent *) ev)->xclient.data.l[1];
    second = ((XEvent *) ev)->xclient.data.l[2];

    if ((first == net_wm_state_shaded) || (second == net_wm_state_shaded))
    {
        if ((action == NET_WM_STATE_ADD)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            clientShade (c);
        }
        else if ((action == NET_WM_STATE_REMOVE)
            && FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            clientUnshade (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            clientToggleShaded (c);
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
    if ((first == net_wm_state_hidden) || (second == net_wm_state_hidden))
    {
        if ((action == NET_WM_STATE_ADD)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_HIDDEN))
        {
            if (CLIENT_CAN_HIDE_WINDOW (c))
            {
                clientHide (c, c->win_workspace, TRUE);
            }
        }
        else if ((action == NET_WM_STATE_REMOVE)
            && FLAG_TEST (c->flags, CLIENT_FLAG_HIDDEN))
        {
            clientShow (c, TRUE);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            if (FLAG_TEST (c->flags, CLIENT_FLAG_HIDDEN))
            {
                clientShow (c, TRUE);
            }
            else if (CLIENT_CAN_HIDE_WINDOW (c))
            {
                clientHide (c, c->win_workspace, TRUE);
            }
        }
    }
#endif

    if ((first == net_wm_state_sticky) || (second == net_wm_state_sticky))
    {
        if (!clientIsTransientOrModal (c)
            && FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STICK))
        {
            if ((action == NET_WM_STATE_ADD)
                && !FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
            {
                clientStick (c, TRUE);
            }
            else if ((action == NET_WM_STATE_REMOVE)
                && FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
            {
                clientUnstick (c, TRUE);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                clientToggleSticky (c, TRUE);
            }
            frameDraw (c, FALSE, FALSE);
        }
    }

    if ((first == net_wm_state_maximized_horz)
        || (second == net_wm_state_maximized_horz)
        || (first == net_wm_state_maximized_vert)
        || (second == net_wm_state_maximized_vert))
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
        {
            if ((action == NET_WM_STATE_ADD)
                && !FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
            {
                unsigned long mode = 0;
                if ((first == net_wm_state_maximized_horz)
                    || (second == net_wm_state_maximized_horz))
                {
                    mode |=
                        !FLAG_TEST (c->flags,
                        CLIENT_FLAG_MAXIMIZED_HORIZ) ?
                        WIN_STATE_MAXIMIZED_HORIZ : 0;
                }
                if ((first == net_wm_state_maximized_vert)
                    || (second == net_wm_state_maximized_vert))
                {
                    mode |=
                        !FLAG_TEST (c->flags,
                        CLIENT_FLAG_MAXIMIZED_VERT) ? WIN_STATE_MAXIMIZED_VERT
                        : 0;
                }
                clientToggleMaximized (c, mode);
            }
            else if ((action == NET_WM_STATE_REMOVE)
                && FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
            {
                unsigned long mode = 0;
                if ((first == net_wm_state_maximized_horz)
                    || (second == net_wm_state_maximized_horz))
                {
                    mode |=
                        FLAG_TEST (c->flags,
                        CLIENT_FLAG_MAXIMIZED_HORIZ) ?
                        WIN_STATE_MAXIMIZED_HORIZ : 0;
                }
                if ((first == net_wm_state_maximized_vert)
                    || (second == net_wm_state_maximized_vert))
                {
                    mode |=
                        FLAG_TEST (c->flags,
                        CLIENT_FLAG_MAXIMIZED_VERT) ? WIN_STATE_MAXIMIZED_VERT
                        : 0;
                }
                clientToggleMaximized (c, mode);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                unsigned long mode = 0;
                if ((first == net_wm_state_maximized_horz)
                    || (second == net_wm_state_maximized_horz))
                {
                    mode |= WIN_STATE_MAXIMIZED_HORIZ;
                }
                if ((first == net_wm_state_maximized_vert)
                    || (second == net_wm_state_maximized_vert))
                {
                    mode |= WIN_STATE_MAXIMIZED_VERT;
                }
                clientToggleMaximized (c, mode);
            }
        }
    }

    if ((first == net_wm_state_modal) || (second == net_wm_state_modal))
    {
        if ((action == NET_WM_STATE_ADD)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState (c);
            clientWindowType (c);
        }
        else if ((action == NET_WM_STATE_REMOVE)
            && FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState (c);
            clientWindowType (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            FLAG_TOGGLE (c->flags, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState (c);
            clientWindowType (c);
        }
    }

    if ((first == net_wm_state_fullscreen)
        || (second == net_wm_state_fullscreen))
    {
        if (!clientIsTransientOrModal (c))
        {
            if ((action == NET_WM_STATE_ADD)
                && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
            {
                FLAG_SET (c->flags, CLIENT_FLAG_FULLSCREEN);
            }
            else if ((action == NET_WM_STATE_REMOVE)
                && FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
            {
                FLAG_UNSET (c->flags, CLIENT_FLAG_FULLSCREEN);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_FULLSCREEN);
            }
            clientUpdateFullscreenState (c);
        }
    }

    if ((first == net_wm_state_above) || (second == net_wm_state_above))
    {
        if (!clientIsTransientOrModal (c) && !FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
        {
            if ((action == NET_WM_STATE_ADD)
                && !FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
            {
                FLAG_SET (c->flags, CLIENT_FLAG_ABOVE);
            }
            else if ((action == NET_WM_STATE_REMOVE)
                && FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
            {
                FLAG_UNSET (c->flags, CLIENT_FLAG_ABOVE);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_ABOVE);
            }
            clientUpdateAboveState (c);
        }
    }

    if ((first == net_wm_state_below) || (second == net_wm_state_below))
    {
        if (!clientIsTransientOrModal (c) && !FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
        {
            if ((action == NET_WM_STATE_ADD)
                && !FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
            {
                FLAG_SET (c->flags, CLIENT_FLAG_BELOW);
            }
            else if ((action == NET_WM_STATE_REMOVE)
                && FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
            {
                FLAG_UNSET (c->flags, CLIENT_FLAG_BELOW);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_BELOW);
            }
            clientUpdateBelowState (c);
        }
    }

    if ((first == net_wm_state_skip_pager)
        || (second == net_wm_state_skip_pager))
    {
        if ((action == NET_WM_STATE_ADD)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_PAGER))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState (c);
        }
        else if ((action == NET_WM_STATE_REMOVE)
            && FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_PAGER))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            FLAG_TOGGLE (c->flags, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState (c);
        }
    }

    if ((first == net_wm_state_skip_taskbar)
        || (second == net_wm_state_skip_taskbar))
    {
        if ((action == NET_WM_STATE_ADD)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_TASKBAR))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState (c);
        }
        else if ((action == NET_WM_STATE_REMOVE)
            && FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_TASKBAR))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            FLAG_TOGGLE (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState (c);
        }
    }
}

void
clientGetNetWmType (Client * c)
{
    int n_atoms = 0;
    Atom *atoms = NULL;
    int i;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGetNetWmType");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    n_atoms = 0;
    atoms = NULL;

    if (!get_atom_list (dpy, c->window, net_wm_window_type, &atoms, &n_atoms))
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
        while (i < n_atoms)
        {
            if ((atoms[i] == net_wm_window_type_desktop)
                || (atoms[i] == net_wm_window_type_dock)
                || (atoms[i] == net_wm_window_type_toolbar)
                || (atoms[i] == net_wm_window_type_menu)
                || (atoms[i] == net_wm_window_type_dialog)
                || (atoms[i] == net_wm_window_type_normal)
                || (atoms[i] == net_wm_window_type_utility)
                || (atoms[i] == net_wm_window_type_splashscreen))
            {
                c->type_atom = atoms[i];
                break;
            }
            ++i;
        }
        if (atoms)
        {
            XFree (atoms);
        }
    }
    clientWindowType (c);
}

static void
clientGetInitialNetWmDesktop (Client * c)
{
    Client *c2 = NULL;
    long val = 0;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGetInitialNetWmDesktop");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    /* This is to make sure that transient are shown with their "ancestor" window */
    c2 = clientGetTransient (c);
    if (c2)
    {
        FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
        c->win_workspace = c2->win_workspace;
        if (FLAG_TEST (c2->flags, CLIENT_FLAG_STICKY))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
            c->win_state |= WIN_STATE_STICKY;
        }
    }
    else
    {
        if (!FLAG_TEST_ALL (c->flags,
                CLIENT_FLAG_SESSION_MANAGED | CLIENT_FLAG_WORKSPACE_SET))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
            c->win_workspace = workspace;
        }
        if (getHint (dpy, c->window, net_wm_desktop, &val))
        {
            TRACE ("atom net_wm_desktop detected");
            if (val == (int) ALL_WORKSPACES)
            {
                if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_STICK,
                        CLIENT_FLAG_STICKY))
                {
                    TRACE
                        ("atom net_wm_desktop specifies window \"%s\" is sticky",
                        c->name);
                    FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
                    c->win_state |= WIN_STATE_STICKY;
                }
                c->win_workspace = workspace;
            }
            else
            {
                TRACE
                    ("atom net_wm_desktop specifies window \"%s\" is on desk %i",
                    c->name, (int) val);
                c->win_workspace = (int) val;
            }
            FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
        }
        else if (getHint (dpy, c->window, win_workspace, &val))
        {
            TRACE ("atom win_workspace specifies window \"%s\" is on desk %i",
                c->name, (int) val);
            c->win_workspace = (int) val;
            FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
        }
    }
    TRACE ("initial desktop for window \"%s\" is %i", c->name,
        c->win_workspace);
    if (c->win_workspace > params.workspace_count - 1)
    {
        TRACE ("value off limits, using %i instead",
            params.workspace_count - 1);
        c->win_workspace = params.workspace_count - 1;
        FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
    }
    TRACE ("initial desktop for window \"%s\" is %i", c->name,
        c->win_workspace);
    setHint (dpy, c->window, win_workspace, c->win_workspace);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        setHint (dpy, c->window, net_wm_desktop,
            (unsigned long) ALL_WORKSPACES);
    }
    else
    {
        setHint (dpy, c->window, net_wm_desktop,
            (unsigned long) c->win_workspace);
    }
}

static void
clientSetNetClientList (Atom a, GList * list)
{
    Window *listw;
    Window *index_dest;
    GList *index_src;
    gint size, i;

    TRACE ("entering clientSetNetClientList");

    size = g_list_length (list);
    if (size < 1)
    {
        XDeleteProperty (dpy, root, a);
    }
    else if ((listw = (Window *) malloc ((size + 1) * sizeof (Window))))
    {
        TRACE ("%i windows in list for %i clients", size, client_count);
        for (i = 0, index_dest = listw, index_src = list; i < size;
            i++, index_dest++, index_src = g_list_next (index_src))
        {
            Client *c = (Client *) index_src->data;
            *index_dest = c->window;
        }
        XChangeProperty (dpy, root, a, XA_WINDOW, 32, PropModeReplace,
            (unsigned char *) listw, size);
        free (listw);
    }
}

void
clientGetNetStruts (Client * c)
{
    gulong *struts = NULL;
    int nitems;
    int i;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGetNetStruts for \"%s\" (0x%lx)", c->name,
        c->window);

    for (i = 0; i < 4; i++)
    {
        c->struts[i] = 0;
    }
    FLAG_UNSET (c->flags, CLIENT_FLAG_HAS_STRUTS);

    if (get_cardinal_list (dpy, c->window, net_wm_strut, &struts, &nitems))
    {
        if (nitems != 4)
        {
            XFree (struts);
            return;
        }

        FLAG_SET (c->flags, CLIENT_FLAG_HAS_STRUTS);
        for (i = 0; i < 4; i++)
        {
            c->struts[i] = (int) struts[i];
        }

        TRACE ("NET_WM_STRUT for window \"%s\"= (%d,%d,%d,%d)", c->name,
            c->struts[0], c->struts[1], c->struts[2], c->struts[3]);
        XFree (struts);
        workspaceUpdateArea (margins, gnome_margins);
    }
}

static void
clientSetNetActions (Client * c)
{
    Atom atoms[6];
    int i = 0;

    atoms[i++] = net_wm_action_close;
    if (CLIENT_CAN_MAXIMIZE_WINDOW (c))
    {
        atoms[i++] = net_wm_action_maximize_horz;
        atoms[i++] = net_wm_action_maximize_vert;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STICK))
    {
        atoms[i++] = net_wm_action_change_desktop;
        atoms[i++] = net_wm_action_stick;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_BORDER))
    {
        atoms[i++] = net_wm_action_shade;
    }
    XChangeProperty (dpy, c->window, net_wm_allowed_actions, XA_ATOM, 32,
        PropModeReplace, (unsigned char *) atoms, i);
}

static void
clientWindowType (Client * c)
{
    WindowType old_type;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientWindowType");
    TRACE ("type for client \"%s\" (0x%lx)", c->name, c->window);

    old_type = c->type;
    c->initial_layer = c->win_layer;
    if (c->type_atom != None)
    {
        if (c->type_atom == net_wm_window_type_desktop)
        {
            TRACE ("atom net_wm_window_type_desktop detected");
            c->type = WINDOW_DESKTOP;
            c->initial_layer = WIN_LAYER_DESKTOP;
            c->win_state |= WIN_STATE_STICKY;
            FLAG_SET (c->flags,
                CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_STICKY |
                CLIENT_FLAG_SKIP_TASKBAR);
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_RESIZE | CLIENT_FLAG_HAS_MOVE | 
                CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE | 
                CLIENT_FLAG_HAS_MENU | CLIENT_FLAG_HAS_STICK |
                CLIENT_FLAG_HAS_BORDER);
        }
        else if (c->type_atom == net_wm_window_type_dock)
        {
            TRACE ("atom net_wm_window_type_dock detected");
            c->type = WINDOW_DOCK;
            c->initial_layer = WIN_LAYER_DOCK;
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_BORDER |  CLIENT_FLAG_HAS_MOVE |
                CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE | 
                CLIENT_FLAG_HAS_MENU | CLIENT_FLAG_HAS_STICK);
        }
        else if (c->type_atom == net_wm_window_type_toolbar)
        {
            TRACE ("atom net_wm_window_type_toolbar detected");
            c->type = WINDOW_TOOLBAR;
            c->initial_layer = WIN_LAYER_NORMAL;
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE |
                CLIENT_FLAG_HAS_STICK);
        }
        else if (c->type_atom == net_wm_window_type_menu)
        {
            TRACE ("atom net_wm_window_type_menu detected");
            c->type = WINDOW_MENU;
            c->initial_layer = WIN_LAYER_NORMAL;
            /* The policy here is unclear :
               http://mail.gnome.org/archives/wm-spec-list/2002-May/msg00001.html
               As it seems, GNOME and KDE don't treat menu the same way...
             */
            FLAG_SET (c->flags,
                CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_SKIP_TASKBAR);
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE |
                CLIENT_FLAG_HAS_STICK);
        }
        else if (c->type_atom == net_wm_window_type_dialog)
        {
            TRACE ("atom net_wm_window_type_dialog detected");
            c->type = WINDOW_DIALOG;
            c->initial_layer = WIN_LAYER_NORMAL;
        }
        else if (c->type_atom == net_wm_window_type_normal)
        {
            TRACE ("atom net_wm_window_type_normal detected");
            c->type = WINDOW_NORMAL;
            c->initial_layer = WIN_LAYER_NORMAL;
        }
        else if (c->type_atom == net_wm_window_type_utility)
        {
            TRACE ("atom net_wm_window_type_utility detected");
            c->type = WINDOW_UTILITY;
            c->initial_layer = WIN_LAYER_NORMAL;
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_STICK);
        }
        else if (c->type_atom == net_wm_window_type_splashscreen)
        {
            TRACE ("atom net_wm_window_type_splashscreen detected");
            c->type = WINDOW_SPLASHSCREEN;
            c->initial_layer = WIN_LAYER_ABOVE_DOCK;
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_HIDE |
                CLIENT_FLAG_HAS_MENU | CLIENT_FLAG_HAS_MOVE |
                CLIENT_FLAG_HAS_RESIZE | CLIENT_FLAG_HAS_STICK);
        }
    }
    else
    {
        TRACE ("no \"net\" atom detected");
        c->type = UNSET;
        c->initial_layer = c->win_layer;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL))
    {
        TRACE ("window is modal");
        c->type = WINDOW_MODAL_DIALOG;
    }

    if (clientIsTransientOrModal (c))
    {
        Client *c2;

        TRACE ("Window \"%s\" is a transient or a modal", c->name);

        c2 = clientGetHighestTransientOrModalFor (c);
        if (c2)
        {
            if (clientIsTransient (c))
            {
                c->initial_layer = c2->win_layer;
            }
            else if (c->initial_layer < c2->win_layer) /* clientIsModal (c) */
            {
                c->initial_layer = c2->win_layer;
            }
            TRACE ("Applied layer is %i", c->initial_layer);
        }
        FLAG_UNSET (c->flags,
            CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_STICK |
            CLIENT_FLAG_STICKY);
    }
    if ((old_type != c->type) || (c->initial_layer != c->win_layer))
    {
        TRACE ("setting layer %i", c->initial_layer);
        clientSetNetState (c);
        clientSetLayer (c, c->initial_layer);
    }
}

void
clientInstallColormaps (Client * c)
{
    XWindowAttributes attr;
    gboolean installed = FALSE;
    int i;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientInstallColormaps");

    if (c->ncmap)
    {
        for (i = c->ncmap - 1; i >= 0; i--)
        {
            XGetWindowAttributes (dpy, c->cmap_windows[i], &attr);
            XInstallColormap (dpy, attr.colormap);
            if (c->cmap_windows[i] == c->window)
            {
                installed = TRUE;
            }
        }
    }
    if ((!installed) && (c->cmap))
    {
        XInstallColormap (dpy, c->cmap);
    }
}

void
clientUpdateColormaps (Client * c)
{
    XWindowAttributes attr;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateColormaps");

    if (c->ncmap)
    {
        XFree (c->cmap_windows);
    }
    if (!XGetWMColormapWindows (dpy, c->window, &c->cmap_windows, &c->ncmap))
    {
        c->ncmap = 0;
    }
    c->cmap = attr.colormap;
}

void
clientUpdateAllFrames (int mask)
{
    Client *c;
    int i;
    XWindowChanges wc;

    TRACE ("entering clientRedrawAllFrames");
    for (c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        if (mask & UPDATE_KEYGRABS)
        {
            clientUngrabKeys (c);
            clientGrabKeys (c);
        }
        if (mask & UPDATE_CACHE)
        {
            clientClearPixmapCache (c);
        }
        if (mask & UPDATE_GRAVITY)
        {
            clientGravitate (c, REMOVE);
            clientGravitate (c, APPLY);
            wc.x = c->x;
            wc.y = c->y;
            wc.width = c->width;
            wc.height = c->height;
            clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_FORCE_REDRAW);
        }
        if (mask & UPDATE_FRAME)
        {
            frameDraw (c, FALSE, FALSE);
        }
    }
}

void
clientGrabKeys (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientGrabKeys");
    TRACE ("grabbing keys for client \"%s\" (0x%lx)", c->name, c->window);

    grabKey (dpy, &params.keys[KEY_MOVE_UP], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_DOWN], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_LEFT], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_RIGHT], c->window);
    grabKey (dpy, &params.keys[KEY_RESIZE_UP], c->window);
    grabKey (dpy, &params.keys[KEY_RESIZE_DOWN], c->window);
    grabKey (dpy, &params.keys[KEY_RESIZE_LEFT], c->window);
    grabKey (dpy, &params.keys[KEY_RESIZE_RIGHT], c->window);
    grabKey (dpy, &params.keys[KEY_CLOSE_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_HIDE_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_MAXIMIZE_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_MAXIMIZE_VERT], c->window);
    grabKey (dpy, &params.keys[KEY_MAXIMIZE_HORIZ], c->window);
    grabKey (dpy, &params.keys[KEY_SHADE_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_STICK_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_RAISE_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_LOWER_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_CYCLE_WINDOWS], c->window);
    grabKey (dpy, &params.keys[KEY_NEXT_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_PREV_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_ADD_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_DEL_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_1], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_2], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_3], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_4], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_5], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_6], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_7], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_8], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_9], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_NEXT_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_PREV_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_1], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_2], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_3], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_4], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_5], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_6], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_7], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_8], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_9], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_1], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_2], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_3], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_4], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_5], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_6], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_7], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_8], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_9], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_10], c->window);
}

void
clientUngrabKeys (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientUngrabKeys");
    TRACE ("ungrabing keys for client \"%s\" (0x%lx)", c->name, c->window);

    ungrabKeys (dpy, c->window);
}

void
clientGrabButtons (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientGrabButtons");
    TRACE ("grabbing buttons for client \"%s\" (0x%lx)", c->name, c->window);
    
    grabButton(dpy, Button1, AltMask, c->window);
    grabButton(dpy, Button2, AltMask, c->window);
    grabButton(dpy, Button3, AltMask, c->window);
}

void
clientUngrabButtons (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientUngrabButtons");
    TRACE ("grabbing buttons for client \"%s\" (0x%lx)", c->name, c->window);
    
    XUngrabButton (dpy, AnyButton, AnyModifier, c->window);
}

static void
clientGrabButton1 (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientGrabButton1");
    TRACE ("grabbing buttons for client \"%s\" (0x%lx)", c->name, c->window);
    
    grabButton(dpy, Button1, 0, c->window);
}

static void
clientUngrabButton1 (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientUngrabButton1");
    TRACE ("ungrabing buttons for client \"%s\" (0x%lx)", c->name, c->window);

    ungrabButton(dpy, Button1, 0, c->window);
}

void
clientPassGrabButton1(Client * c)
{
    TRACE ("entering clientPassGrabButton1");
    TRACE ("ungrabing buttons for client \"%s\" (0x%lx)", c->name, c->window);

    if (c == NULL)
    {
        if (last_ungrab)
        {
            clientGrabButton1 (last_ungrab);
        }
        last_ungrab = NULL;
        return;
    }
    
    if (last_ungrab == c)
    {
        return;
    }
    
    if (last_ungrab)
    {
        clientGrabButton1 (last_ungrab);
    }
    
    clientUngrabButton1 (c);
    last_ungrab = c;
}

void
clientCoordGravitate (Client * c, int mode, int *x, int *y)
{
    int dx = 0, dy = 0;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientCoordGravitate");

    c->gravity =
        c->size->flags & PWinGravity ? c->size->win_gravity : NorthWestGravity;
    switch (c->gravity)
    {
        case CenterGravity:
            dx = (c->border_width * 2) - ((frameLeft (c) +
                    frameRight (c)) / 2);
            dy = (c->border_width * 2) - ((frameTop (c) +
                    frameBottom (c)) / 2);
            break;
        case NorthGravity:
            dx = (c->border_width * 2) - ((frameLeft (c) +
                    frameRight (c)) / 2);
            dy = frameTop (c);
            break;
        case SouthGravity:
            dx = (c->border_width * 2) - ((frameLeft (c) +
                    frameRight (c)) / 2);
            dy = (c->border_width * 2) - frameBottom (c);
            break;
        case EastGravity:
            dx = (c->border_width * 2) - frameRight (c);
            dy = (c->border_width * 2) - ((frameTop (c) +
                    frameBottom (c)) / 2);
            break;
        case WestGravity:
            dx = frameLeft (c);
            dy = (c->border_width * 2) - ((frameTop (c) +
                    frameBottom (c)) / 2);
            break;
        case NorthWestGravity:
            dx = frameLeft (c);
            dy = frameTop (c);
            break;
        case NorthEastGravity:
            dx = (c->border_width * 2) - frameRight (c);
            dy = frameTop (c);
            break;
        case SouthWestGravity:
            dx = frameLeft (c);
            dy = (c->border_width * 2) - frameBottom (c);
            break;
        case SouthEastGravity:
            dx = (c->border_width * 2) - frameRight (c);
            dy = (c->border_width * 2) - frameBottom (c);
            break;
        default:
            dx = 0;
            dy = 0;
            break;
    }
    *x = *x + (dx * mode);
    *y = *y + (dy * mode);
}

void
clientGravitate (Client * c, int mode)
{
    int x, y;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGravitate");

    x = c->x;
    y = c->y;
    clientCoordGravitate (c, mode, &x, &y);
    c->x = x;
    c->y = y;
}

static void
clientAddToList (Client * c)
{
    Client *client_sibling = NULL;
    GList *sibling = NULL;
    
    g_return_if_fail (c != NULL);
    TRACE ("entering clientAddToList");

    client_count++;
    if (clients)
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

    TRACE ("adding window \"%s\" (0x%lx) to windows list", c->name,
        c->window);
    windows = g_list_append (windows, c);

    client_sibling = clientGetLowestTransient (c);
    if (client_sibling)
    {
        /* The client has already a transient mapped */
        sibling =
            g_list_find (windows_stack, (gconstpointer) client_sibling);
        windows_stack = g_list_insert_before (windows_stack, sibling, c);
    }
    else
    {
        client_sibling = clientGetNextTopMost (c->win_layer, c);
        if (client_sibling)
        {
            sibling =
                g_list_find (windows_stack, (gconstpointer) client_sibling);
            windows_stack = g_list_insert_before (windows_stack, sibling, c);
        }
        else
        {
            windows_stack = g_list_append (windows_stack, c);
        }
    }

    clientSetNetClientList (net_client_list, windows);
    clientSetNetClientList (win_client_list, windows);
    clientSetNetClientList (net_client_list_stacking, windows_stack);
  
    FLAG_SET (c->flags, CLIENT_FLAG_MANAGED);
}

static void
clientRemoveFromList (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientRemoveFromList");
    g_assert (client_count > 0);

    FLAG_UNSET (c->flags, CLIENT_FLAG_MANAGED);
    client_count--;
    if (client_count == 0)
    {
        clients = NULL;
    }
    else
    {
        c->next->prev = c->prev;
        c->prev->next = c->next;
        if (c == clients)
        {
            clients = clients->next;
        }
    }

    TRACE ("removing window \"%s\" (0x%lx) from windows list", c->name,
        c->window);
    windows = g_list_remove (windows, c);

    TRACE ("removing window \"%s\" (0x%lx) from windows_stack list", c->name,
        c->window);
    windows_stack = g_list_remove (windows_stack, c);

    clientSetNetClientList (net_client_list, windows);
    clientSetNetClientList (win_client_list, windows);
    clientSetNetClientList (net_client_list_stacking, windows_stack);

    FLAG_UNSET (c->flags, CLIENT_FLAG_MANAGED);
}

static void
clientSetWidth (Client * c, int w1)
{
    int w2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetWidth");
    TRACE ("setting width %i for client \"%s\" (0x%lx)", w1, c->name,
        c->window);

    if ((c->size->flags & PResizeInc) && (c->size->width_inc))
    {
        w2 = (w1 - c->size->min_width) / c->size->width_inc;
        w1 = c->size->min_width + (w2 * c->size->width_inc);
    }
    if (c->size->flags & PMaxSize)
    {
        if (w1 > c->size->max_width)
        {
            w1 = c->size->max_width;
        }
    }
    if (c->size->flags & PMinSize)
    {
        if (w1 < c->size->min_width)
        {
            w1 = c->size->min_width;
        }
    }
    if (w1 < 1)
    {
        w1 = 1;
    }
    c->width = w1;
}

static void
clientSetHeight (Client * c, int h1)
{
    int h2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetHeight");
    TRACE ("setting height %i for client \"%s\" (0x%lx)", h1, c->name,
        c->window);

    if ((c->size->flags & PResizeInc) && (c->size->height_inc))
    {
        h2 = (h1 - c->size->min_height) / c->size->height_inc;
        h1 = c->size->min_height + (h2 * c->size->height_inc);
    }
    if (c->size->flags & PMaxSize)
    {
        if (h1 > c->size->max_height)
        {
            h1 = c->size->max_height;
        }
    }
    if (c->size->flags & PMinSize)
    {
        if (h1 < c->size->min_height)
        {
            h1 = c->size->min_height;
        }
    }
    if (h1 < 1)
    {
        h1 = 1;
    }
    c->height = h1;
}

static inline void
clientApplyStackList (GList * list)
{
    Window *xwinstack;
    guint nwindows;
    gint i = 0;

    g_return_if_fail (list != NULL);

    DBG ("applying stack list");
    nwindows = g_list_length (list);

    xwinstack = g_new (Window, nwindows + 2);
    xwinstack[i++] = sidewalk[0];
    xwinstack[i++] = sidewalk[1];

    if (nwindows)
    {
        GList *index;
        Client *c;
        
        for (index = g_list_last(list); index; index = g_list_previous (index))
        {
            c = (Client *) index->data;
            xwinstack[i++] = c->frame;
            DBG ("  [%i] \"%s\" (0x%lx)", i, c->name, c->window);
        }
    }

    XRestackWindows (dpy, xwinstack, (int) nwindows + 2);
    
    g_free (xwinstack);
}

static inline gboolean
clientTransientOrModalHasAncestor (Client * c, int ws)
{
    Client *c2;
    GList *index;

    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientTransientOrModalHasAncestor");

    if (!clientIsTransientOrModal (c))
    {
        return FALSE;
    }

    for (index = windows_stack; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if ((c2 != c) && !clientIsTransientOrModal (c2)
            && clientIsTransientOrModalFor (c, c2)
            && !FLAG_TEST (c2->flags, CLIENT_FLAG_HIDDEN)
            && (c2->win_workspace == ws))
        {
            return TRUE;
        }
    }
    return FALSE;

}

static inline Client *
clientGetLowestTransient (Client * c)
{
    Client *lowest_transient = NULL, *c2;
    GList *index;

    g_return_val_if_fail (c != NULL, NULL);

    TRACE ("entering clientGetLowestTransient");

    for (index = windows_stack; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if ((c2 != c) && clientIsTransientFor (c2, c))
        {
            lowest_transient = c2;
            break;
        }
    }
    return lowest_transient;
}

static inline Client *
clientGetHighestTransientOrModal (Client * c)
{
    Client *highest_transient = NULL;
    Client *c2, *c3;
    GList *transients = NULL;
    GList *index1, *index2;

    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering clientGetHighestTransientOrModal");

    for (index1 = windows_stack; index1; index1 = g_list_next (index1))
    {
        c2 = (Client *) index1->data;
        if (c2)
        {
            if ((c2 != c) && clientIsTransientOrModalFor (c2, c))
            {
                transients = g_list_append (transients, c2);
                highest_transient = c2;
            }
            else
            {
                for (index2 = transients; index2;
                    index2 = g_list_next (index2))
                {
                    c3 = (Client *) index2->data;
                    if ((c3 != c2) && clientIsTransientOrModalFor (c2, c3))
                    {
                        transients = g_list_append (transients, c2);
                        highest_transient = c2;
                        break;
                    }
                }
            }
        }
    }
    if (transients)
    {
        g_list_free (transients);
    }

    return highest_transient;
}

static inline Client *
clientGetHighestTransientOrModalFor (Client * c)
{
    Client *highest_transient = NULL;
    Client *c2;
    GList *index;

    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering clientGetHighestTransientOrModalFor");

    for (index = windows_stack; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if (c2)
        {
            if (clientIsTransientOrModalFor (c, c2))
            {
                highest_transient = c2;
            }
        }
    }

    return highest_transient;
}

static inline Client *
clientGetTopMostForGroup (Client * c)
{
    Client *top_most = NULL;
    Client *c2;
    GList *index;

    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering clientGetTopMostForGroup");

    for (index = windows_stack; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if (c2 != c)
        {
            if (clientSameGroup (c, c2))
            {
                top_most = c2;
            }
        }
    }

    return top_most;
}

static inline gboolean
clientVisibleForGroup (Client * c, int workspace)
{
    gboolean has_visible = FALSE;
    Client *c2;
    GList *index;

    g_return_val_if_fail (c != NULL, FALSE);
    TRACE ("entering clientVisibleForGroup");

    for (index = windows_stack; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if (c2)
        {
            if (clientSameGroup (c, c2) && (c2->win_workspace == workspace))
            {
                has_visible = TRUE;
                break;
            }
        }
    }

    return has_visible;
}

static inline Client *
clientGetNextTopMost (int layer, Client * exclude)
{
    Client *top = NULL, *c;
    GList *index;

    TRACE ("entering clientGetNextTopMost");

    for (index = windows_stack; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        TRACE ("*** stack window \"%s\" (0x%lx), layer %i", c->name,
            c->window, (int) c->win_layer);
        if (!exclude || (c != exclude))
        {
            if (c->win_layer > layer)
            {
                top = c;
                break;
            }
        }
    }

    return top;
}

static inline Client *
clientGetTopMostFocusable (int layer, Client * exclude)
{
    Client *top = NULL, *c;
    GList *index;

    TRACE ("entering clientGetTopMost");

    for (index = windows_stack; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        TRACE ("*** stack window \"%s\" (0x%lx), layer %i", c->name,
            c->window, (int) c->win_layer);
        if (!exclude || (c != exclude))
        {
            if ((c->win_layer <= layer) && clientAcceptFocus (c)
                && FLAG_TEST (c->flags, CLIENT_FLAG_VISIBLE))
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

static inline Client *
clientGetBottomMost (int layer, Client * exclude)
{
    Client *bot = NULL, *c;
    GList *index;

    TRACE ("entering clientGetBottomMost");

    for (index = windows_stack; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        if (c)
        {
            TRACE ("*** stack window \"%s\" (0x%lx), layer %i", c->name,
                c->window, (int) c->win_layer);
            if (!exclude || (c != exclude))
            {
                if (c->win_layer < layer)
                {
                    bot = c;
                }
                else if (c->win_layer >= layer)
                {
                    break;
                }
            }
        }
    }
    return bot;
}

static inline Client *
clientGetModalFor (Client * c)
{
    Client *latest_modal = NULL;
    Client *c2, *c3;
    GList *modals = NULL;
    GList *index1, *index2;

    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering clientGetModalFor");

    for (index1 = windows_stack; index1; index1 = g_list_next (index1))
    {
        c2 = (Client *) index1->data;
        if (c2)
        {
            if ((c2 != c) && clientIsModalFor (c2, c))
            {
                modals = g_list_append (modals, c2);
                latest_modal = c2;
            }
            else
            {
                for (index2 = modals; index2;
                    index2 = g_list_next (index2))
                {
                    c3 = (Client *) index2->data;
                    if ((c3 != c2) && clientIsModalFor (c2, c3))
                    {
                        modals = g_list_append (modals, c2);
                        latest_modal = c2;
                        break;
                    }
                }
            }
        }
    }
    if (modals)
    {
        g_list_free (modals);
    }

    return latest_modal;
}

/* clientConstrainRatio - adjust the given width and height to account for
   the constraints imposed by size hints

   The aspect ratio stuff, is borrowed from uwm's CheckConsistency routine.
 */

#define MAKE_MULT(a,b) ((b==1) ? (a) : (((int)((a)/(b))) * (b)) )
static inline void
clientConstrainRatio (Client * c, int w1, int h1, int corner)
{

    g_return_if_fail (c != NULL);
    TRACE ("entering clientConstrainRatio");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    
    if (c->size->flags & PAspect)
    {
        int xinc, yinc, minx, miny, maxx, maxy, delta;

        xinc = c->size->width_inc;
        yinc = c->size->height_inc;
        minx = c->size->min_aspect.x;
        miny = c->size->min_aspect.y;
        maxx = c->size->max_aspect.x;
        maxy = c->size->max_aspect.y;

        if ((minx * h1 > miny * w1) && 
            (miny) && (corner == 4 + SIDE_BOTTOM))
        {
            /* Change width to match */
            delta = MAKE_MULT (minx * h1 /  miny - w1, xinc);
            if (!(c->size->flags & PMaxSize) || 
                (w1 + delta <= c->size->max_width))
            {
                w1 += delta;
            }
        }
        if ((minx * h1 > miny * w1) &&
            (minx))
        {
            delta = MAKE_MULT (h1 - w1 * miny / minx, yinc);
            if (!(c->size->flags & PMinSize) ||
                (h1 - delta >= c->size->min_height))
            {
                h1 -= delta;
            }
            else
            {
                delta = MAKE_MULT (minx * h1 / miny - w1, xinc);
                if (!(c->size->flags & PMaxSize) ||
                    (w1 + delta <= c->size->max_width))
                  w1 += delta;
            }
        }

        if ((maxx * h1 < maxy * w1) &&
            (maxx) &&
            ((corner == 4 + SIDE_LEFT) || (corner == 4 + SIDE_RIGHT)))
        {
            delta = MAKE_MULT (w1 * maxy / maxx - h1, yinc);
            if (!(c->size->flags & PMaxSize) ||
                (h1 + delta <= c->size->max_height))
            {
                h1 += delta;
            }
        }
        if ((maxx * h1 < maxy * w1) &&
             (maxy))
        {
            delta = MAKE_MULT (w1 - maxx * h1 / maxy, xinc);
            if (!(c->size->flags & PMinSize) ||
                (w1 - delta >= c->size->min_width))
            {
                w1 -= delta;
            }
            else
            {
                delta = MAKE_MULT (w1 * maxy / maxx - h1, yinc);
                if (!(c->size->flags & PMaxSize) ||
                    (h1 + delta <= c->size->max_height))
                {
                    h1 += delta;
                }
            }
        }
    }

    c->height = h1;
    c->width = w1;
}

/* clientConstrainPos() is used when moving windows
   to ensure that the window stays accessible to the user
 */
static inline void
clientConstrainPos (Client * c, gboolean show_full)
{
    int client_margins[4];
    int cx, cy, left, right, top, bottom;
    int disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_x, frame_y, frame_height, frame_width, frame_top, frame_left;
    gboolean leftMostHead, rightMostHead, topMostHead, bottomMostHead;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientConstrainPos %s",
        show_full ? "(with show full)" : "(w/out show full)");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE ("ignoring constrained for client \"%s\" (0x%lx)", c->name,
            c->window);
        return;
    }
    /* We use a bunch of local vars to reduce the overhead of calling other functions all the time */
    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_height = frameHeight (c);
    frame_width = frameWidth (c);
    frame_top = frameTop (c);
    frame_left = frameLeft (c);

    cx = frame_x + (frame_width / 2);
    cy = frame_y + (frame_height / 2);

    leftMostHead = isLeftMostHead (dpy, screen, cx, cy);
    rightMostHead = isRightMostHead (dpy, screen, cx, cy);
    topMostHead = isTopMostHead (dpy, screen, cx, cy);
    bottomMostHead = isBottomMostHead (dpy, screen, cx, cy);

    client_margins[MARGIN_TOP] = margins[MARGIN_TOP];
    client_margins[MARGIN_LEFT] = margins[MARGIN_LEFT];
    client_margins[MARGIN_RIGHT] = margins[MARGIN_RIGHT];
    client_margins[MARGIN_BOTTOM] = margins[MARGIN_BOTTOM];

    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUTS))
    {
        workspaceGetArea (client_margins, NULL, c);
    }

    left = (leftMostHead ? (int) client_margins[MARGIN_LEFT] : 0);
    right = (rightMostHead ? (int) client_margins[MARGIN_RIGHT] : 0);
    top = (topMostHead ? (int) client_margins[MARGIN_TOP] : 0);
    bottom = (bottomMostHead ? (int) client_margins[MARGIN_BOTTOM] : 0);

    disp_x = MyDisplayX (cx, cy);
    disp_y = MyDisplayY (cx, cy);
    disp_max_x = MyDisplayMaxX (dpy, screen, cx, cy);
    disp_max_y = MyDisplayMaxY (dpy, screen, cx, cy);

    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_height = frameHeight (c);
    frame_width = frameWidth (c);
    frame_top = frameTop (c);
    frame_left = frameLeft (c);

    if (show_full)
    {
        if (rightMostHead && (frame_x + frame_width > disp_max_x - right))
        {
            c->x = disp_max_x - right - frame_width + frame_left;
            frame_x = frameX (c);
        }
        if (leftMostHead && (frame_x < disp_x + left))
        {
            c->x = disp_x + left + frame_left;
        }
        if (bottomMostHead && (frame_y + frame_height > disp_max_y - bottom))
        {
            c->y = disp_max_y - bottom - frame_height + frame_top;
            frame_y = frameY (c);
        }
        if (topMostHead && (frame_y < disp_y + top))
        {
            c->y = disp_y + top + frame_top;
        }
    }
    else
    {
        if (rightMostHead
            && (frame_x + CLIENT_MIN_VISIBLE > disp_max_x - right))
        {
            c->x = disp_max_x - right - CLIENT_MIN_VISIBLE + frame_left;
            frame_x = frameX (c);
        }
        if (leftMostHead
            && (frame_x + frame_width < disp_x + CLIENT_MIN_VISIBLE + left))
        {
            c->x =
                disp_x + CLIENT_MIN_VISIBLE + left - frame_width + frame_left;
        }
        if (bottomMostHead
            && (frame_y + CLIENT_MIN_VISIBLE > disp_max_y - bottom))
        {
            c->y = disp_max_y - bottom - CLIENT_MIN_VISIBLE + frame_top;
            frame_y = frameY (c);
        }
        if (topMostHead
            && (frame_y + frame_height < disp_y + CLIENT_MIN_VISIBLE + top))
        {
            c->y =
                disp_y + CLIENT_MIN_VISIBLE + top - frame_height + frame_top;
        }
    }
}

/* clientKeepVisible is used at initial mapping, to make sure
   the window is visible on screen. It also does coordonate
   translation in Xinerama to center window on physical screen
   Not to be confused with clientConstrainPos()
 */
static inline void
clientKeepVisible (Client * c)
{
    int client_margins[4];
    int cx, cy, left, right, top, bottom;
    int diff_x, diff_y;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientKeepVisible");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    cx = frameX (c) + (frameWidth (c) / 2);
    cy = frameY (c) + (frameHeight (c) / 2);

    client_margins[MARGIN_TOP] = margins[MARGIN_TOP];
    client_margins[MARGIN_LEFT] = margins[MARGIN_LEFT];
    client_margins[MARGIN_RIGHT] = margins[MARGIN_RIGHT];
    client_margins[MARGIN_BOTTOM] = margins[MARGIN_BOTTOM];

    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUTS))
    {
        workspaceGetArea (client_margins, NULL, c);
    }

    left =
        (isLeftMostHead (dpy, screen, cx,
            cy) ? (int) client_margins[MARGIN_LEFT] : 0);
    right =
        (isRightMostHead (dpy, screen, cx,
            cy) ? (int) client_margins[MARGIN_RIGHT] : 0);
    top =
        (isTopMostHead (dpy, screen, cx,
            cy) ? (int) client_margins[MARGIN_TOP] : 0);
    bottom =
        (isBottomMostHead (dpy, screen, cx,
            cy) ? (int) client_margins[MARGIN_BOTTOM] : 0);

    /* Translate coodinates to center on physical screen */

    diff_x = abs (c->size->x - ((MyDisplayFullWidth (dpy, screen) - c->width) / 2));
    diff_y = abs (c->size->y - ((MyDisplayFullHeight (dpy, screen) - c->height) / 2));

    if ((use_xinerama) && (diff_x < 25) && (diff_y < 25))
    {
        /* We consider that the windows is centered on screen,
         * Thus, will move it so its center on the current
         * physical screen
         */
        getMouseXY (root, &cx, &cy);
        
        c->x =
            MyDisplayX (cx, cy) + (MyDisplayWidth (dpy, screen, cx,
                cy) - c->width) / 2;
        c->y =
            MyDisplayY (cx, cy) + (MyDisplayHeight (dpy, screen, cx,
                cy) - c->height) / 2;
    }
    clientConstrainPos (c, TRUE);
}

/* Compute rectangle overlap area */
static inline unsigned long
overlap (int x0, int y0, int x1, int y1, int tx0, int ty0, int tx1, int ty1)
{
    /* Compute overlapping box */
    if (tx0 > x0)
    {
        x0 = tx0;
    }
    if (ty0 > y0)
    {
        y0 = ty0;
    }
    if (tx1 < x1)
    {
        x1 = tx1;
    }
    if (ty1 < y1)
    {
        y1 = ty1;
    }
    if (x1 <= x0 || y1 <= y0)
    {
        return 0;
    }
    return (x1 - x0) * (y1 - y0);
}

static void
clientInitPosition (Client * c)
{
    int client_margins[4];
    int test_x = 0, test_y = 0;
    Client *c2;
    int xmax, ymax, best_x, best_y, i, msx, msy;
    int left, right, top, bottom;
    int frame_x, frame_y, frame_height, frame_width, frame_left, frame_top;
    unsigned long best_overlaps = 0;
    gboolean first = TRUE;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientInitPosition");

    clientGravitate (c, APPLY);

    if ((c->size->flags & (PPosition | USPosition)) ||
        (c->type & (WINDOW_TYPE_DONT_PLACE)) ||
        ((c->type & (WINDOW_TYPE_DIALOG)) && !clientIsTransient (c)))
    {
        if (CONSTRAINED_WINDOW (c))
        {
            clientKeepVisible (c);
        }
        return;
    }

    if (clientIsTransient (c) && (c2 = clientGetTransient (c)))
    {
        /* Center transient relative to their parent window */
        c->x = c2->x + (c2->width - c->width) / 2;
        c->y = c2->y + (c2->height - c->height) / 2;
        if (CONSTRAINED_WINDOW (c))
        {
            clientKeepVisible (c);
        }
        return;
    }

    client_margins[MARGIN_TOP] = margins[MARGIN_TOP];
    client_margins[MARGIN_LEFT] = margins[MARGIN_LEFT];
    client_margins[MARGIN_RIGHT] = margins[MARGIN_RIGHT];
    client_margins[MARGIN_BOTTOM] = margins[MARGIN_BOTTOM];

    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUTS))
    {
        workspaceGetArea (client_margins, NULL, c);
    }

    getMouseXY (root, &msx, &msy);
    left =
        (isLeftMostHead (dpy, screen, msx,
            msy) ? MAX ((int) client_margins[MARGIN_LEFT],
            params.xfwm_margins[MARGIN_LEFT]) : 0);
    right =
        (isRightMostHead (dpy, screen, msx,
            msy) ? MAX ((int) client_margins[MARGIN_RIGHT],
            params.xfwm_margins[MARGIN_RIGHT]) : 0);
    top =
        (isTopMostHead (dpy, screen, msx,
            msy) ? MAX ((int) client_margins[MARGIN_TOP],
            params.xfwm_margins[MARGIN_TOP]) : 0);
    bottom =
        (isBottomMostHead (dpy, screen, msx,
            msy) ? MAX ((int) client_margins[MARGIN_BOTTOM],
            params.xfwm_margins[MARGIN_BOTTOM]) : 0);

    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_height = frameHeight (c);
    frame_width = frameWidth (c);
    frame_left = frameLeft(c);
    frame_top = frameTop (c);

    xmax = MyDisplayMaxX (dpy, screen, msx, msy) - c->width - frameRight (c) - right;
    ymax = MyDisplayMaxY (dpy, screen, msx, msy) - c->height - frameBottom (c) - bottom;
    best_x = MyDisplayX (msx, msy) + frameLeft (c) + left;
    best_y = MyDisplayY (msx, msy) + frameTop (c) + top;

    for (test_y = MyDisplayY (msx, msy) + frameTop (c) + top; test_y <= ymax;
        test_y += 8)
    {
        for (test_x = MyDisplayX (msx, msy) + frameLeft (c) + left;
            test_x <= xmax; test_x += 8)
        {
            unsigned long count_overlaps = 0;
            TRACE ("analyzing %i clients", client_count);
            for (c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
            {
                if ((c2 != c) && (c2->type != WINDOW_DESKTOP)
                    && (c->win_workspace == c2->win_workspace)
                    && FLAG_TEST (c2->flags, CLIENT_FLAG_VISIBLE))
                {
                    count_overlaps +=
                        overlap (test_x - frame_left, 
                                 test_y - frame_top, 
                                 test_x - frame_left + frame_width,
                                 test_y - frame_top + frame_height, 
                                 frameX (c2), 
                                 frameY (c2),
                                 frameX (c2) + frameWidth (c2),
                                 frameY (c2) + frameHeight (c2));
                }
            }
            TRACE ("overlaps so far is %u", (unsigned int) count_overlaps);
            if (count_overlaps == 0)
            {
                TRACE ("overlaps is 0 so it's the best we can get");
                c->x = test_x;
                c->y = test_y;
                return;
            }
            else if ((count_overlaps < best_overlaps) || (first))
            {
                TRACE ("overlaps %u is better than the best we have %u",
                    (unsigned int) count_overlaps,
                    (unsigned int) best_overlaps);
                best_x = test_x;
                best_y = test_y;
                best_overlaps = count_overlaps;
            }
            if (first)
            {
                first = FALSE;
            }
        }
    }
    c->x = best_x;
    c->y = best_y;
    return;
}

void
clientConfigure (Client * c, XWindowChanges * wc, int mask, unsigned short flags)
{
    XConfigureEvent ce;
    gboolean moved = FALSE;
    gboolean resized = FALSE;
    
    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    TRACE ("entering clientConfigure");
    TRACE ("configuring client \"%s\" (0x%lx) %s, type %u", c->name,
        c->window, flags & CFG_CONSTRAINED ? "constrained" : "not contrained", c->type);

    if (mask & CWX)
    {
        moved = TRUE;
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_MOVING_RESIZING))
        {
            c->x = wc->x;
        }
    }
    if (mask & CWY)
    {
        moved = TRUE;
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_MOVING_RESIZING))
        {
            c->y = wc->y;
        }
    }
    if (mask & CWWidth)
    {
        resized = TRUE;
        clientSetWidth (c, wc->width);
    }
    if (mask & CWHeight)
    {
        resized = TRUE;
        clientSetHeight (c, wc->height);
    }
    if (mask & CWBorderWidth)
    {
        c->border_width = wc->border_width;
    }
    if (mask & CWStackMode)
    {
        switch (wc->stack_mode)
        {
                /* Limitation: we don't support sibling... */
            case Above:
            case TopIf:
                TRACE ("Above");
                clientRaise (c);
                break;
            case Below:
            case BottomIf:
                TRACE ("Below");
                clientLower (c);
                break;
            case Opposite:
            default:
                break;
        }
        mask &= ~(CWStackMode | CWSibling);
    }

    if ((flags & CFG_CONSTRAINED) && (mask & (CWX | CWY)) && 
         CONSTRAINED_WINDOW (c))
    {
        clientConstrainPos (c, TRUE);
    }
    wc->x = frameX (c);
    wc->y = frameY (c);
    wc->width = frameWidth (c);
    wc->height = frameHeight (c);
    wc->border_width = 0;
    XConfigureWindow (dpy, c->frame, mask, wc);
    wc->x = frameLeft (c);
    wc->y = frameTop (c);
    wc->width = c->width;
    wc->height = c->height;
    XConfigureWindow (dpy, c->window, mask, wc);

    if (resized || (flags & CFG_FORCE_REDRAW))
    {
        frameDraw (c, FALSE, TRUE);
    }
    
    if ((flags & CFG_NOTIFY) ||
        ((flags & CFG_REQUEST) && !(moved || resized)) ||
        (moved && !resized))
    {
        DBG ("Sending ConfigureNotify");
        ce.type = ConfigureNotify;
        ce.display = dpy;
        ce.event = c->window;
        ce.window = c->window;
        ce.x = c->x;
        ce.y = c->y;
        ce.width = c->width;
        ce.height = c->height;
        ce.border_width = 0;
        ce.above = c->frame;
        ce.override_redirect = FALSE;
        XSendEvent (dpy, c->window, FALSE, StructureNotifyMask,
            (XEvent *) & ce);
    }
}

void
clientGetMWMHints (Client * c, gboolean update)
{
    PropMwmHints *mwm_hints;
    XWindowChanges wc;
    
    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);
    
    TRACE ("entering clientGetMWMHints client \"%s\" (0x%lx)", c->name,
        c->window);
    
    mwm_hints = getMotifHints (dpy, c->window);
    if (mwm_hints)
    {
        if (mwm_hints->flags & MWM_HINTS_DECORATIONS)
        {
            if (mwm_hints->decorations & MWM_DECOR_ALL)
            {
                FLAG_SET (c->flags,
                    CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_MENU);
            }
            else
            {
                FLAG_UNSET (c->flags,
                    CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_MENU);
                FLAG_SET (c->flags,
                    (mwm_hints->
                        decorations & (MWM_DECOR_TITLE | MWM_DECOR_BORDER)) ?
                    CLIENT_FLAG_HAS_BORDER : 0);
                FLAG_SET (c->flags,
                    (mwm_hints->
                        decorations & (MWM_DECOR_MENU)) ? CLIENT_FLAG_HAS_MENU
                    : 0);
                /*
                   FLAG_UNSET(c->flags, CLIENT_FLAG_HAS_HIDE);
                   FLAG_UNSET(c->flags, CLIENT_FLAG_HAS_MAXIMIZE);
                   FLAG_SET(c->flags, (mwm_hints->decorations & (MWM_DECOR_MINIMIZE)) ? CLIENT_FLAG_HAS_HIDE : 0);
                   FLAG_SET(c->flags, (mwm_hints->decorations & (MWM_DECOR_MAXIMIZE)) ? CLIENT_FLAG_HAS_MAXIMIZE : 0);
                 */
            }
        }
        /* The following is from Metacity : */
        if (mwm_hints->flags & MWM_HINTS_FUNCTIONS)
        {
            if (!(mwm_hints->functions & MWM_FUNC_ALL))
            {
                FLAG_UNSET (c->flags,
                    CLIENT_FLAG_HAS_CLOSE | CLIENT_FLAG_HAS_HIDE |
                    CLIENT_FLAG_HAS_MAXIMIZE | CLIENT_FLAG_HAS_MOVE |
                    CLIENT_FLAG_HAS_RESIZE);
            }
            else
            {
                FLAG_SET (c->flags,
                    CLIENT_FLAG_HAS_CLOSE | CLIENT_FLAG_HAS_HIDE |
                    CLIENT_FLAG_HAS_MAXIMIZE | CLIENT_FLAG_HAS_MOVE |
                    CLIENT_FLAG_HAS_RESIZE);
            }

            if (mwm_hints->functions & MWM_FUNC_CLOSE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_HAS_CLOSE);
            }
            if (mwm_hints->functions & MWM_FUNC_MINIMIZE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_HAS_HIDE);
            }
            if (mwm_hints->functions & MWM_FUNC_MAXIMIZE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_HAS_MAXIMIZE);
            }
            if (mwm_hints->functions & MWM_FUNC_RESIZE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_HAS_RESIZE);
            }
            if (mwm_hints->functions & MWM_FUNC_MOVE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_HAS_MOVE);
            }
        }
        g_free (mwm_hints);
    }
    
    if (update)
    {
        /* EWMH window type takes precedences over Motif hints */ 
        clientWindowType (c);
        if (FLAG_TEST_AND_NOT(c->flags, CLIENT_FLAG_HAS_BORDER, CLIENT_FLAG_FULLSCREEN) && 
           (c->legacy_fullscreen))
        {
            /* legacy app changed its decoration, put it back on regular layer */
            c->legacy_fullscreen = FALSE;
            clientSetLayer (c, WIN_LAYER_NORMAL);
        }
        wc.x = c->x;
        wc.y = c->y;
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_FORCE_REDRAW);
    }
}

void
clientGetWMNormalHints (Client * c, gboolean update)
{
    unsigned long previous_value;
    long dummy = 0;
    XWindowChanges wc;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);
    
    TRACE ("entering clientGetWMNormalHints client \"%s\" (0x%lx)", c->name,
        c->window);

    if (!c->size)
    {
        c->size = XAllocSizeHints ();
    }
    g_assert (c->size);
    if (!XGetWMNormalHints (dpy, c->window, c->size, &dummy))
    {
        c->size->flags = 0;
    }
    
    previous_value = FLAG_TEST (c->flags, CLIENT_FLAG_IS_RESIZABLE);
    FLAG_UNSET (c->flags, CLIENT_FLAG_IS_RESIZABLE);

    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;

    if (!(c->size->flags & PMaxSize))
    {
        c->size->max_width = G_MAXINT;
        c->size->max_height = G_MAXINT;
        c->size->flags |= PMaxSize;
    }
    
    if (!(c->size->flags & PMinSize))
    {
        if ((c->size->flags & PBaseSize))
        {
            c->size->min_width = c->size->base_width;
            c->size->min_height = c->size->base_height;
        }
        else
        {
            c->size->min_width = 1;
            c->size->min_height = 1;
        }
        c->size->flags |= PMinSize;
    }

    if (c->size->flags & PResizeInc)
    {
        if (c->size->width_inc < 1)
        {
            c->size->width_inc = 1;
        }
        if (c->size->height_inc < 1)
        {
            c->size->height_inc = 1;
        }
    }
    else
    {
        c->size->width_inc = 1;
        c->size->height_inc = 1;
    }

    if (c->size->flags & PAspect)
    {
        if (c->size->min_aspect.x < 1)
        {
            c->size->min_aspect.x = 1;
        }
        if (c->size->min_aspect.y < 1)
        {
            c->size->min_aspect.y = 1;
        }
        if (c->size->max_aspect.x < 1)
        {
            c->size->max_aspect.x = 1;
        }
        if (c->size->max_aspect.y < 1)
        {
            c->size->max_aspect.y = 1;
        }
    }
    else
    {
        c->size->min_aspect.x = 1;
        c->size->min_aspect.y = 1;
        c->size->max_aspect.x = G_MAXINT;
        c->size->max_aspect.y = G_MAXINT;
    }

    if (c->size->min_width < 1)
    {
        c->size->min_width = 1;
    }
    if (c->size->min_height < 1)
    {
        c->size->min_height = 1;
    }
    if (c->size->max_width < 1)
    {
        c->size->max_width = 1;
    }
    if (c->size->max_height < 1)
    {
        c->size->max_height = 1;
    }
    if (wc.width > c->size->max_width)
    {
        wc.width = c->size->max_width;
    }
    if (wc.height > c->size->max_height)
    {
        wc.height = c->size->max_height;
    }
    if (wc.width < c->size->min_width)
    {
        wc.width = c->size->min_width;
    }
    if (wc.height < c->size->min_height)
    {
        wc.height = c->size->min_height;
    }

    if ((c->size->min_width < c->size->max_width) || 
        (c->size->min_height < c->size->max_height))
    {
        FLAG_SET (c->flags, CLIENT_FLAG_IS_RESIZABLE);
    }
    
    if (update)
    {
        if ((c->width != wc.width) || (c->height != wc.height))
        {
            clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_CONSTRAINED);
        }
        else if (FLAG_TEST (c->flags, CLIENT_FLAG_IS_RESIZABLE) != previous_value)
        {
            frameDraw (c, TRUE, FALSE);
        }
    }
    else
    {
        c->width = wc.width;
        c->height = wc.height;
    }
}

void
clientGetWMProtocols (Client * c)
{
    unsigned int wm_protocols_flags = 0;
    
    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);
    
    TRACE ("entering clientGetWMProtocols client \"%s\" (0x%lx)", c->name,
        c->window);
    wm_protocols_flags = getWMProtocols (dpy, c->window);
    FLAG_SET (c->wm_flags,
        (wm_protocols_flags & WM_PROTOCOLS_DELETE_WINDOW) ?
        WM_FLAG_DELETE : 0);
    FLAG_SET (c->wm_flags,
        (wm_protocols_flags & WM_PROTOCOLS_TAKE_FOCUS) ?
        WM_FLAG_TAKEFOCUS : 0);
    /* KDE extension */
    FLAG_SET (c->wm_flags, 
        (wm_protocols_flags & WM_PROTOCOLS_CONTEXT_HELP) ?
        WM_FLAG_CONTEXT_HELP : 0);
}

static inline void
clientFree (Client * c)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientFree");
    TRACE ("freeing client \"%s\" (0x%lx)", c->name, c->window);

    if (c->name)
    {
        free (c->name);
    }
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    if (c->startup_id)
    {
        free (c->startup_id);
    }
#endif
    if (c->size)
    {
        XFree (c->size);
    }
    if (c->wmhints)
    {
        XFree (c->wmhints);
    }
    if (c->ncmap > 0)
    {
        XFree (c->cmap_windows);
    }
    if (c->class.res_name)
    {
        XFree (c->class.res_name);
    }
    if (c->class.res_class)
    {
        XFree (c->class.res_class);
    }

    g_free (c);
}

static inline void
clientGetWinState (Client * c)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientGetWinState");

    if (c->win_state & WIN_STATE_STICKY)
    {
        if (!clientIsTransientOrModal (c))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
        }
    }
    if (c->win_state & WIN_STATE_SHADED)
    {
        FLAG_SET (c->flags, CLIENT_FLAG_SHADED);
    }
    if (c->win_state & WIN_STATE_MAXIMIZED_HORIZ)
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
        }
    }
    if (c->win_state & WIN_STATE_MAXIMIZED_VERT)
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
        }
    }
    if (c->win_state & WIN_STATE_MAXIMIZED)
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED);
        }
    }
}

static inline void
clientApplyInitialState (Client * c)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientApplyInitialState");

    /* We check that afterwards to make sure all states are now known */
    if (FLAG_TEST (c->flags,
            CLIENT_FLAG_MAXIMIZED_HORIZ | CLIENT_FLAG_MAXIMIZED_VERT))
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
        {
            unsigned long mode = 0;

            TRACE ("Applying client's initial state: maximized");
            if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ))
            {
                TRACE ("initial state: maximized horiz.");
                mode |= WIN_STATE_MAXIMIZED_HORIZ;
            }
            if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT))
            {
                TRACE ("initial state: maximized vert.");
                mode |= WIN_STATE_MAXIMIZED_VERT;
            }
            /* Unset fullscreen mode so that clientToggleMaximized() really change the state */
            FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED);
            clientToggleMaximized (c, mode);
        }
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        if (!clientIsTransientOrModal (c))
        {
            TRACE ("Applying client's initial state: fullscreen");
            clientUpdateFullscreenState (c);
        }
    }
    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_ABOVE, CLIENT_FLAG_BELOW))
    {
        if (!clientIsTransientOrModal (c))
        {
            TRACE ("Applying client's initial state: above");
            clientUpdateAboveState (c);
        }
    }
    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_BELOW, CLIENT_FLAG_ABOVE))
    {
        if (!clientIsTransientOrModal (c))
        {
            TRACE ("Applying client's initial state: below");
            clientUpdateBelowState (c);
        }
    }
    if (FLAG_TEST_ALL (c->flags, CLIENT_FLAG_STICKY | CLIENT_FLAG_HAS_STICK))
    {
        if (!clientIsTransientOrModal (c))
        {
            TRACE ("Applying client's initial state: sticky");
            clientStick (c, TRUE);
        }
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        TRACE ("Applying client's initial state: sticky");
        clientShade (c);
    }
}

void
clientFocusNew(Client * c)
{
    g_return_if_fail (c != NULL);

    if (!clientAcceptFocus (c))
    {
        return;
    }
    if (params.focus_new || FLAG_TEST(c->flags, CLIENT_FLAG_STATE_MODAL))
    {
        clientSetFocus (c, FOCUS_SORT | FOCUS_IGNORE_MODAL);
        clientPassGrabButton1 (c);
    }
    else
    {
        clientPassGrabButton1 (NULL);
    }
}

void
clientClearPixmapCache (Client * c)
{
    g_return_if_fail (c != NULL);

    myPixmapFree (dpy, &c->pm_cache.pm_title[ACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_title[INACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_LEFT][ACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_LEFT][INACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_RIGHT][ACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_RIGHT][INACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_BOTTOM][ACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_BOTTOM][INACTIVE]);
}

void
clientFrame (Window w, gboolean recapture)
{
    XWindowAttributes attr;
    XWindowChanges wc;
    XSetWindowAttributes attributes;
    Client *c;
    unsigned long valuemask;
    int i;

    g_return_if_fail (w != None);
    TRACE ("entering clientFrame");
    TRACE ("framing client (0x%lx)", w);

    if (w == gnome_win)
    {
        TRACE ("Not managing our own window");
        return;
    }

    gdk_error_trap_push ();
    if (checkKdeSystrayWindow (dpy, w) && (systray != None))
    {
        TRACE ("Not managing KDE systray windows");
        sendSystrayReqDock (dpy, w, systray);
        gdk_error_trap_pop ();
        return;
    }

    if (!XGetWindowAttributes (dpy, w, &attr))
    {
        TRACE ("Cannot get window attributes");
        gdk_error_trap_pop ();
        return;
    }

    if (attr.override_redirect)
    {
        TRACE ("Not managing override_redirect windows");
        gdk_error_trap_pop ();
        return;
    }

    c = g_new(Client, 1);
    if (!c)
    {
        TRACE ("Cannot allocate memory for the window structure");
        gdk_error_trap_pop ();
        return;
    }
    
    c->window = w;
    c->serial = client_serial++;

    getWindowName (dpy, c->window, &c->name);
    TRACE ("name \"%s\"", c->name);
    getTransientFor (dpy, screen, c->window, &c->transient_for);

    /* Initialize structure */
    c->size = NULL;
    c->flags = CLIENT_FLAG_INITIAL_VALUES;
    c->wm_flags = 0L;
    c->x = attr.x;
    c->y = attr.y;
    c->width = attr.width;
    c->height = attr.height;
    clientGetWMNormalHints (c, FALSE);

    c->old_x = c->x;
    c->old_y = c->y;
    c->old_width = c->width;
    c->old_height = c->height;
    c->size->x = c->x;
    c->size->y = c->y;
    c->size->width = c->width;
    c->size->height = c->height;
    c->fullscreen_old_x = c->x;
    c->fullscreen_old_y = c->y;
    c->fullscreen_old_width = c->width;
    c->fullscreen_old_height = c->height;
    c->border_width = attr.border_width;
    c->cmap = attr.colormap;

    if (((c->size->flags & (PMinSize | PMaxSize)) != (PMinSize | PMaxSize))
        || (((c->size->flags & (PMinSize | PMaxSize)) ==
                (PMinSize | PMaxSize))
            && ((c->size->min_width < c->size->max_width)
                || (c->size->min_height < c->size->max_height))))
    {
        FLAG_SET (c->flags, CLIENT_FLAG_IS_RESIZABLE);
    }

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        c->button_pressed[i] = FALSE;
    }

    if (!XGetWMColormapWindows (dpy, c->window, &c->cmap_windows, &c->ncmap))
    {
        c->ncmap = 0;
    }

    c->class.res_name = NULL;
    c->class.res_class = NULL;
    XGetClassHint (dpy, w, &c->class);
    c->wmhints = XGetWMHints (dpy, c->window);
    c->group_leader = None;
    if (c->wmhints)
    {
        if (c->wmhints->flags & WindowGroupHint)
        {
            c->group_leader = c->wmhints->window_group;
        }
    }
    c->client_leader = getClientLeader (dpy, c->window);
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    c->startup_id = NULL;
    getWindowStartupId (dpy, c->window, &c->startup_id);
#endif
    TRACE ("\"%s\" (0x%lx) initial map_state = %s",
                c->name, c->window,
                (attr.map_state == IsUnmapped) ?
                "IsUnmapped" :
                (attr.map_state == IsViewable) ?
                "IsViewable" :
                (attr.map_state == IsUnviewable) ?
                "IsUnviewable" :
                "(unknown)");
    if (attr.map_state != IsUnmapped)
    {
        /* XReparentWindow() will cause an UnmapNotify followed by 
         * a MapNotify. 
         * Unset the "mapped" flag to avoid a transition to 
         * withdrawn state.
         */ 
        XUnmapWindow (dpy, c->window);
        FLAG_SET (c->flags, CLIENT_FLAG_MAP_PENDING);
    }

    c->ignore_unmap = 0;
    c->type = UNSET;
    c->type_atom = None;

    FLAG_SET (c->flags, START_ICONIC (c) ? CLIENT_FLAG_HIDDEN : 0);
    FLAG_SET (c->wm_flags, ACCEPT_INPUT (c->wmhints) ? WM_FLAG_INPUT : 0);

    clientGetWMProtocols (c);
    clientGetMWMHints (c, FALSE);
    getHint (dpy, w, win_hints, &c->win_hints);
    getHint (dpy, w, win_state, &c->win_state);
    if (!getHint (dpy, w, win_layer, &c->win_layer))
    {
        c->win_layer = WIN_LAYER_NORMAL;
    }

    /* Reload from session */
    if (sessionMatchWinToSM (c))
    {
        FLAG_SET (c->flags, CLIENT_FLAG_SESSION_MANAGED);
    }

    /* Beware, order of calls is important here ! */
    sn_client_startup_properties (c);
    clientGetWinState (c);
    clientGetNetState (c);
    clientGetNetWmType (c);
    clientGetInitialNetWmDesktop (c);
    clientGetNetStruts (c);

    c->legacy_fullscreen = FALSE;
    /* Fullscreen for older legacy apps */
    if ((c->x == 0) && (c->y == 0) &&
        (c->width == MyDisplayFullWidth (dpy, screen)) &&
        (c->height == MyDisplayFullHeight (dpy, screen)) &&
        !FLAG_TEST(c->flags, CLIENT_FLAG_HAS_BORDER) &&
        (c->win_layer == WIN_LAYER_NORMAL) &&
        (c->type == WINDOW_NORMAL))
    {
        c->legacy_fullscreen = TRUE;
    }

    /* Once we know the type of window, we can initialize window position */
    if (!FLAG_TEST (c->flags, CLIENT_FLAG_SESSION_MANAGED))
    {
        if ((attr.map_state != IsUnmapped))
        {
            clientGravitate (c, APPLY);
        }
        else
        {
            clientInitPosition (c);
        }
    }

    /* We must call clientApplyInitialState() after having placed the
       window so that the inital position values are correctly set if the
       inital state is maximize or fullscreen
     */
    clientApplyInitialState (c);

    if (!recapture)
    {
        MyXGrabServer ();
    }
    if (!MyCheckWindow(w))
    {
        TRACE ("Client has vanished");
        clientFree(c);
        if (!recapture)
        {
            MyXUngrabServer ();
        }
        gdk_error_trap_pop ();
        return;
    }

    valuemask = CWEventMask;
    attributes.event_mask = (FRAME_EVENT_MASK | POINTER_EVENT_MASK);
    c->frame =
        XCreateWindow (dpy, root, frameX (c), frameY (c), frameWidth (c),
        frameHeight (c), 0, CopyFromParent, InputOutput, CopyFromParent,
        valuemask, &attributes);
    XSetWindowBorderWidth (dpy, c->window, 0);
    XReparentWindow (dpy, c->window, c->frame, frameLeft (c), frameTop (c));
    
    valuemask = CWEventMask;
    attributes.event_mask = (CLIENT_EVENT_MASK);
    XChangeWindowAttributes (dpy, c->window, valuemask, &attributes);
    if (shape)
    {
        XShapeSelectInput (dpy, c->window, ShapeNotifyMask);
    }
    if (!recapture)
    {
        /* Window is reparented now, so we can safely release the grab 
         * on the server 
         */
        MyXUngrabServer ();
    }   

    clientAddToList (c);
    clientSetNetActions (c);
    clientGrabKeys (c);
    clientGrabButtons(c);

    /* Initialize pixmap caching */
    myPixmapInit (&c->pm_cache.pm_title[ACTIVE]);
    myPixmapInit (&c->pm_cache.pm_title[INACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_LEFT][ACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_LEFT][INACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_RIGHT][ACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_RIGHT][INACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_BOTTOM][ACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_BOTTOM][INACTIVE]);
    c->pm_cache.previous_width = -1;
    c->pm_cache.previous_height = -1;

    myWindowCreate (dpy, c->frame, &c->sides[SIDE_LEFT],
        resize_cursor[4 + SIDE_LEFT]);
    myWindowCreate (dpy, c->frame, &c->sides[SIDE_RIGHT],
        resize_cursor[4 + SIDE_RIGHT]);
    myWindowCreate (dpy, c->frame, &c->sides[SIDE_BOTTOM],
        resize_cursor[4 + SIDE_BOTTOM]);
    myWindowCreate (dpy, c->frame, &c->corners[CORNER_BOTTOM_LEFT],
        resize_cursor[CORNER_BOTTOM_LEFT]);
    myWindowCreate (dpy, c->frame, &c->corners[CORNER_BOTTOM_RIGHT],
        resize_cursor[CORNER_BOTTOM_RIGHT]);
    myWindowCreate (dpy, c->frame, &c->corners[CORNER_TOP_LEFT],
        resize_cursor[CORNER_TOP_LEFT]);
    myWindowCreate (dpy, c->frame, &c->corners[CORNER_TOP_RIGHT],
        resize_cursor[CORNER_TOP_RIGHT]);
    myWindowCreate (dpy, c->frame, &c->title, None);
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        myWindowCreate (dpy, c->frame, &c->buttons[i], None);
    }
    TRACE ("now calling configure for the new window \"%s\" (0x%lx)", c->name,
        c->window);
    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    clientConfigure (c, &wc, CWX | CWY | CWHeight | CWWidth, CFG_NOTIFY | CFG_FORCE_REDRAW);
    clientApplyStackList (windows_stack);
    last_raise = c;

    /* First map is used to bypass the caching system at first map */
    c->first_map = TRUE;
    if (!FLAG_TEST (c->flags, CLIENT_FLAG_HIDDEN))
    {
        if ((c->win_workspace == workspace) || 
            FLAG_TEST(c->flags, CLIENT_FLAG_STICKY))
        {
            clientShow (c, TRUE);
            if (recapture)
            {
                clientSortRing(c);
            }
            else
            {
                clientFocusNew(c);
            }
        }
    }
    else
    {
        setWMState (dpy, c->window, IconicState);
        clientSetNetState (c);
    }
    gdk_error_trap_pop ();

    DBG ("client \"%s\" (0x%lx) is now managed", c->name, c->window);
    DBG ("client_count=%d", client_count);
}

void
clientUnframe (Client * c, gboolean remap)
{
    int i;
    XEvent ev;
    gboolean reparented;

    TRACE ("entering clientUnframe");
    TRACE ("unframing client \"%s\" (0x%lx) [%s]", 
            c->name, c->window, remap ? "remap" : "no remap");

    g_return_if_fail (c != NULL);
    if (client_focus == c)
    {
        client_focus = NULL;
    }
    if (last_raise == c)
    {
        last_raise = NULL;
    }
    if (last_ungrab == c)
    {
        last_ungrab = NULL;
    }

    clientRemoveFromList (c);
    MyXGrabServer ();
    gdk_error_trap_push ();
    clientUngrabKeys (c);
    clientGrabButtons (c);
    XUnmapWindow (dpy, c->window);
    XUnmapWindow (dpy, c->frame);
    clientGravitate (c, REMOVE);
    XSelectInput (dpy, c->window, NoEventMask);
    reparented = XCheckTypedWindowEvent (dpy, c->window, ReparentNotify, &ev);

    if (remap || !reparented)
    {
        XReparentWindow (dpy, c->window, root, c->x, c->y);
        XSetWindowBorderWidth (dpy, c->window, c->border_width);
        if (remap)
        {
            XMapWindow (dpy, c->window);
        }
        else
        {
            setWMState (dpy, c->window, WithdrawnState);
        }
    }

    if (!remap)
    {
        XDeleteProperty (dpy, c->window, net_wm_state);
        XDeleteProperty (dpy, c->window, win_state);
        XDeleteProperty (dpy, c->window, net_wm_desktop);
        XDeleteProperty (dpy, c->window, win_workspace);
        XDeleteProperty (dpy, c->window, net_wm_allowed_actions);
    }
    
    myWindowDelete (&c->title);
    myWindowDelete (&c->sides[SIDE_LEFT]);
    myWindowDelete (&c->sides[SIDE_RIGHT]);
    myWindowDelete (&c->sides[SIDE_BOTTOM]);
    myWindowDelete (&c->sides[CORNER_BOTTOM_LEFT]);
    myWindowDelete (&c->sides[CORNER_BOTTOM_RIGHT]);
    myWindowDelete (&c->sides[CORNER_TOP_LEFT]);
    myWindowDelete (&c->sides[CORNER_TOP_RIGHT]);
    clientClearPixmapCache (c);
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        myWindowDelete (&c->buttons[i]);
    }
    XDestroyWindow (dpy, c->frame);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUTS))
    {
        workspaceUpdateArea (margins, gnome_margins);
    }
    
    MyXUngrabServer ();
    gdk_error_trap_pop ();
    clientFree (c);
}

void
clientFrameAll ()
{
    unsigned int count, i;
    Client *new_focus;
    Window shield, w1, w2, *wins = NULL;
    XWindowAttributes attr;

    TRACE ("entering clientFrameAll");

    /* Since this fn is called at startup, it's safe to initialize some vars here */
    client_count = 0;
    clients = NULL;
    windows = NULL;
    windows_stack = NULL;
    client_focus = NULL;

    clientSetFocus (NULL, FOCUS_NONE);
    shield =
        setTmpEventWin (0, 0, 
                        MyDisplayFullWidth (dpy, screen),
                        MyDisplayFullHeight (dpy, screen), 
                        EnterWindowMask);

    XSync (dpy, FALSE);
    MyXGrabServer ();
    XQueryTree (dpy, root, &w1, &w2, &wins, &count);
    for (i = 0; i < count; i++)
    {
        XGetWindowAttributes (dpy, wins[i], &attr);
        if ((!(attr.override_redirect)) && (attr.map_state == IsViewable))
        {
            clientFrame (wins[i], TRUE);
        }
    }
    if (wins)
    {
        XFree (wins);
    }
    new_focus = clientGetTopMostFocusable (WIN_LAYER_NORMAL, NULL);
    clientSetFocus (new_focus, FOCUS_SORT);
    removeTmpEventWin (shield);
    MyXUngrabServer ();
    XSync (dpy, FALSE);
}

void
clientUnframeAll ()
{
    Client *c;
    unsigned int count, i;
    Window w1, w2, *wins = NULL;

    TRACE ("entering clientUnframeAll");

    clientSetFocus (NULL, FOCUS_IGNORE_MODAL);
    XSync (dpy, FALSE);
    MyXGrabServer ();
    XQueryTree (dpy, root, &w1, &w2, &wins, &count);
    for (i = 0; i < count; i++)
    {
        c = clientGetFromWindow (wins[i], FRAME);
        if (c)
        {
            clientUnframe (c, TRUE);
        }
    }
    MyXUngrabServer ();
    XSync(dpy, FALSE);
    if (wins)
    {
        XFree (wins);
    }
}

Client *
clientGetFromWindow (Window w, int mode)
{
    Client *c;
    int i;

    g_return_val_if_fail (w != None, NULL);
    TRACE ("entering clientGetFromWindow");
    TRACE ("looking for (0x%lx)", w);

    for (c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        switch (mode)
        {
            case WINDOW:
                if (c->window == w)
                {
                    TRACE ("found \"%s\" (mode WINDOW)", c->name);
                    return (c);
                }
                break;
            case FRAME:
                if (c->frame == w)
                {
                    TRACE ("found \"%s\" (mode FRAME)", c->name);
                    return (c);
                }
                break;
            case ANY:
            default:
                if ((c->frame == w) || (c->window == w))
                {
                    TRACE ("found \"%s\" (mode ANY)", c->name);
                    return (c);
                }
                break;
        }
    }
    TRACE ("no client found");

    return NULL;
}

Client *
clientAtPosition (int x, int y, Client * exclude)
{
    /* This function does the same as XQueryPointer but w/out the race
       conditions caused by querying the X server
     */
    GList *index;
    Client *c = NULL;
    Client *c2 = NULL;

    TRACE ("entering clientAtPos");

    for (index = g_list_last (windows_stack); index; index = g_list_previous (index))
    {
        c2 = (Client *) index->data;
        if (clientSelectMask (c2, 0) && (c2 != exclude))
        {
            if ((frameX (c2) < x) && (frameX (c2) + frameWidth (c2) > x)
                && (frameY (c2) < y) && (frameY (c2) + frameHeight (c2) > y))
            {
                c = c2;
                break;
            }
        }
    }

    return c;
}

static inline gboolean
clientSelectMask (Client * c, int mask)
{
    gboolean okay = TRUE;

    TRACE ("entering clientSelectMask");
    if ((!clientAcceptFocus (c)) && !(mask & INCLUDE_SKIP_FOCUS))
    {
        okay = FALSE;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_HIDDEN) && !(mask & INCLUDE_HIDDEN))
    {
        okay = FALSE;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_PAGER)
        && !(mask & INCLUDE_SKIP_PAGER))
    {
        okay = FALSE;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_TASKBAR)
        && !(mask & INCLUDE_SKIP_TASKBAR))
    {
        okay = FALSE;
    }
    if ((c->win_workspace != workspace) && !(mask & INCLUDE_ALL_WORKSPACES))
    {
        okay = FALSE;
    }

    return okay;
}

Client *
clientGetNext (Client * c, int mask)
{
    Client *c2;
    unsigned int i;

    TRACE ("entering clientGetNext");

    if (c)
    {
        for (c2 = c->next, i = 0; (c2) && (i < client_count);
            c2 = c2->next, i++)
        {
            if ((c2->type == WINDOW_SPLASHSCREEN)
                || (c2->type == WINDOW_DESKTOP))
            {
                continue;
            }
            if (clientSelectMask (c2, mask))
            {
                return c2;
            }
        }
    }
    return NULL;
}

Client *
clientGetPrevious (Client * c, int mask)
{
    Client *c2;
    unsigned int i;

    TRACE ("entering clientGetPrevious");

    if (c)
    {
        for (c2 = c->prev, i = 0; (c2) && (i < client_count);
            c2 = c2->prev, i++)
        {
            if ((c2->type == WINDOW_SPLASHSCREEN)
                || (c2->type == WINDOW_DESKTOP))
            {
                continue;
            }
            if (clientSelectMask (c2, mask))
            {
                return c2;
            }
        }
    }
    return NULL;
}

/* Build a GList of clients that have a transient relationship */
static GList *
clientListTransient (Client * c)
{
    GList *transients = NULL;
    GList *index1, *index2;
    Client *c2, *c3;

    g_return_val_if_fail (c != NULL, NULL);

    transients = g_list_append (transients, c);
    for (index1 = windows_stack; index1; index1 = g_list_next (index1))
    {
        c2 = (Client *) index1->data;
        if (c2 != c)
        {
            if (clientIsTransientFor (c2, c))
            {
                transients = g_list_append (transients, c2);
            }
            else
            {
                for (index2 = transients; index2;
                    index2 = g_list_next (index2))
                {
                    c3 = (Client *) index2->data;
                    if ((c3 != c2) && clientIsTransientFor (c2, c3))
                    {
                        transients = g_list_append (transients, c2);
                        break;
                    }
                }
            }
        }
    }
    return transients;
}

/* Build a GList of clients that have a transient or modal relationship */
static GList *
clientListTransientOrModal (Client * c)
{
    GList *transients = NULL;
    GList *index1, *index2;
    Client *c2, *c3;

    g_return_val_if_fail (c != NULL, NULL);

    transients = g_list_append (transients, c);
    for (index1 = windows_stack; index1; index1 = g_list_next (index1))
    {
        c2 = (Client *) index1->data;
        if (c2 != c)
        {
            if (clientIsTransientOrModalFor (c2, c))
            {
                transients = g_list_append (transients, c2);
            }
            else
            {
                for (index2 = transients; index2;
                    index2 = g_list_next (index2))
                {
                    c3 = (Client *) index2->data;
                    if ((c3 != c2) && clientIsTransientOrModalFor (c2, c3))
                    {
                        transients = g_list_append (transients, c2);
                        break;
                    }
                }
            }
        }
    }
    return transients;
}

void
clientPassFocus (Client * c)
{
    GList *list_of_windows = NULL;
    Client *new_focus = NULL;
    Client *top_most = NULL;
    Client *c2;
    Window dr, window;
    int rx, ry, wx, wy;
    unsigned int i, mask;

    TRACE ("entering clientPassFocus");

    if ((!c) || (c != client_focus))
    {
        return;
    }

    top_most = clientGetTopMostFocusable (c->win_layer, c);
    if (params.click_to_focus)
    {
        /* Fairly simple logic:
           1) if the window is a modal, send focus back to its parent window
           2) Otherwise, rewind the focus stack until we find an eligible window
              (by eligible, I mean a window that is not a transient for the current
              window)
         */
        if (clientIsTransientOrModal (c))
        {
            c2 = clientGetTransient (c);
            
            if (c2 && FLAG_TEST(c2->flags, CLIENT_FLAG_VISIBLE))
            {
                new_focus = c2;
            }
        }
        
        if (!new_focus)
        {
            list_of_windows = clientListTransient (c);
            for (c2 = c->next, i = 0; (c2) && (i < client_count);
                c2 = c2->next, i++)
            {
                if (clientSelectMask (c2, 0)
                    && !g_list_find (list_of_windows, (gconstpointer) c2))
                {
                    new_focus = c2;
                    break;
                }
            }
            g_list_free (list_of_windows);
        }
    }
    else if (XQueryPointer (dpy, root, &dr, &window, &rx, &ry, &wx, &wy,
            &mask))
    {
        new_focus = clientAtPosition (rx, ry, c);
    }
    if (!new_focus)
    {
        new_focus = top_most;
    }
    clientSetFocus (new_focus, (FOCUS_SORT | FOCUS_IGNORE_MODAL));
    if (new_focus == top_most)
    {
        clientPassGrabButton1 (new_focus);
    }
    else if (last_ungrab == c)
    {
        clientPassGrabButton1 (NULL);
    }
}

static inline void
clientShowSingle (Client * c, gboolean change_state)
{
    g_return_if_fail (c != NULL);
    MyXGrabServer ();
    if ((c->win_workspace == workspace)
        || FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        TRACE ("showing client \"%s\" (0x%lx)", c->name, c->window);
        FLAG_SET (c->flags, CLIENT_FLAG_VISIBLE);
        XMapWindow (dpy, c->frame);
        XMapWindow (dpy, c->window);
    }
    if (change_state)
    {
        FLAG_UNSET (c->flags, CLIENT_FLAG_HIDDEN);
        setWMState (dpy, c->window, NormalState);
        workspaceUpdateArea (margins, gnome_margins);
    }
    MyXUngrabServer ();
    clientSetNetState (c);
}

void
clientShow (Client * c, gboolean change_state)
{
    GList *list_of_windows = NULL;
    GList *index;
    Client *c2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientShow \"%s\" (0x%lx) [with %s]", 
           c->name, c->window,
           change_state ? "state change" : "no state change");
             
    list_of_windows = clientListTransientOrModal (c);
    for (index = g_list_last (list_of_windows); index; index = g_list_previous (index))
    {
        c2 = (Client *) index->data;
        clientSetWorkspaceSingle (c2, c->win_workspace);
        /* Ignore request before if the window is not yet managed */
        if (!FLAG_TEST (c2->flags, CLIENT_FLAG_MANAGED))
        {
            continue;
        }
        clientShowSingle (c2, change_state);
    }
    g_list_free (list_of_windows);
}

static inline void
clientHideSingle (Client * c, int ws, gboolean change_state)
{
    g_return_if_fail (c != NULL);
    MyXGrabServer ();
    TRACE ("hiding client \"%s\" (0x%lx)", c->name, c->window);
    clientPassFocus(c);
    XUnmapWindow (dpy, c->window);
    XUnmapWindow (dpy, c->frame);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_VISIBLE))
    {
        FLAG_UNSET (c->flags, CLIENT_FLAG_VISIBLE);
        c->ignore_unmap++;
    }
    if (change_state)
    {
        FLAG_SET (c->flags, CLIENT_FLAG_HIDDEN);
        setWMState (dpy, c->window, IconicState);
        workspaceUpdateArea (margins, gnome_margins);
    }
    MyXUngrabServer ();
    clientSetNetState (c);
}

void
clientHide (Client * c, int ws, gboolean change_state)
{
    GList *list_of_windows = NULL;
    GList *index;
    Client *c2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientHide");

    list_of_windows = clientListTransientOrModal (c);
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;

        /* Ignore request before if the window is not yet managed */
        if (!FLAG_TEST (c2->flags, CLIENT_FLAG_MANAGED))
        {
            continue;
        }

        /* ws is used when transitioning between desktops, to avoid
           hiding a transient for group that will be shown again on the new
           workspace (transient for groups can be transients for multiple 
           ancesors splitted across workspaces...)
         */
        if (clientIsTransientOrModalForGroup (c2)
            && clientTransientOrModalHasAncestor (c2, ws))
        {
            /* Other ancestors for that transient are still on screen, so don't
               hide it...
             */
            continue;
        }
        clientHideSingle (c2, ws, change_state);
    }
    g_list_free (list_of_windows);
}

void
clientHideAll (Client * c, int ws)
{
    Client *c2;
    int i;

    TRACE ("entering clientHideAll");

    for (c2 = c->next, i = 0; (c2) && (i < client_count); c2 = c2->next, i++)
    {
        if (CLIENT_CAN_HIDE_WINDOW (c2)
            && FLAG_TEST (c2->flags, CLIENT_FLAG_HAS_BORDER)
            && !clientIsTransientOrModal (c2) && (c2 != c))
        {
            if (((!c) && (c2->win_workspace == ws)) || ((c)
                    && !clientIsTransientOrModalFor (c, c2)
                    && (c2->win_workspace == c->win_workspace)))
            {
                clientHide (c2, ws, TRUE);
            }
        }
    }
}

void
clientClose (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientClose");
    TRACE ("closing client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->wm_flags, WM_FLAG_DELETE))
    {
        sendClientMessage (c->window, wm_protocols, wm_delete_window, CurrentTime);
    }
    else
    {
        clientKill (c);
    }
}

void
clientKill (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientKill");
    TRACE ("killing client \"%s\" (0x%lx)", c->name, c->window);

    XKillClient (dpy, c->window);
}

void
clientEnterContextMenuState (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientEnterContextMenuState");
    TRACE ("Showing the what's this help for client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->wm_flags, WM_FLAG_CONTEXT_HELP))
    {
        sendClientMessage (c->window, wm_protocols, kde_net_wm_context_help, CurrentTime);
    }
}

void
clientRaise (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientRaise");

    if (c == last_raise)
    {
        TRACE ("client \"%s\" (0x%lx) already raised", c->name, c->window);
        return;
    }
    TRACE ("raising client \"%s\" (0x%lx)", c->name, c->window);

    if (g_list_length (windows_stack) < 1)
    {
        return;
    }

    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
    {
        Client *c2, *c3;
        Client *client_sibling = NULL;
        GList *transients = NULL;
        GList *sibling = NULL;
        GList *index1, *index2;
        GList *windows_stack_copy;

        /* Copy the existing window stack temporarily as reference */
        windows_stack_copy = g_list_copy (windows_stack);
        /* Search for the window that will be just on top of the raised window (layers...) */
        client_sibling = clientGetNextTopMost (c->win_layer, c);
        windows_stack = g_list_remove (windows_stack, (gconstpointer) c);
        if (client_sibling)
        {
            /* If there is one, look for its place in the list */
            sibling = g_list_find (windows_stack, (gconstpointer) client_sibling);
            /* Place the raised window just before it */
            windows_stack = g_list_insert_before (windows_stack, sibling, c);
        }
        else
        {
            /* There will be no window on top of the raised window, so place it at the end of list */
            windows_stack = g_list_append (windows_stack, c);
        }
        /* Now, look for transients, transients of transients, etc. */
        for (index1 = windows_stack_copy; index1; index1 = g_list_next (index1))
        {
            c2 = (Client *) index1->data;
            if (c2)
            {
                if ((c2 != c) && clientIsTransientOrModalFor (c2, c))
                {
                    transients = g_list_append (transients, c2);
                    if (sibling)
                    {
                        /* Make sure client_sibling is not c2 otherwise we create a circular linked list */
                        if (client_sibling != c2)
                        {
                            /* Place the transient window just before sibling */
                            windows_stack = g_list_remove (windows_stack, (gconstpointer) c2);
                            windows_stack = g_list_insert_before (windows_stack, sibling, c2);
                        }
                    }
                    else
                    {
                        /* There will be no window on top of the transient window, so place it at the end of list */
                        windows_stack = g_list_remove (windows_stack, (gconstpointer) c2);
                        windows_stack = g_list_append (windows_stack, c2);
                    }
                }
                else
                {
                    for (index2 = transients; index2; index2 = g_list_next (index2))
                    {
                        c3 = (Client *) index2->data;
                        if ((c3 != c2) && clientIsTransientOrModalFor (c2, c3))
                        {
                            transients = g_list_append (transients, c2);
                            if (sibling)
                            {
                                /* Again, make sure client_sibling is not c2 to avoid a circular linked list */
                                if (client_sibling != c2)
                                {
                                    /* Place the transient window just before sibling */
                                    windows_stack = g_list_remove (windows_stack, (gconstpointer) c2);
                                    windows_stack = g_list_insert_before (windows_stack, sibling, c2);
                                }
                            }
                            else
                            {
                                /* There will be no window on top of the transient window, so place it at the end of list */
                                windows_stack = g_list_remove (windows_stack, (gconstpointer) c2);
                                windows_stack = g_list_append (windows_stack, c2);
                            }
                            break;
                        }
                    }
                }
            }
        }
        if (transients)
        {
            g_list_free (transients);
        }
        if (windows_stack_copy)
        {
            g_list_free (windows_stack_copy);
        }
        /* Now, windows_stack contains the correct window stack
           We still need to tell the X Server to reflect the changes 
         */
        clientApplyStackList (windows_stack);
        clientSetNetClientList (net_client_list_stacking, windows_stack);
        last_raise = c;
    }
}

void
clientLower (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientLower");
    TRACE ("lowering client \"%s\" (0x%lx)", c->name, c->window);

    if (g_list_length (windows_stack) < 1)
    {
        return;
    }

    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
    {
        Client *client_sibling = NULL;

        if (clientIsTransientOrModalForGroup (c))
        {
            client_sibling = clientGetTopMostForGroup (c);
        }
        else if (clientIsTransient (c))
        {
            client_sibling = clientGetTransient (c);
        }
        if (!client_sibling)
        {
            client_sibling = clientGetBottomMost (c->win_layer, c);
        }
        windows_stack = g_list_remove (windows_stack, (gconstpointer) c);
        if (client_sibling)
        {
            GList *sibling = g_list_find (windows_stack,
                (gconstpointer) client_sibling);
            gint position = g_list_position (windows_stack, sibling) + 1;
            windows_stack = g_list_insert (windows_stack, c, position);
            TRACE ("lowest client is \"%s\" (0x%lx) at position %i",
                client_sibling->name, client_sibling->window, position);
        }
        else
        {
            windows_stack = g_list_prepend (windows_stack, c);
        }
        /* Now, windows_stack contains the correct window stack
           We still need to tell the X Server to reflect the changes 
         */
        clientApplyStackList (windows_stack);
        clientSetNetClientList (net_client_list_stacking, windows_stack);
        if (last_raise == c)
        {
            last_raise = NULL;
        }
    }
}

void
clientSetLayer (Client * c, int l)
{
    GList *list_of_windows = NULL;
    GList *index;
    Client *c2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetLayer");

    list_of_windows = clientListTransientOrModal (c);
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if (c2->win_layer != l)
        {
            TRACE ("setting client \"%s\" (0x%lx) layer to %d", c2->name,
                c2->window, l);
            c2->win_layer = l;
            setHint (dpy, c2->window, win_layer, l);
        }
    }
    g_list_free (list_of_windows);
    if (last_raise == c)
    {
        last_raise = NULL;
    }
    clientRaise (c);
    clientPassGrabButton1 (c);
}

static inline void
clientSetWorkspaceSingle (Client * c, int ws)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientSetWorkspaceSingle");

    if (ws > params.workspace_count - 1)
    {
        ws = params.workspace_count - 1;
        TRACE ("value off limits, using %i instead", ws);
    }

    if (c->win_workspace != ws)
    {
        TRACE ("setting client \"%s\" (0x%lx) to workspace %d", c->name,
            c->window, ws);
        c->win_workspace = ws;
        setHint (dpy, c->window, win_workspace, ws);
        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
        {
            setHint (dpy, c->window, net_wm_desktop, (unsigned long) ALL_WORKSPACES);
        }
        else
        {
            setHint (dpy, c->window, net_wm_desktop, (unsigned long) ws);
        }
    }
    FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
}

void
clientSetWorkspace (Client * c, int ws, gboolean manage_mapping)
{
    GList *list_of_windows = NULL;
    GList *index;
    Client *c2;

    g_return_if_fail (c != NULL);

    TRACE ("entering clientSetWorkspace");

    list_of_windows = clientListTransientOrModal (c);
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if (c2->win_workspace != ws)
        {
            TRACE ("setting client \"%s\" (0x%lx) to workspace %d", c->name,
                c->window, ws);
            clientSetWorkspaceSingle (c2, ws);
            if (manage_mapping && !clientIsTransientOrModal (c2)
                && !FLAG_TEST (c2->flags, CLIENT_FLAG_HIDDEN))
            {
                if (FLAG_TEST (c2->flags, CLIENT_FLAG_STICKY))
                {
                    clientShow (c2, FALSE);
                }
                else
                {
                    if (ws == workspace)
                    {
                        clientShow (c2, FALSE);
                    }
                    else
                    {
                        clientHide (c2, workspace, FALSE);
                    }
                }
            }
        }
    }
    g_list_free (list_of_windows);
}

void
clientShade (Client * c)
{
    XWindowChanges wc;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleShaded");
    TRACE ("shading client \"%s\" (0x%lx)", c->name, c->window);

    if (!FLAG_TEST (c->flags, CLIENT_FLAG_HAS_BORDER)
        || FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE
            ("cowardly refusing to shade \"%s\" (0x%lx) because it has no border",
            c->name, c->window);
        return;
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        TRACE ("\"%s\" (0x%lx) is already shaded", c->name, c->window);
        return;
    }

    c->win_state |= WIN_STATE_SHADED;
    FLAG_SET (c->flags, CLIENT_FLAG_SHADED);
    clientSetNetState (c);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
    {
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure (c, &wc, CWWidth | CWHeight, CFG_FORCE_REDRAW);
    }
}

void
clientUnshade (Client * c)
{
    XWindowChanges wc;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleShaded");
    TRACE ("shading/unshading client \"%s\" (0x%lx)", c->name, c->window);

    if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        TRACE ("\"%s\" (0x%lx) is not shaded", c->name, c->window);
        return;
    }
    c->win_state &= ~WIN_STATE_SHADED;
    FLAG_UNSET (c->flags, CLIENT_FLAG_SHADED);
    clientSetNetState (c);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
    {
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure (c, &wc, CWWidth | CWHeight, CFG_FORCE_REDRAW);
    }
}

void
clientToggleShaded (Client * c)
{
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        clientUnshade (c);
    }
    else
    {
        clientShade (c);
    }
}

void
clientStick (Client * c, gboolean include_transients)
{
    GList *list_of_windows = NULL;
    GList *index;
    Client *c2 = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientStick");

    if (include_transients)
    {
        list_of_windows = clientListTransientOrModal (c);
        for (index = list_of_windows; index; index = g_list_next (index))
        {
            c2 = (Client *) index->data;
            TRACE ("sticking client \"%s\" (0x%lx)", c2->name, c2->window);
            c2->win_state |= WIN_STATE_STICKY;
            FLAG_SET (c2->flags, CLIENT_FLAG_STICKY);
            setHint (dpy, c2->window, net_wm_desktop,
                (unsigned long) ALL_WORKSPACES);
            clientSetNetState (c2);
        }
        clientSetWorkspace (c, workspace, TRUE);
        g_list_free (list_of_windows);
    }
    else
    {
        TRACE ("sticking client \"%s\" (0x%lx)", c->name, c->window);
        c->win_state |= WIN_STATE_STICKY;
        FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
        setHint (dpy, c->window, net_wm_desktop,
            (unsigned long) ALL_WORKSPACES);
        clientSetNetState (c);
        clientSetWorkspace (c, workspace, TRUE);
    }
}

void
clientUnstick (Client * c, gboolean include_transients)
{
    GList *list_of_windows = NULL;
    GList *index;
    Client *c2 = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUnstick");
    TRACE ("unsticking client \"%s\" (0x%lx)", c->name, c->window);

    if (include_transients)
    {
        list_of_windows = clientListTransientOrModal (c);
        for (index = list_of_windows; index; index = g_list_next (index))
        {
            c2 = (Client *) index->data;
            c2->win_state &= ~WIN_STATE_STICKY;
            FLAG_UNSET (c2->flags, CLIENT_FLAG_STICKY);
            setHint (dpy, c2->window, net_wm_desktop,
                (unsigned long) workspace);
            clientSetNetState (c2);
        }
        clientSetWorkspace (c, workspace, TRUE);
        g_list_free (list_of_windows);
    }
    else
    {
        c->win_state &= ~WIN_STATE_STICKY;
        FLAG_UNSET (c->flags, CLIENT_FLAG_STICKY);
        setHint (dpy, c->window, net_wm_desktop,
            (unsigned long) workspace);
        clientSetNetState (c);
        clientSetWorkspace (c, workspace, TRUE);
    }
}

void
clientToggleSticky (Client * c, gboolean include_transients)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleSticky");
    TRACE ("sticking/unsticking client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        clientUnstick (c, include_transients);
    }
    else
    {
        clientStick (c, include_transients);
    }
}

static void
clientUpdateFullscreenState (Client * c)
{
    XWindowChanges wc;
    int layer;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateFullscreenState");
    TRACE ("Update fullscreen state for client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        int cx, cy;

        cx = frameX (c) + (frameWidth (c) / 2);
        cy = frameY (c) + (frameHeight (c) / 2);

        c->fullscreen_old_x = c->x;
        c->fullscreen_old_y = c->y;
        c->fullscreen_old_width = c->width;
        c->fullscreen_old_height = c->height;
        c->fullscreen_old_layer = c->win_layer;

        wc.x = MyDisplayX (cx, cy);
        wc.y = MyDisplayY (cx, cy);
        wc.width = MyDisplayWidth (dpy, screen, cx, cy);
        wc.height = MyDisplayHeight (dpy, screen, cx, cy);
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
    clientSetNetState (c);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
    {
        clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_NONE);
    }
    else
    {
        c->x = wc.x;
        c->y = wc.y;
        c->height = wc.height;
        c->width = wc.width;
    }
    clientSetLayer (c, layer);
}

void clientToggleFullscreen (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleFullscreen");
    TRACE ("toggle fullscreen client \"%s\" (0x%lx)", c->name, c->window);

    if (!clientIsTransientOrModal (c))
    {
        FLAG_TOGGLE (c->flags, CLIENT_FLAG_FULLSCREEN);
        clientUpdateAboveState (c);
    }
}

static void
clientUpdateAboveState (Client * c)
{
    int layer;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateAboveState");
    TRACE ("Update above state for client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
    {
        layer = WIN_LAYER_ABOVE_DOCK;
    }
    else
    {
        layer = c->initial_layer;
    }
    clientSetNetState (c);
    clientSetLayer (c, layer);
}

void clientToggleAbove (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleAbove");
    TRACE ("toggle above client \"%s\" (0x%lx)", c->name, c->window);

    if (!clientIsTransientOrModal (c) && !FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
    {
        FLAG_TOGGLE (c->flags, CLIENT_FLAG_ABOVE);
        clientUpdateAboveState (c);
    }
}

static void
clientUpdateBelowState (Client * c)
{
    int layer;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateBelowState");
    TRACE ("Update below state for client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
    {
        layer = WIN_LAYER_BELOW;
    }
    else
    {
        layer = c->initial_layer;
    }
    clientSetNetState (c);
    clientSetLayer (c, layer);
}

void clientToggleBelow (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleBelow");
    TRACE ("toggle below client \"%s\" (0x%lx)", c->name, c->window);

    if (!clientIsTransientOrModal (c) && !FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
    {
        FLAG_TOGGLE (c->flags, CLIENT_FLAG_BELOW);
        clientUpdateAboveState (c);
    }
}

inline void
clientRemoveMaximizeFlag (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientRemoveMaximizeFlag");
    TRACE ("Removing maximize flag on client \"%s\" (0x%lx)", c->name,
        c->window);

    c->win_state &= ~WIN_STATE_MAXIMIZED;
    FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED);
    frameDraw (c, FALSE, FALSE);
    clientSetNetState (c);
}

void
clientToggleMaximized (Client * c, int mode)
{
    XWindowChanges wc;
    int cx, cy, left, right, top, bottom;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleMaximized");
    TRACE ("maximzing/unmaximizing client \"%s\" (0x%lx)", c->name,
        c->window);

    if (!CLIENT_CAN_MAXIMIZE_WINDOW (c))
    {
        return;
    }

    cx = frameX (c) + (frameWidth (c) / 2);
    cy = frameY (c) + (frameHeight (c) / 2);

    left =
        (isLeftMostHead (dpy, screen, cx,
            cy) ? MAX ((int) margins[MARGIN_LEFT],
            params.xfwm_margins[MARGIN_LEFT]) : 0);
    right =
        (isRightMostHead (dpy, screen, cx,
            cy) ? MAX ((int) margins[MARGIN_RIGHT],
            params.xfwm_margins[MARGIN_RIGHT]) : 0);
    top =
        (isTopMostHead (dpy, screen, cx, cy) ? MAX ((int) margins[MARGIN_TOP],
            params.xfwm_margins[MARGIN_TOP]) : 0);
    bottom =
        (isBottomMostHead (dpy, screen, cx,
            cy) ? MAX ((int) margins[MARGIN_BOTTOM],
            params.xfwm_margins[MARGIN_BOTTOM]) : 0);

    if (mode & WIN_STATE_MAXIMIZED_HORIZ)
    {
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ))
        {
            c->old_x = c->x;
            c->old_width = c->width;
            wc.x = MyDisplayX (cx, cy) + frameLeft (c) + left;
            wc.width =
                MyDisplayWidth (dpy, screen, cx,
                cy) - frameLeft (c) - frameRight (c) - left - right;
            c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
        }
        else
        {
            wc.x = c->old_x;
            wc.width = c->old_width;
            c->win_state &= ~WIN_STATE_MAXIMIZED_HORIZ;
            FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
        }
    }
    else
    {
        wc.x = c->x;
        wc.width = c->width;
    }

    if (mode & WIN_STATE_MAXIMIZED_VERT)
    {
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT))
        {
            c->old_y = c->y;
            c->old_height = c->height;
            wc.y = MyDisplayY (cx, cy) + frameTop (c) + top;
            wc.height =
                MyDisplayHeight (dpy, screen, cx,
                cy) - frameTop (c) - frameBottom (c) - top - bottom;
            c->win_state |= WIN_STATE_MAXIMIZED_VERT;
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
        }
        else
        {
            wc.y = c->old_y;
            wc.height = c->old_height;
            c->win_state &= ~WIN_STATE_MAXIMIZED_VERT;
            FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
        }
    }
    else
    {
        wc.y = c->y;
        wc.height = c->height;
    }

    clientSetNetState (c);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
    {
        clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_NOTIFY);
    }
    else
    {
        c->x = wc.x;
        c->y = wc.y;
        c->height = wc.height;
        c->width = wc.width;
    }
}

inline gboolean
clientAcceptFocus (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientAcceptFocus");

    /* Modal dialogs *always* accept focus */
    if (FLAG_TEST(c->flags, CLIENT_FLAG_STATE_MODAL))
    {
        return TRUE; 
    }
    /* First check GNOME protocol */
    if (c->win_hints & WIN_HINTS_SKIP_FOCUS)
    {
        return FALSE;
    }
    /* then try ICCCM */
    else if (FLAG_TEST (c->wm_flags, WM_FLAG_TAKEFOCUS))
    {
        return TRUE;
    }
    /* At last, use wmhints */
    return (FLAG_TEST (c->wm_flags, WM_FLAG_INPUT) ? TRUE : FALSE);
}

static inline void
clientSortRing(Client *c)
{
    g_return_if_fail (c != NULL);

    TRACE ("Sorting...");
    if (client_count > 2 && c != clients)
    {
        c->prev->next = c->next;
        c->next->prev = c->prev;

        c->prev = clients->prev;
        c->next = clients;
        clients->prev->next = c;
        clients->prev = c;
    }
    clients = c;
}

void
clientUpdateFocus (Client * c, unsigned short flags)
{
    Client *c2 = ((client_focus != c) ? client_focus : NULL);
    unsigned long data[2];

    TRACE ("entering clientUpdateFocus");

    if ((c) && !clientAcceptFocus (c))
    {
        TRACE ("SKIP_FOCUS set for client \"%s\" (0x%lx)", c->name,
            c->window);
        return;
    }
    if ((c == client_focus) && !(flags & FOCUS_FORCE))
    {
        TRACE ("client \"%s\" (0x%lx) is already focused, ignoring request",
            c->name, c->window);
        return;
    }
    client_focus = c;
    if (c)
    {
        clientInstallColormaps (c);
        data[0] = c->window;
        if ((c->legacy_fullscreen) || FLAG_TEST(c->flags, CLIENT_FLAG_FULLSCREEN))
        {
            clientSetLayer (c, WIN_LAYER_ABOVE_DOCK);
        }
        frameDraw (c, FALSE, FALSE);
    }
    else
    {
        data[0] = None;
    }
    if (c2)
    {
        TRACE ("redrawing previous focus client \"%s\" (0x%lx)", c2->name,
            c2->window);
        /* Requires a bit of explanation here... Legacy apps automatically
           switch to above layer when receiving focus, and return to
           normal layer when loosing focus.
           The following "logic" is in charge of that behaviour.
         */
        if ((c2->legacy_fullscreen) || FLAG_TEST(c2->flags, CLIENT_FLAG_FULLSCREEN))
        {
            if (FLAG_TEST(c2->flags, CLIENT_FLAG_FULLSCREEN))
            {
                clientSetLayer (c2, c2->fullscreen_old_layer);
            }
            else
            {
                clientSetLayer (c2, WIN_LAYER_NORMAL);
            }
            if (c)
            {
                clientRaise(c);
                clientPassGrabButton1 (c);
            }
        }
        frameDraw (c2, FALSE, FALSE);
    }
    data[1] = None;
    XChangeProperty (dpy, root, net_active_window, XA_WINDOW, 32,
        PropModeReplace, (unsigned char *) data, 2);
}

void
clientSetFocus (Client * c, unsigned short flags)
{
    Client *c2, *c3;
    unsigned long data[2];

    TRACE ("entering clientSetFocus");
    
    if ((c) && !(flags & FOCUS_IGNORE_MODAL))
    {
        c3 = clientGetModalFor (c);

        if (c3)
        {
            c = c3;
        }
    }
    c2 = ((client_focus != c) ? client_focus : NULL);
    if ((c) && FLAG_TEST (c->flags, CLIENT_FLAG_VISIBLE))
    {
        TRACE ("setting focus to client \"%s\" (0x%lx)", c->name, c->window);
        if (!clientAcceptFocus (c))
        {
            TRACE ("SKIP_FOCUS set for client \"%s\" (0x%lx)", c->name,
                c->window);
            return;
        }
        if ((c == client_focus) && !(flags & FOCUS_FORCE))
        {
            TRACE
                ("client \"%s\" (0x%lx) is already focused, ignoring request",
                c->name, c->window);
            return;
        }        
        client_focus = c;
        clientInstallColormaps (c);
        if (flags & FOCUS_SORT)
        {
            clientSortRing(c);
        }
        XSetInputFocus (dpy, c->window, RevertToNone, CurrentTime);
        XFlush (dpy);
        if (FLAG_TEST(c->wm_flags, WM_FLAG_TAKEFOCUS))
        {
            sendClientMessage (c->window, wm_protocols, wm_takefocus, CurrentTime);
        }
        if ((c->legacy_fullscreen) || FLAG_TEST(c->flags, CLIENT_FLAG_FULLSCREEN))
        {
            clientSetLayer (c, WIN_LAYER_ABOVE_DOCK);
        }
        frameDraw (c, FALSE, FALSE);
        data[0] = c->window;
    }
    else
    {
        TRACE ("setting focus to none");
        client_focus = NULL;
        XSetInputFocus (dpy, gnome_win, RevertToNone, CurrentTime);
        XFlush (dpy);
        data[0] = None;
    }
    if (c2)
    {
        /* Legacy apps layer switching. See comment in clientUpdateFocus () */
        if ((c2->legacy_fullscreen) || FLAG_TEST(c2->flags, CLIENT_FLAG_FULLSCREEN))
        {
            if (FLAG_TEST(c2->flags, CLIENT_FLAG_FULLSCREEN))
            {
                clientSetLayer (c2, c2->fullscreen_old_layer);
            }
            else
            {
                clientSetLayer (c2, WIN_LAYER_NORMAL);
            }
            if (c)
            {
                clientRaise(c);
                clientPassGrabButton1 (c);
            }
        }
        TRACE ("redrawing previous focus client \"%s\" (0x%lx)", c2->name,
            c2->window);
        frameDraw (c2, FALSE, FALSE);
    }
    data[1] = None;
    XChangeProperty (dpy, root, net_active_window, XA_WINDOW, 32,
        PropModeReplace, (unsigned char *) data, 2);
}

Client *
clientGetFocus (void)
{
    return (client_focus);
}

/* Xrandr stuff: on screen size change, make sure all clients are still visible */
void 
clientScreenResize(void)
{
    Client *c = NULL;
    GList *index;
    XWindowChanges wc;
    
    for (index = windows_stack; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        if (!CONSTRAINED_WINDOW (c))
        {
            continue;
        }
        wc.x = c->x;
        wc.y = c->y;
        clientConfigure (c, &wc, CWX | CWY, CFG_CONSTRAINED);
    }
}

void
clientDrawOutline (Client * c)
{
    TRACE ("entering clientDrawOutline");

    XDrawRectangle (dpy, root, params.box_gc, frameX (c), frameY (c),
        frameWidth (c) - 1, frameHeight (c) - 1);
    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER,
            CLIENT_FLAG_FULLSCREEN | CLIENT_FLAG_SHADED))
    {
        XDrawRectangle (dpy, root, params.box_gc, c->x, c->y, c->width - 1,
            c->height - 1);
    }
}

static inline void
clientSnapPosition (Client * c)
{
    int cx, cy, left, right, top, bottom;
    int disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_x, frame_y, frame_height, frame_width;
    int frame_top, frame_left, frame_right, frame_bottom;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSnapPosition");
    TRACE ("Snapping client \"%s\" (0x%lx)", c->name, c->window);

    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_height = frameHeight (c);
    frame_width = frameWidth (c);
    frame_top = frameTop (c);
    frame_left = frameLeft (c);
    frame_right = frameRight (c);
    frame_bottom = frameBottom (c);

    cx = frame_x + (frame_width / 2);
    cy = frame_y + (frame_height / 2);

    left =
        (isLeftMostHead (dpy, screen, cx,
            cy) ? (int) margins[MARGIN_LEFT] : 0);
    right =
        (isRightMostHead (dpy, screen, cx,
            cy) ? (int) margins[MARGIN_RIGHT] : 0);
    top =
        (isTopMostHead (dpy, screen, cx, cy) ? (int) margins[MARGIN_TOP] : 0);
    bottom =
        (isBottomMostHead (dpy, screen, cx,
            cy) ? (int) margins[MARGIN_BOTTOM] : 0);

    if (!(params.snap_to_border) && !(params.snap_to_windows))
    {
        if ((frame_y + frame_top > 0) && (frame_y < top))
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
        Client *c2;

        disp_x = MyDisplayX (cx, cy);
        disp_y = MyDisplayY (cx, cy);
        disp_max_x = MyDisplayMaxX (dpy, screen, cx, cy);
        disp_max_y = MyDisplayMaxY (dpy, screen, cx, cy);

        if (params.snap_to_border)
        {
            if (abs (disp_x + left - frame_x) <
                abs (disp_max_x - right - frame_x2))
            {
                best_delta_x = abs (disp_x + left - frame_x);
                best_frame_x = disp_x + left;
            }
            else
            {
                best_delta_x = abs (disp_max_x - right - frame_x2);
                best_frame_x = disp_max_x - right - frame_width;
            }

            if (abs (disp_y + top - frame_y) <
                abs (disp_max_y - bottom - frame_y2))
            {
                best_delta_y = abs (disp_y + top - frame_y);
                best_frame_y = disp_y + top;
            }
            else
            {
                best_delta_y = abs (disp_max_y - bottom - frame_y2);
                best_frame_y = disp_max_y - bottom - frame_height;
            }
        }

        if (params.snap_to_windows)
        {
            for (c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
            {
                if (FLAG_TEST (c2->flags, CLIENT_FLAG_VISIBLE)
                    && (c2->win_layer == c->win_layer) && (c2 != c))
                {
                    c_frame_x1 = frameX (c2);
                    c_frame_x2 = c_frame_x1 + frameWidth (c2);
                    c_frame_y1 = frameY (c2);
                    c_frame_y2 = c_frame_y1 + frameHeight (c2);

                    if ((c_frame_y1 <= frame_y2) && (c_frame_y2 >= frame_y))
                    {
                        delta = abs (c_frame_x2 - frame_x);
                        if (delta < best_delta_x)
                        {
                            best_delta_x = delta;
                            best_frame_x = c_frame_x2;
                        }

                        delta = abs (c_frame_x1 - frame_x2);
                        if (delta < best_delta_x)
                        {
                            best_delta_x = delta;
                            best_frame_x = c_frame_x1 - frame_width;
                        }
                    }

                    if ((c_frame_x1 <= frame_x2) && (c_frame_x2 >= frame_x))
                    {
                        delta = abs (c_frame_y2 - frame_y);
                        if (delta < best_delta_y)
                        {
                            best_delta_y = delta;
                            best_frame_y = c_frame_y2;
                        }

                        delta = abs (c_frame_y1 - frame_y2);
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

static GtkToXEventFilterStatus
clientMove_event_filter (XEvent * xevent, gpointer data)
{
    static int edge_scroll_x = 0;
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    MoveResizeData *passdata = (MoveResizeData *) data;
    Client *c = passdata->c;
    gboolean moving = TRUE;
    XWindowChanges wc;

    TRACE ("entering clientMove_event_filter");

    last_timestamp = stashEventTime (last_timestamp, xevent);
    if (xevent->type == KeyPress)
    {
        if (passdata->use_keys)
        {
            if (!passdata->grab && params.box_move)
            {
                MyXGrabServer ();
                passdata->grab = TRUE;
                clientDrawOutline (c);
            }
            if (params.box_move)
            {
                clientDrawOutline (c);
            }
            if (xevent->xkey.keycode == params.keys[KEY_MOVE_LEFT].keycode)
            {
                c->x = c->x - 16;
            }
            else if (xevent->xkey.keycode == params.keys[KEY_MOVE_RIGHT].keycode)
            {
                c->x = c->x + 16;
            }
            else if (xevent->xkey.keycode == params.keys[KEY_MOVE_UP].keycode)
            {
                c->y = c->y - 16;
            }
            else if (xevent->xkey.keycode == params.keys[KEY_MOVE_DOWN].keycode)
            {
                c->y = c->y + 16;
            }
            clientConstrainPos (c, FALSE);

            if (params.box_move)
            {
                clientDrawOutline (c);
            }
            else
            {
                wc.x = c->x;
                wc.y = c->y;
                clientConfigure (c, &wc, CWX | CWY, CFG_NONE);
            }
        }
    }
    else if (xevent->type == KeyRelease)
    {
        if (passdata->use_keys)
        {
            if (IsModifierKey (XKeycodeToKeysym (dpy, xevent->xkey.keycode, 0)))
            {
                moving = FALSE;
            }
        }
    }
    else if (xevent->type == MotionNotify)
    {
        while (XCheckMaskEvent (dpy, ButtonMotionMask | PointerMotionMask, xevent))
        {
            last_timestamp = stashEventTime (last_timestamp, xevent);
        }
        
        if (xevent->type == ButtonRelease)
        {
            moving = FALSE;
        }

        if (!passdata->grab && params.box_move)
        {
            MyXGrabServer ();
            passdata->grab = TRUE;
            clientDrawOutline (c);
        }
        if (params.box_move)
        {
            clientDrawOutline (c);
        }

        if ((params.workspace_count > 1) && !clientIsTransientOrModal (c))
        {
            if (params.workspace_count && params.wrap_windows
                && params.wrap_resistance)
            {
                int msx, msy, max;

                msx = xevent->xmotion.x_root;
                msy = xevent->xmotion.y_root;
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
                        XWarpPointer (dpy, None, root, 0, 0, 0, 0, max - 10,
                            msy);
                        msx = xevent->xmotion.x_root = max - 10;
                        workspaceSwitch (workspace - 1, c);
                    }
                    else if (msx == max)
                    {
                        XWarpPointer (dpy, None, root, 0, 0, 0, 0, 10, msy);
                        msx = xevent->xmotion.x_root = 10;
                        workspaceSwitch (workspace + 1, c);
                    }
                }
            }
        }

        c->x = passdata->ox + (xevent->xmotion.x_root - passdata->mx);
        c->y = passdata->oy + (xevent->xmotion.y_root - passdata->my);

        clientSnapPosition (c);

        clientConstrainPos (c, FALSE);
        if (params.box_move)
        {
            clientDrawOutline (c);
        }
        else
        {
            wc.x = c->x;
            wc.y = c->y;
            clientConfigure (c, &wc, CWX | CWY, CFG_NONE);
        }
    }
    else if (xevent->type == ButtonRelease)
    {
        if (!passdata->use_keys)
        {
            moving = FALSE;
        }
    }
    else if (xevent->type == UnmapNotify && xevent->xunmap.window == c->window)
    {
        moving = FALSE;
    }
    else if ((xevent->type == EnterNotify) || (xevent->type == LeaveNotify))
    {
        /* Ignore enter/leave events */
    }
    else
    {
        status = XEV_FILTER_CONTINUE;
    }

    TRACE ("leaving clientMove_event_filter");

    if (!moving)
    {
        TRACE ("event loop now finished");
        edge_scroll_x = 0;
        gtk_main_quit ();
    }

    return status;
}

void
clientMove (Client * c, XEvent * e)
{
    XWindowChanges wc;
    MoveResizeData passdata;
    Cursor cursor = None;
    int g1 = GrabSuccess, g2 = GrabSuccess;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientDoMove");
    TRACE ("moving client \"%s\" (0x%lx)", c->name, c->window);

    passdata.c = c;
    passdata.ox = c->x;
    passdata.oy = c->y;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    /*
       The following trick is experimental, based on a patch for Kwin 3.1alpha1 by aviv bergman

       It is supposed to reduce latency during move/resize by mapping a screen large window that
       receives all pointer events.

       Original mail message is available here :

       http://www.xfree86.org/pipermail/xpert/2002-July/019434.html

       Note:

       I'm not sure it makes any difference, but who knows... It doesn' t hurt.
     */

    passdata.tmp_event_window =
        setTmpEventWin (0, 0, 
                        MyDisplayFullWidth (dpy, screen),
                        MyDisplayFullHeight (dpy, screen), 
                        ButtonMotionMask | ButtonReleaseMask);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
    {
        clientRemoveMaximizeFlag (c);
    }

    if (e->type == KeyPress)
    {
        last_timestamp = stashEventTime (last_timestamp, e);
        cursor = None;
        passdata.use_keys = TRUE;
        passdata.mx = e->xkey.x_root;
        passdata.my = e->xkey.y_root;
    }
    else if (e->type == ButtonPress)
    {
        last_timestamp = stashEventTime (last_timestamp, e);
        cursor = None;
        passdata.mx = e->xbutton.x_root;
        passdata.my = e->xbutton.y_root;
    }
    else
    {
        cursor = move_cursor;
        getMouseXY (root, &passdata.mx, &passdata.my);
    }
    g1 = XGrabKeyboard (dpy, passdata.tmp_event_window, FALSE,
        GrabModeAsync, GrabModeAsync, last_timestamp);
    g2 = XGrabPointer (dpy, passdata.tmp_event_window, FALSE,
        ButtonMotionMask | ButtonReleaseMask, GrabModeAsync,
        GrabModeAsync, None, cursor, last_timestamp);

    if (((passdata.use_keys) && (g1 != GrabSuccess)) || (g2 != GrabSuccess))
    {
        TRACE ("grab failed in clientMove");
        gdk_beep ();
        if ((passdata.use_keys) && (g1 == GrabSuccess))
        {
            XUngrabKeyboard (dpy, CurrentTime);
        }
        if (g2 == GrabSuccess)
        {
            XUngrabPointer (dpy, CurrentTime);
        }
        removeTmpEventWin (passdata.tmp_event_window);
        return;
    }

    if (passdata.use_keys)
    {
        XPutBackEvent (dpy, e);
    }

    FLAG_SET (c->flags, CLIENT_FLAG_MOVING_RESIZING);
    TRACE ("entering move loop");
    pushEventFilter (clientMove_event_filter, &passdata);
    gtk_main ();
    popEventFilter ();
    TRACE ("leaving move loop");
    FLAG_UNSET (c->flags, CLIENT_FLAG_MOVING_RESIZING);

    if (passdata.grab && params.box_move)
    {
        clientDrawOutline (c);
    }
    wc.x = c->x;
    wc.y = c->y;
    clientConfigure (c, &wc, CWX | CWY, CFG_NONE);

    XUngrabKeyboard (dpy, last_timestamp);
    XUngrabPointer (dpy, last_timestamp);
    removeTmpEventWin (passdata.tmp_event_window);

    if (passdata.grab && params.box_move)
    {
        MyXUngrabServer ();
    }
}

static GtkToXEventFilterStatus
clientResize_event_filter (XEvent * xevent, gpointer data)
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

    TRACE ("entering clientResize_event_filter");

    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_height = frameHeight (c);
    frame_width = frameWidth (c);
    frame_top = frameTop (c);
    frame_left = frameLeft (c);
    frame_right = frameRight (c);
    frame_bottom = frameBottom (c);

    cx = frame_x + (frame_width / 2);
    cy = frame_y + (frame_height / 2);

    left =
        (isLeftMostHead (dpy, screen, cx,
            cy) ? (int) margins[MARGIN_LEFT] : 0);
    right =
        (isRightMostHead (dpy, screen, cx,
            cy) ? (int) margins[MARGIN_RIGHT] : 0);
    top =
        (isTopMostHead (dpy, screen, cx, cy) ? (int) margins[MARGIN_TOP] : 0);
    bottom =
        (isBottomMostHead (dpy, screen, cx,
            cy) ? (int) margins[MARGIN_BOTTOM] : 0);

    disp_x = MyDisplayX (cx, cy);
    disp_y = MyDisplayY (cx, cy);
    disp_max_x = MyDisplayMaxX (dpy, screen, cx, cy);
    disp_max_y = MyDisplayMaxY (dpy, screen, cx, cy);

    last_timestamp = stashEventTime (last_timestamp, xevent);
    if (xevent->type == KeyPress)
    {
        if (passdata->use_keys)
        {
            int key_width_inc, key_height_inc;
            int corner = -1;

            key_width_inc = c->size->width_inc;
            key_height_inc = c->size->height_inc;

            if (key_width_inc < 10)
            {
                key_width_inc = ((int) (10 / key_width_inc)) * key_width_inc;
            }

            if (key_height_inc < 10)
            {
                key_height_inc = ((int) (10 / key_height_inc)) * key_height_inc;
            }

            if (!passdata->grab && params.box_resize)
            {
                MyXGrabServer ();
                passdata->grab = TRUE;
                clientDrawOutline (c);
            }
            if (params.box_resize)
            {
                clientDrawOutline (c);
            }
            /* Store previous height in case the resize hides the window behind the curtain */
            prev_width = c->width;
            prev_height = c->height;

            if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
                && (xevent->xkey.keycode == params.keys[KEY_MOVE_UP].keycode))
            {
                c->height = c->height - key_height_inc;
                corner = 4 + SIDE_BOTTOM;
            }
            else if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
                && (xevent->xkey.keycode == params.keys[KEY_MOVE_DOWN].keycode))
            {
                c->height = c->height + key_height_inc;
                corner = 4 + SIDE_BOTTOM;
            }
            else if (xevent->xkey.keycode == params.keys[KEY_MOVE_LEFT].keycode)
            {
                c->width = c->width - key_width_inc;
                corner = 4 + SIDE_RIGHT;
            }
            else if (xevent->xkey.keycode == params.keys[KEY_MOVE_RIGHT].keycode)
            {
                c->width = c->width + key_width_inc;
                corner = 4 + SIDE_RIGHT;
            }
            if (c->x + c->width < disp_x + left + CLIENT_MIN_VISIBLE)
            {
                c->width = prev_width;
            }
            if (c->y + c->height < disp_y + top + CLIENT_MIN_VISIBLE)
            {
                c->height = prev_height;
            }
            if (corner >= 0)
            {
                clientConstrainRatio (c, c->width, c->height, corner);
            }
            if (params.box_resize)
            {
                clientDrawOutline (c);
            }
            else
            {
                wc.x = c->x;
                wc.y = c->y;
                wc.width = c->width;
                wc.height = c->height;
                clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_NONE);
            }
        }
    }
    else if (xevent->type == KeyRelease)
    {
        if (passdata->use_keys)
        {
            if (IsModifierKey (XKeycodeToKeysym (dpy, xevent->xkey.keycode, 0)))
            {
                resizing = FALSE;
            }
        }
    }
    else if (xevent->type == MotionNotify)
    {
        while (XCheckMaskEvent (dpy, ButtonMotionMask | PointerMotionMask, xevent))
        {
            last_timestamp = stashEventTime (last_timestamp, xevent);
        }
        
        if (xevent->type == ButtonRelease)
        {
            resizing = FALSE;
        }

        if (!passdata->grab && params.box_resize)
        {
            MyXGrabServer ();
            passdata->grab = TRUE;
            clientDrawOutline (c);
        }
        if (params.box_resize)
        {
            clientDrawOutline (c);
        }
        passdata->oldw = c->width;
        passdata->oldh = c->height;
        /* Store previous values in case the resize puts the window title off bounds */
        prev_x = c->x;
        prev_y = c->y;
        prev_width = c->width;
        prev_height = c->height;

        if ((passdata->corner == CORNER_TOP_LEFT)
            || (passdata->corner == CORNER_BOTTOM_LEFT)
            || (passdata->corner == 4 + SIDE_LEFT))
        {
            c->width = passdata->ox - (xevent->xmotion.x_root - passdata->mx);
        }
        else if ((passdata->corner == CORNER_BOTTOM_RIGHT)
            || (passdata->corner == CORNER_TOP_RIGHT)
            || (passdata->corner == 4 + SIDE_RIGHT))
        {
            c->width = passdata->ox + (xevent->xmotion.x_root - passdata->mx);
        }
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            if ((passdata->corner == CORNER_TOP_LEFT)
                || (passdata->corner == CORNER_TOP_RIGHT))
            {
                c->height = passdata->oy - (xevent->xmotion.y_root - passdata->my);
            }
            else if ((passdata->corner == CORNER_BOTTOM_RIGHT)
                || (passdata->corner == CORNER_BOTTOM_LEFT)
                || (passdata->corner == 4 + SIDE_BOTTOM))
            {
                c->height = passdata->oy + (xevent->xmotion.y_root - passdata->my);
            }
        }
        clientSetWidth (c, c->width);
        clientSetHeight (c, c->height);
        clientConstrainRatio (c, c->width, c->height, passdata->corner);

        if ((passdata->corner == CORNER_TOP_LEFT)
            || (passdata->corner == CORNER_BOTTOM_LEFT)
            || (passdata->corner == 4 + SIDE_LEFT))
        {
            c->x = c->x - (c->width - passdata->oldw);
            frame_x = frameX (c);
        }
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
            && (passdata->corner == CORNER_TOP_LEFT
                || passdata->corner == CORNER_TOP_RIGHT))
        {
            c->y = c->y - (c->height - passdata->oldh);
            frame_y = frameY (c);
        }
        if ((passdata->corner == CORNER_TOP_LEFT)
            || (passdata->corner == CORNER_TOP_RIGHT))
        {
            if ((frame_y < disp_y + top)
                || (c->y > disp_max_y - bottom - CLIENT_MIN_VISIBLE))
            {
                c->y = prev_y;
                c->height = prev_height;
            }
        }
        else if ((passdata->corner == CORNER_BOTTOM_LEFT)
            || (passdata->corner == CORNER_BOTTOM_RIGHT)
            || (passdata->corner == 4 + SIDE_BOTTOM))
        {
            if (c->y + c->height < disp_y + top + CLIENT_MIN_VISIBLE)
            {
                c->height = prev_height;
            }
        }
        if ((passdata->corner == CORNER_TOP_LEFT)
            || (passdata->corner == CORNER_BOTTOM_LEFT)
            || (passdata->corner == 4 + SIDE_LEFT))
        {
            if (c->x > disp_max_x - right - CLIENT_MIN_VISIBLE)
            {
                c->x = prev_x;
                c->width = prev_width;
            }
        }
        else if ((passdata->corner == CORNER_TOP_RIGHT)
            || (passdata->corner == CORNER_BOTTOM_RIGHT)
            || (passdata->corner == 4 + SIDE_RIGHT))
        {
            if (c->x + c->width < disp_x + left + CLIENT_MIN_VISIBLE)
            {
                c->width = prev_width;
            }
        }
        if (params.box_resize)
        {
            clientDrawOutline (c);
        }
        else
        {
            wc.x = c->x;
            wc.y = c->y;
            wc.width = c->width;
            wc.height = c->height;
            clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_NONE);
        }
    }
    else if (xevent->type == ButtonRelease)
    {
        if (!passdata->use_keys)
        {
            resizing = FALSE;
        }
    }
    else if ((xevent->type == UnmapNotify) && (xevent->xunmap.window == c->window))
    {
        resizing = FALSE;
    }
    else if ((xevent->type == EnterNotify) || (xevent->type == LeaveNotify))
    {
        /* Ignore enter/leave events */
    }
    else
    {
        status = XEV_FILTER_CONTINUE;
    }

    TRACE ("leaving clientResize_event_filter");

    if (!resizing)
    {
        TRACE ("event loop now finished");
        gtk_main_quit ();
    }

    return status;
}

void
clientResize (Client * c, int corner, XEvent * e)
{
    XWindowChanges wc;
    MoveResizeData passdata;
    int g1 = GrabSuccess, g2 = GrabSuccess;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientResize");
    TRACE ("resizing client \"%s\" (0x%lx)", c->name, c->window);

    passdata.c = c;
    passdata.ox = c->width;
    passdata.oy = c->height;
    passdata.corner = CORNER_BOTTOM_RIGHT;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    passdata.corner = corner;
    passdata.tmp_event_window =
        setTmpEventWin (0, 0, 
                        MyDisplayFullWidth (dpy, screen),
                        MyDisplayFullHeight (dpy, screen), 
                        ButtonMotionMask | ButtonReleaseMask);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
    {
        clientRemoveMaximizeFlag (c);
    }

    if (e->type == KeyPress)
    {
        passdata.use_keys = TRUE;
        last_timestamp = stashEventTime (last_timestamp, e);
        passdata.mx = e->xkey.x_root;
        passdata.my = e->xkey.y_root;
    }
    else if (e->type == ButtonPress)
    {
        last_timestamp = stashEventTime (last_timestamp, e);
        passdata.mx = e->xbutton.x_root;
        passdata.my = e->xbutton.y_root;
    }
    else
    {
        getMouseXY (root, &passdata.mx, &passdata.my);
    }
    g1 = XGrabKeyboard (dpy, passdata.tmp_event_window, FALSE,
        GrabModeAsync, GrabModeAsync, last_timestamp);
    g2 = XGrabPointer (dpy, passdata.tmp_event_window, FALSE,
        ButtonMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync,
        None, resize_cursor[passdata.corner], last_timestamp);

    if (((passdata.use_keys) && (g1 != GrabSuccess)) || (g2 != GrabSuccess))
    {
        TRACE ("grab failed in clientResize");
        gdk_beep ();
        if ((passdata.use_keys) && (g1 == GrabSuccess))
        {
            XUngrabKeyboard (dpy, CurrentTime);
        }
        if (g2 == GrabSuccess)
        {
            XUngrabPointer (dpy, CurrentTime);
        }
        removeTmpEventWin (passdata.tmp_event_window);
        return;
    }

    if (passdata.use_keys)
    {
        XPutBackEvent (dpy, e);
    }
    FLAG_SET (c->flags, CLIENT_FLAG_MOVING_RESIZING);
    TRACE ("entering resize loop");
    pushEventFilter (clientResize_event_filter, &passdata);
    gtk_main ();
    popEventFilter ();
    TRACE ("leaving resize loop");
    FLAG_UNSET (c->flags, CLIENT_FLAG_MOVING_RESIZING);

    if (passdata.grab && params.box_resize)
    {
        clientDrawOutline (c);
    }

    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    clientConfigure (c, &wc, CWX | CWY | CWHeight | CWWidth, CFG_NOTIFY);

    XUngrabKeyboard (dpy, last_timestamp);
    XUngrabPointer (dpy, last_timestamp);
    removeTmpEventWin (passdata.tmp_event_window);

    if (passdata.grab && params.box_resize)
    {
        MyXUngrabServer ();
    }
}

static GtkToXEventFilterStatus
clientCycle_event_filter (XEvent * xevent, gpointer data)
{
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    gboolean cycling = TRUE;
    gboolean gone = FALSE;
    ClientCycleData *passdata = (ClientCycleData *) data;

    TRACE ("entering clientCycle_event_filter");

    last_timestamp = stashEventTime (last_timestamp, xevent);
    switch (xevent->type)
    {
        case DestroyNotify:
            gone |=
                (passdata->c ==
                clientGetFromWindow (((XDestroyWindowEvent *) xevent)->window,
                    WINDOW));
            status = XEV_FILTER_CONTINUE;
        case UnmapNotify:
            gone |=
                (passdata->c ==
                clientGetFromWindow (((XUnmapEvent *) xevent)->window,
                    WINDOW));
            status = XEV_FILTER_CONTINUE;
        case KeyPress:
            if (gone
                || (xevent->xkey.keycode ==
                    params.keys[KEY_CYCLE_WINDOWS].keycode))
            {
                /* Hide frame draw */
                clientDrawOutline (passdata->c);

                /* If KEY_CYCLE_WINDOWS has Shift, then do not reverse */
                if (!(params.keys[KEY_CYCLE_WINDOWS].modifier & ShiftMask)
                        && xevent->xkey.state & ShiftMask) {
                    passdata->c =
                        clientGetPrevious (passdata->c, passdata->cycle_range);
                }
                else
                {
                    passdata->c =
                        clientGetNext (passdata->c, passdata->cycle_range);
                }

                if (passdata->c)
                {
                    /* Redraw frame draw */
                    clientDrawOutline (passdata->c);
                    tabwinSetLabel (passdata->tabwin, passdata->c->name);
                }
                else
                {
                    cycling = FALSE;
                }
            }
            break;
        case KeyRelease:
            {
                int keysym = XKeycodeToKeysym (dpy, xevent->xkey.keycode, 0);

                /* If KEY_CYCE_WINDOWS has Shift, then stop cycling on Shift
                 * release.
                 */
                if (IsModifierKey (keysym)
                        && ( (params.keys[KEY_CYCLE_WINDOWS].modifier
                             & ShiftMask)
                        || (keysym != XK_Shift_L && keysym != XK_Shift_R) ) )
                {
                    cycling = FALSE;
                }
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

    if (!cycling)
    {
        TRACE ("event loop now finished");
        gtk_main_quit ();
    }

    return status;
}

void
clientCycle (Client * c, XEvent * e)
{
    ClientCycleData passdata;
    int g1, g2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientCycle");

    last_timestamp = stashEventTime (last_timestamp, e);
    g1 = XGrabKeyboard (dpy, gnome_win, FALSE, GrabModeAsync, GrabModeAsync,
        last_timestamp);
    g2 = XGrabPointer (dpy, gnome_win, FALSE, NoEventMask, GrabModeAsync,
        GrabModeAsync, None, None, last_timestamp);
    if ((g1 != GrabSuccess) || (g2 != GrabSuccess))
    {
        TRACE ("grab failed in clientCycle");
        gdk_beep ();
        if (g1 == GrabSuccess)
        {
            XUngrabKeyboard (dpy, CurrentTime);
        }
        if (g2 == GrabSuccess)
        {
            XUngrabPointer (dpy, CurrentTime);
        }
        return;
    }

    MyXGrabServer ();
    if (params.cycle_minimum)
    {
        passdata.cycle_range = INCLUDE_HIDDEN;
    }
    else
    {
        passdata.cycle_range = INCLUDE_HIDDEN | INCLUDE_SKIP_TASKBAR | INCLUDE_SKIP_PAGER;
    }
    passdata.c = clientGetNext (c, passdata.cycle_range);
    if (passdata.c)
    {
        passdata.tabwin = tabwinCreate (passdata.c->name);
        TRACE ("entering cycle loop");
        /* Draw frame draw */
        clientDrawOutline (passdata.c);
        pushEventFilter (clientCycle_event_filter, &passdata);
        gtk_main ();
        popEventFilter ();
        if (passdata.c)
        {
            /* Hide frame draw */
            clientDrawOutline (passdata.c);
        }
        TRACE ("leaving cycle loop");
        tabwinDestroy (passdata.tabwin);
        g_free (passdata.tabwin);
    }
    MyXUngrabServer ();
    XUngrabKeyboard (dpy, last_timestamp);
    XUngrabPointer (dpy, last_timestamp);

    if (passdata.c)
    {
        clientShow (passdata.c, TRUE);
        clientRaise (passdata.c);
        clientSetFocus (passdata.c, FOCUS_SORT);
        clientPassGrabButton1 (passdata.c);
    }
}

static GtkToXEventFilterStatus
clientButtonPress_event_filter (XEvent * xevent, gpointer data)
{
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    gboolean pressed = TRUE;
    Client *c = ((ButtonPressData *) data)->c;
    int b = ((ButtonPressData *) data)->b;

    if (xevent->type == EnterNotify)
    {
        c->button_pressed[b] = TRUE;
        frameDraw (c, FALSE, FALSE);
    }
    else if (xevent->type == LeaveNotify)
    {
        c->button_pressed[b] = FALSE;
        frameDraw (c, FALSE, FALSE);
    }
    else if (xevent->type == ButtonRelease)
    {
        pressed = FALSE;
    }
    else if ((xevent->type == UnmapNotify)
        && (xevent->xunmap.window == c->window))
    {
        pressed = FALSE;
        c->button_pressed[b] = FALSE;
    }
    else if ((xevent->type == KeyPress) || (xevent->type == KeyRelease))
    {
    }
    else
    {
        status = XEV_FILTER_CONTINUE;
    }

    if (!pressed)
    {
        TRACE ("event loop now finished");
        gtk_main_quit ();
    }

    return status;
}

void
clientButtonPress (Client * c, Window w, XButtonEvent * bev)
{
    int b, g1;
    ButtonPressData passdata;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientButtonPress");

    for (b = 0; b < BUTTON_COUNT; b++)
    {
        if (MYWINDOW_XWINDOW (c->buttons[b]) == w)
        {
            break;
        }
    }

    g1 = XGrabPointer (dpy, w, FALSE,
        ButtonReleaseMask | EnterWindowMask | LeaveWindowMask, GrabModeAsync,
        GrabModeAsync, None, None, CurrentTime);

    if (g1 != GrabSuccess)
    {
        TRACE ("grab failed in clientButtonPress");
        gdk_beep ();
        if (g1 == GrabSuccess)
        {
            XUngrabKeyboard (dpy, CurrentTime);
        }
        return;
    }

    passdata.c = c;
    passdata.b = b;

    c->button_pressed[b] = TRUE;
    frameDraw (c, FALSE, FALSE);

    TRACE ("entering button press loop");
    pushEventFilter (clientButtonPress_event_filter, &passdata);
    gtk_main ();
    popEventFilter ();
    TRACE ("leaving button press loop");

    XUngrabPointer (dpy, CurrentTime);

    if (c->button_pressed[b])
    {
        c->button_pressed[b] = FALSE;
        switch (b)
        {
            case HIDE_BUTTON:
                if (CLIENT_CAN_HIDE_WINDOW (c))
                {
                    clientHide (c, c->win_workspace, TRUE);
                }
                break;
            case CLOSE_BUTTON:
                clientClose (c);
                break;
            case MAXIMIZE_BUTTON:
                if (CLIENT_CAN_MAXIMIZE_WINDOW (c))
                {
                    unsigned long mode = c->win_state & WIN_STATE_MAXIMIZED;

                    if (bev->button == Button1)
                    {
                        clientToggleMaximized (c,
                            mode ? mode : WIN_STATE_MAXIMIZED);
                    }
                    else if (bev->button == Button2)
                    {
                        clientToggleMaximized (c,
                            mode ? mode : WIN_STATE_MAXIMIZED_VERT);
                    }
                    else if (bev->button == Button3)
                    {
                        clientToggleMaximized (c,
                            mode ? mode : WIN_STATE_MAXIMIZED_HORIZ);
                    }
                }
                break;
            case SHADE_BUTTON:
                clientToggleShaded (c);
                break;
            case STICK_BUTTON:
                clientToggleSticky (c, TRUE);
                break;
            default:
                break;
        }
        frameDraw (c, FALSE, FALSE);
    }
}

Client *
clientGetLeader (Client * c)
{
    Client *c2 = NULL;

    TRACE ("entering clientGetLeader");
    g_return_val_if_fail (c != NULL, NULL);

    if (c->group_leader != None)
    {
        c2 = clientGetFromWindow (c->group_leader, WINDOW);
    }
    else if (c->client_leader != None)
    {
        c2 = clientGetFromWindow (c->client_leader, WINDOW);
    }
    return c2;
}

GList *
clientGetStackList (void)
{
    GList *windows_stack_copy = NULL;
    if (windows_stack)
    {
        windows_stack_copy = g_list_copy (windows_stack);
    }
    return windows_stack_copy;
}

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
char *
clientGetStartupId (Client * c)
{
    TRACE ("entering clientStartupId");
    g_return_val_if_fail (c != NULL, NULL);

    if (c->startup_id)
    {
        return c->startup_id;
    }
    else
    {
        Client *c2 = NULL;

        c2 = clientGetLeader (c);
        if (c2)
        {
            return c2->startup_id;
        }
    }
    return NULL;
}
#endif
