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

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib.h>

#include "main.h"
#include "client.h"
#include "frame.h"
#include "hints.h"
#include "workspaces.h"
#include "settings.h"
#include "gtktoxevent.h"
#include "debug.h"

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
    (!(focus_hint) || \
    ((!(wmhints) || \
    ((wmhints) && !(wmhints->flags & InputHint)) || \
    ((wmhints) && (wmhints->flags & InputHint) && (wmhints->input)))))

#define START_ICONIC(c) \
    ((c->wmhints) && \
    (c->wmhints->initial_state == IconicState) && \
    !(c->transient_for))

/* You don't like that ? Me either, but, hell, it's the way glib lists are designed */
#define XWINDOW_TO_GPOINTER(w)  ((gpointer) (Window) (w))
#define GPOINTER_TO_XWINDOW(p)  ((Window) (p))

Client *clients = NULL;
unsigned int client_count = 0;

static Window *client_list = NULL;
static GSList *windows = NULL;
static GSList *windows_stack = NULL;
static Client *client_focus = NULL;

/* Forward decl */
static void clientWindowType(Client * c);

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

typedef struct _ButtonPressData ButtonPressData;
struct _ButtonPressData
{
    int b;
    Client *c;
};

void clientSetNetState(Client * c)
{
    int i;
    Atom data[12];

    g_return_if_fail(c != NULL);
    DBG("entering clientSetNetState\n");
    DBG("client \"%s\" (%#lx)\n", c->name, c->window);

    i = 0;
    if(c->shaded)
    {
        DBG("clientSetNetState : shaded\n");
        data[i++] = net_wm_state_shaded;
    }
    if(c->sticky)
    {
        DBG("clientSetNetState : sticky\n");
        data[i++] = net_wm_state_sticky;
    }
    if(c->state_modal)
    {
        DBG("clientSetNetState : modal\n");
        data[i++] = net_wm_state_modal;
    }
    if(c->skip_pager)
    {
        DBG("clientSetNetState : skip_pager\n");
        data[i++] = net_wm_state_skip_pager;
    }
    if(c->skip_taskbar)
    {
        DBG("clientSetNetState : skip_taskbar\n");
        data[i++] = net_wm_state_skip_taskbar;
    }
    if(c->win_state & WIN_STATE_MAXIMIZED)
    {
        DBG("clientSetNetState : maximize vert + horiz\n");
        data[i++] = net_wm_state_maximized_horz;
        data[i++] = net_wm_state_maximized_vert;
    }
    else if(c->win_state & WIN_STATE_MAXIMIZED_HORIZ)
    {
        DBG("clientSetNetState : maximize horiz\n");
        data[i++] = net_wm_state_maximized_horz;
    }
    else if(c->win_state & WIN_STATE_MAXIMIZED_VERT)
    {
        DBG("clientSetNetState : vert\n");
        data[i++] = net_wm_state_maximized_vert;
    }
    if(c->fullscreen)
    {
        DBG("clientSetNetState : fullscreen\n");
        data[i++] = net_wm_state_fullscreen;
    }
    if((c->shaded) || (c->hidden))
    {
        DBG("clientSetNetState : hidden\n");
        data[i++] = net_wm_state_hidden;
    }
    XChangeProperty(dpy, c->window, net_wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char *)data, i);
    setNetHint(dpy, c->window, net_wm_desktop, (unsigned long)c->win_workspace);
}

static void clientGetNetState(Client * c)
{
    int n_atoms = 0;
    Atom *atoms = NULL;

    g_return_if_fail(c != NULL);
    DBG("entering clientGetNetState\n");
    DBG("client \"%s\" (%#lx)\n", c->name, c->window);

    if(get_atom_list(dpy, c->window, net_wm_state, &atoms, &n_atoms))
    {
        int i;
        DBG("clientGetNetState: %i atoms detected\n", n_atoms);

        i = 0;
        while(i < n_atoms)
        {
            if(atoms[i] == net_wm_state_shaded)
            {
                DBG("clientGetNetState : shaded\n");
                c->win_state |= WIN_STATE_SHADED;
                c->shaded = True;
            }
            if(atoms[i] == net_wm_state_sticky)
            {
                DBG("clientGetNetState : sticky\n");
                c->win_state |= WIN_STATE_STICKY;
                c->sticky = True;
            }
            else if(atoms[i] == net_wm_state_maximized_horz)
            {
                DBG("clientGetNetState : maximized horiz\n");
                c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
                c->maximized = True;
            }
            else if(atoms[i] == net_wm_state_maximized_vert)
            {
                DBG("clientGetNetState : maximized vert\n");
                c->win_state |= WIN_STATE_MAXIMIZED_VERT;
                c->maximized = True;
            }
            else if(atoms[i] == net_wm_state_fullscreen)
            {
                DBG("clientGetNetState : fullscreen\n");
                c->fullscreen = True;
            }
            else if(atoms[i] == net_wm_state_modal)
            {
                DBG("clientGetNetState : modal\n");
                c->state_modal = True;
            }
            else if(atoms[i] == net_wm_state_skip_pager)
            {
                DBG("clientGetNetState : skip_pager\n");
                c->skip_pager = True;
            }
            else if(atoms[i] == net_wm_state_skip_taskbar)
            {
                DBG("clientGetNetState : skip_taskbar\n");
                c->skip_taskbar = True;
            }
            else
            {
                g_message("Unmanaged net_wm_state\n");
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
    DBG("entering clientUpdateNetState\n");
    DBG("client \"%s\" (%#lx)\n", c->name, c->window);

    action = ((XEvent *) ev)->xclient.data.l[0];
    first = ((XEvent *) ev)->xclient.data.l[1];
    second = ((XEvent *) ev)->xclient.data.l[2];

    if((first == net_wm_state_shaded) || (second == net_wm_state_shaded))
    {
        if(((action == NET_WM_STATE_ADD) && !(c->shaded)) || ((action == NET_WM_STATE_REMOVE) && (c->shaded)) || (action == NET_WM_STATE_TOGGLE))
        {
            clientToggleShaded(c);
        }
    }

    if((first == net_wm_state_sticky) || (second == net_wm_state_sticky))
    {
        if(((action == NET_WM_STATE_ADD) && !(c->sticky)) || ((action == NET_WM_STATE_REMOVE) && (c->sticky)) || (action == NET_WM_STATE_TOGGLE))
        {
            clientToggleSticky(c);
        }
    }

    if((first == net_wm_state_maximized_horz) || (second == net_wm_state_maximized_horz) || (first == net_wm_state_maximized_vert) || (second == net_wm_state_maximized_vert))
    {
        if((action == NET_WM_STATE_ADD) && !(c->maximized))
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
        else if((action == NET_WM_STATE_REMOVE) && (c->maximized))
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
        if((action == NET_WM_STATE_ADD) && !(c->state_modal))
        {
            c->state_modal = True;
            clientSetNetState(c);
        }
        else if((action == NET_WM_STATE_REMOVE) && (c->state_modal))
        {
            c->state_modal = False;
            clientSetNetState(c);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            c->state_modal = ((c->state_modal) ? False : True);
            clientSetNetState(c);
        }
    }

    if((first == net_wm_state_fullscreen) || (second == net_wm_state_fullscreen))
    {
        if((action == NET_WM_STATE_ADD) && !(c->fullscreen))
        {
            c->fullscreen = True;
        }
        else if((action == NET_WM_STATE_REMOVE) && (c->fullscreen))
        {
            c->fullscreen = False;
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            c->fullscreen = ((c->fullscreen) ? False : True);
        }
        clientToggleFullscreen(c);
    }

    if((first == net_wm_state_skip_pager) || (second == net_wm_state_skip_pager))
    {
        if((action == NET_WM_STATE_ADD) && !(c->skip_pager))
        {
            c->skip_pager = True;
            clientSetNetState(c);
        }
        else if((action == NET_WM_STATE_REMOVE) && (c->skip_pager))
        {
            c->skip_pager = False;
            clientSetNetState(c);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            c->skip_pager = ((c->skip_pager) ? False : True);
            clientSetNetState(c);
        }
    }

    if((first == net_wm_state_skip_taskbar) || (second == net_wm_state_skip_taskbar))
    {
        if((action == NET_WM_STATE_ADD) && !(c->skip_taskbar))
        {
            c->skip_taskbar = True;
            clientSetNetState(c);
        }
        else if((action == NET_WM_STATE_REMOVE) && (c->skip_taskbar))
        {
            c->skip_taskbar = False;
            clientSetNetState(c);
        }
        else if(action == NET_WM_STATE_TOGGLE)
        {
            c->skip_taskbar = ((c->skip_taskbar) ? False : True);
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
    DBG("entering clientGetNetWmType\n");
    DBG("client \"%s\" (%#lx)\n", c->name, c->window);

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
            default:
                c->type_atom = net_wm_window_type_normal;
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
    long val = 0;

    g_return_if_fail(c != NULL);
    DBG("entering clientGetInitialNetWmDesktop\n");
    DBG("client \"%s\" (%#lx)\n", c->name, c->window);

    c->win_workspace = workspace;
    if(getNetHint(dpy, c->window, net_wm_desktop, &val))
    {
        DBG("atom net_wm_desktop detected\n");
        if(val == (int)0xFFFFFFFF)
        {
            DBG("atom net_wm_desktop specifies window \"%s\" is sticky\n", c->name);
            c->win_workspace = workspace;
            clientStick(c);
        }
        else
        {
            DBG("atom net_wm_desktop specifies window \"%s\" is on desk %i\n", c->name, (int)val);
            c->win_workspace = (int)val;
        }
    }
    else if(getGnomeHint(dpy, c->window, win_workspace, &val))
    {
        DBG("atom win_workspace specifies window \"%s\" is on desk %i\n", c->name, (int)val);
        c->win_workspace = (int)val;
    }
    DBG("initial desktop for window \"%s\" is %i\n", c->name, c->win_workspace);
    if(c->win_workspace > workspace_count - 1)
    {
        DBG("value off limits, using %i instead\n", workspace_count - 1);
        c->win_workspace = workspace_count - 1;
    }
    DBG("initial desktop for window \"%s\" is %i\n", c->name, c->win_workspace);
    setGnomeHint(dpy, c->window, win_workspace, c->win_workspace);
    setNetHint(dpy, c->window, net_wm_desktop, c->win_workspace);
}

static void clientSetNetClientList(Atom a, GSList * list)
{
    Window *listw;
    Window *index_dest;
    GSList *index_src;
    guint size, i;

    DBG("entering clientSetNetClientList\n");

    size = g_slist_length(list);
    if(size <= 0)
    {
        XDeleteProperty(dpy, root, a);
    }
    else if((listw = (Window *) malloc((size + 1) * sizeof(Window))))
    {
        DBG("%i windows in list for %i clients\n", size, client_count);
        for(i = 0, index_dest = listw, index_src = list; i < size; i++, index_dest++, index_src = g_slist_next(index_src))
        {
            *index_dest = GPOINTER_TO_XWINDOW(index_src->data);
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
    DBG("entering clientGetNetStruts for \"%s\" (%#lx)\n", c->name, c->window);

    for(i = 0; i < 4; i++)
    {
        c->struts[i] = 0;
    }
    c->has_struts = False;

    if(get_cardinal_list(dpy, c->window, net_wm_strut, &struts, &nitems))
    {
        if(nitems != 4)
        {
            XFree(struts);
            return;
        }

        c->has_struts = True;
        for(i = 0; i < 4; i++)
        {
            c->struts[i] = struts[i];
        }

        DBG("NET_WM_STRUT for window \"%s\"= (%d,%d,%d,%d)\n", c->name, c->struts[0], c->struts[1], c->struts[2], c->struts[3]);
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
    if(c->has_border)
    {
        atoms[i++] = net_wm_action_shade;
    }
    XChangeProperty(dpy, c->window, net_wm_allowed_actions, XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms, i);
}

static void clientWindowType(Client * c)
{
    WindowType old_type;
    int layer;

    g_return_if_fail(c != NULL);
    DBG("entering clientWindowType\n");
    DBG("type for client \"%s\" (%#lx)\n", c->name, c->window);

    old_type = c->type;
    layer = c->win_layer;
    if(c->type_atom != None)
    {
        if(c->type_atom == net_wm_window_type_desktop)
        {
            DBG("atom net_wm_window_type_desktop detected\n");
            c->type = WINDOW_DESKTOP;
            c->win_state |= WIN_STATE_STICKY;
            c->has_border = False;
            c->sticky = True;
            c->skip_pager = True;
            c->skip_taskbar = True;
            layer = WIN_LAYER_DESKTOP;
        }
        else if(c->type_atom == net_wm_window_type_dock)
        {
            DBG("atom net_wm_window_type_dock detected\n");
            c->type = WINDOW_DOCK;
            layer = WIN_LAYER_DOCK;
        }
        else if(c->type_atom == net_wm_window_type_toolbar)
        {
            DBG("atom net_wm_window_type_toolbar detected\n");
            c->type = WINDOW_TOOLBAR;
            layer = WIN_LAYER_NORMAL;
        }
        else if(c->type_atom == net_wm_window_type_menu)
        {
            DBG("atom net_wm_window_type_menu detected\n");
            c->type = WINDOW_MENU;
            layer = WIN_LAYER_NORMAL;
            /* The policy here is unclear :
               http://mail.gnome.org/archives/wm-spec-list/2002-May/msg00001.html
               As it seems, GNOME and KDE don't treat menu the same way...
               c->win_state |= WIN_STATE_STICKY;
               c->has_border = False;
               c->sticky = True;
             */
            c->skip_pager = True;
            c->skip_taskbar = True;
        }
        else if(c->type_atom == net_wm_window_type_dialog)
        {
            DBG("atom net_wm_window_type_dialog detected\n");
            c->type = WINDOW_DIALOG;
            layer = WIN_LAYER_ONTOP;
        }
        else if(c->type_atom == net_wm_window_type_normal)
        {
            DBG("atom net_wm_window_type_normal detected\n");
            c->type = WINDOW_NORMAL;
            layer = c->win_layer;
        }
        else if(c->type_atom == net_wm_window_type_utility)
        {
            DBG("atom net_wm_window_type_utility detected\n");
            c->type = WINDOW_UTILITY;
            c->has_border = False;
            layer = WIN_LAYER_NORMAL;
        }
        else if(c->type_atom == net_wm_window_type_splashscreen)
        {
            DBG("atom net_wm_window_type_splashscreen detected\n");
            c->type = WINDOW_SPLASHSCREEN;
            c->has_border = False;
            layer = WIN_LAYER_ABOVE_DOCK;
        }
    }
    else if(c->transient_for)
    {
        Client *c2;

        DBG("no \"net\" atom detected\n");

        c2 = clientGetFromWindow(c->transient_for, WINDOW);
        if(c2)
        {
            c->type = c2->type;
            layer = c2->win_layer;
        }
    }
    else
    {
        DBG("no \"net\" atom detected, not even a transient\n");
        c->type = UNSET;
        layer = c->win_layer;
    }
    if((old_type != c->type) || (layer != c->win_layer))
    {
        DBG("setting layer %i\n", layer);
        clientSetNetState(c);
        clientSetLayer(c, layer);
    }
}

void clientInstallColormaps(Client * c)
{
    XWindowAttributes attr;
    gboolean installed = False;
    int i;

    g_return_if_fail(c != NULL);
    DBG("entering clientInstallColormaps\n");

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
    DBG("entering clientUpdateColormaps\n");

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

void clientGrabKeys(Client * c)
{
    g_return_if_fail(c != NULL);
    DBG("entering clientGrabKeys\n");
    DBG("grabbing keys for client \"%s\" (%#lx)\n", c->name, c->window);

    grabKey(dpy, &keys[KEY_MOVE_UP], c->window);
    grabKey(dpy, &keys[KEY_MOVE_DOWN], c->window);
    grabKey(dpy, &keys[KEY_MOVE_LEFT], c->window);
    grabKey(dpy, &keys[KEY_MOVE_RIGHT], c->window);
    grabKey(dpy, &keys[KEY_RESIZE_UP], c->window);
    grabKey(dpy, &keys[KEY_RESIZE_DOWN], c->window);
    grabKey(dpy, &keys[KEY_RESIZE_LEFT], c->window);
    grabKey(dpy, &keys[KEY_RESIZE_RIGHT], c->window);
    grabKey(dpy, &keys[KEY_CLOSE_WINDOW], c->window);
    grabKey(dpy, &keys[KEY_HIDE_WINDOW], c->window);
    grabKey(dpy, &keys[KEY_MAXIMIZE_WINDOW], c->window);
    grabKey(dpy, &keys[KEY_MAXIMIZE_VERT], c->window);
    grabKey(dpy, &keys[KEY_MAXIMIZE_HORIZ], c->window);
    grabKey(dpy, &keys[KEY_SHADE_WINDOW], c->window);
    grabKey(dpy, &keys[KEY_CYCLE_WINDOWS], c->window);
    grabKey(dpy, &keys[KEY_LOWER_WINDOW_LAYER], c->window);
    grabKey(dpy, &keys[KEY_RAISE_WINDOW_LAYER], c->window);
    grabKey(dpy, &keys[KEY_NEXT_WORKSPACE], c->window);
    grabKey(dpy, &keys[KEY_PREV_WORKSPACE], c->window);
    grabKey(dpy, &keys[KEY_ADD_WORKSPACE], c->window);
    grabKey(dpy, &keys[KEY_DEL_WORKSPACE], c->window);
    grabKey(dpy, &keys[KEY_STICK_WINDOW], c->window);
    grabKey(dpy, &keys[KEY_WORKSPACE_1], c->window);
    grabKey(dpy, &keys[KEY_WORKSPACE_2], c->window);
    grabKey(dpy, &keys[KEY_WORKSPACE_3], c->window);
    grabKey(dpy, &keys[KEY_WORKSPACE_4], c->window);
    grabKey(dpy, &keys[KEY_WORKSPACE_5], c->window);
    grabKey(dpy, &keys[KEY_WORKSPACE_6], c->window);
    grabKey(dpy, &keys[KEY_WORKSPACE_7], c->window);
    grabKey(dpy, &keys[KEY_WORKSPACE_8], c->window);
    grabKey(dpy, &keys[KEY_WORKSPACE_9], c->window);
    grabKey(dpy, &keys[KEY_MOVE_NEXT_WORKSPACE], c->window);
    grabKey(dpy, &keys[KEY_MOVE_PREV_WORKSPACE], c->window);
    grabKey(dpy, &keys[KEY_MOVE_WORKSPACE_1], c->window);
    grabKey(dpy, &keys[KEY_MOVE_WORKSPACE_2], c->window);
    grabKey(dpy, &keys[KEY_MOVE_WORKSPACE_3], c->window);
    grabKey(dpy, &keys[KEY_MOVE_WORKSPACE_4], c->window);
    grabKey(dpy, &keys[KEY_MOVE_WORKSPACE_5], c->window);
    grabKey(dpy, &keys[KEY_MOVE_WORKSPACE_6], c->window);
    grabKey(dpy, &keys[KEY_MOVE_WORKSPACE_7], c->window);
    grabKey(dpy, &keys[KEY_MOVE_WORKSPACE_8], c->window);
    grabKey(dpy, &keys[KEY_MOVE_WORKSPACE_9], c->window);
}

void clientUngrabKeys(Client * c)
{
    g_return_if_fail(c != NULL);
    DBG("entering clientUngrabKeys\n");
    DBG("ungrabing keys for client \"%s\" (%#lx)\n", c->name, c->window);

    ungrabKeys(dpy, c->window);
}

void clientCoordGravitate(Client * c, int mode, int *x, int *y)
{
    int gravity, dx = 0, dy = 0;

    g_return_if_fail(c != NULL);
    DBG("entering clientCoordGravitate\n");

    gravity = c->size->flags & PWinGravity ? c->size->win_gravity : NorthWestGravity;
    switch (gravity)
    {
        case CenterGravity:
            dx = (c->border_width * 2) - ((frameLeft(c) + frameRight(c)) / 2);
            dy = (c->border_width * 2) - ((frameTop(c) + frameBottom(c)) / 2);
            break;
        case NorthGravity:
            dx = (c->border_width * 2) - ((frameLeft(c) + frameRight(c)) / 2);
            dy = frameTop(c);
            break;
        case SouthGravity:
            dx = (c->border_width * 2) - ((frameLeft(c) + frameRight(c)) / 2);
            dy = (c->border_width * 2) - frameBottom(c);
            break;
        case EastGravity:
            dx = (c->border_width * 2) - frameRight(c);
            dy = (c->border_width * 2) - ((frameTop(c) + frameBottom(c)) / 2);
            break;
        case WestGravity:
            dx = frameLeft(c);
            dy = (c->border_width * 2) - ((frameTop(c) + frameBottom(c)) / 2);
            break;
        case NorthWestGravity:
            dx = frameLeft(c);
            dy = frameTop(c);
            break;
        case NorthEastGravity:
            dx = (c->border_width * 2) - frameRight(c);
            dy = frameTop(c);
            break;
        case SouthWestGravity:
            dx = frameLeft(c);
            dy = (c->border_width * 2) - frameBottom(c);
            break;
        case SouthEastGravity:
            dx = (c->border_width * 2) - frameRight(c);
            dy = (c->border_width * 2) - frameBottom(c);
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
    DBG("entering clientGravitate\n");

    x = c->x;
    y = c->y;
    clientCoordGravitate(c, mode, &x, &y);
    c->x = x;
    c->y = y;
}

static void clientAddToList(Client * c)
{
    int i;
    Window *new;

    g_return_if_fail(c != NULL);
    DBG("entering clientAddToList\n");
    client_count++;
    new = malloc(sizeof(Window) * client_count);
    if(new)
    {
        for(i = 0; i < client_count - 1; i++)
        {
            new[i] = client_list[i];
        }
        new[i] = c->window;
        if(client_list)
        {
            free(client_list);
        }
        client_list = new;
        XChangeProperty(dpy, root, win_client_list, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)client_list, client_count);
    }
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
    if(!(c->skip_taskbar))
    {
        DBG("adding window \"%s\" (%#lx) to windows list\n", c->name, c->window);
        windows = g_slist_append(windows, XWINDOW_TO_GPOINTER(c->window));
        clientSetNetClientList(net_client_list, windows);
    }

    if(!(c->skip_pager))
    {
        DBG("adding window \"%s\" (%#lx) to windows_stack list\n", c->name, c->window);
        windows_stack = g_slist_append(windows_stack, XWINDOW_TO_GPOINTER(c->window));
        clientSetNetClientList(net_client_list_stacking, windows_stack);
    }
    c->managed = True;
}

static void clientRemoveFromList(Client * c)
{
    int i, j;
    Window *new;

    g_return_if_fail(c != NULL);
    DBG("entering clientRemoveFromList\n");
    g_assert(client_count > 0);

    c->managed = False;
    new = malloc(sizeof(Window) * client_count);
    if(new)
    {
        for(i = 0, j = 0; i < client_count; i++)
        {
            if(client_list[i] != c->window)
            {
                new[j++] = client_list[i];
            }
        }
        if(client_list)
        {
            free(client_list);
        }
        client_list = new;
        XChangeProperty(dpy, root, win_client_list, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)client_list, client_count - 1);
    }
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

    if(!(c->skip_taskbar))
    {
        DBG("removing window \"%s\" (%#lx) from windows list\n", c->name, c->window);
        windows = g_slist_remove(windows, XWINDOW_TO_GPOINTER(c->window));
        clientSetNetClientList(net_client_list, windows);
    }
    if(!(c->skip_pager))
    {
        DBG("removing window \"%s\" (%#lx) from windows_stack list\n", c->name, c->window);
        windows_stack = g_slist_remove(windows_stack, XWINDOW_TO_GPOINTER(c->window));
        clientSetNetClientList(net_client_list_stacking, windows_stack);
    }
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
    DBG("entering clientSetWidth\n");
    DBG("setting width %i for client \"%s\" (%#lx)\n", w1, c->name, c->window);

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
    DBG("entering clientSetHeight\n");
    DBG("setting height %i for client \"%s\" (%#lx)\n", h1, c->name, c->window);

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

static inline Client *clientGetTopMost(int layer, Client * exclude)
{
    Window w1, w2, *wins = NULL;
    unsigned int i, count;
    Client *top = NULL, *c;

    DBG("entering clientGetTopMost\n");

    XQueryTree(dpy, root, &w1, &w2, &wins, &count);
    DBG("XQueryTree returns %i child window(s)\n", count);
    for(i = 0; i < count; i++)
    {
        c = clientGetFromWindow(wins[i], FRAME);
        if(c)
        {
            DBG("*** stack window [%i]=(%lx) \"%s\", layer %i\n", i, c->window, c->name, c->win_layer);
            if(!exclude || (c != exclude))
            {
                if(c->win_layer <= layer)
                {
                    top = c;
                }
            }
        }
    }
    if(wins)
    {
        XFree(wins);
    }
    return top;
}

static inline Client *clientGetBottomMost(int layer, Client * exclude)
{
    Window w1, w2, *wins = NULL;
    unsigned int i, count;
    Client *c = NULL;

    DBG("entering clientGetBottomMost\n");

    XQueryTree(dpy, root, &w1, &w2, &wins, &count);
    DBG("XQueryTree returns %i child window(s)\n", count);
    for(i = 0; i < count; i++)
    {
        c = clientGetFromWindow(wins[i], FRAME);
        if(c)
        {
            DBG("*** stack window [%i]=(%lx) \"%s\", layer %i\n", i, c->window, c->name, c->win_layer);
            if(!exclude || (c != exclude))
            {
                if(c->win_layer >= layer)
                {
                    break;
                }
            }
        }
    }
    if(wins)
    {
        XFree(wins);
    }
    return c;
}

static inline void clientComputeStackList(Client * c, Client * sibling, XWindowChanges * wc, int mask)
{
    if((c->managed) && !(c->skip_pager) && (mask & CWStackMode))
    {
        if((sibling) && (sibling != c) && (g_slist_find(windows_stack, XWINDOW_TO_GPOINTER(sibling->window))))
        {
            gint position;

            if(wc->stack_mode == Below)
            {
                if(g_slist_index(windows_stack, XWINDOW_TO_GPOINTER(sibling->window)) > -1)
                {
                    DBG("Below with sibling -> inserting window \"%s\" (%#lx) at position %i in stack list\n", c->name, c->window, position);
                    windows_stack = g_slist_remove(windows_stack, XWINDOW_TO_GPOINTER(c->window));
                    position = g_slist_index(windows_stack, XWINDOW_TO_GPOINTER(sibling->window));
                    windows_stack = g_slist_insert(windows_stack, XWINDOW_TO_GPOINTER(c->window), position);
                }
            }
            else
            {
                if(g_slist_index(windows_stack, XWINDOW_TO_GPOINTER(sibling->window)) > -1)
                {
                    windows_stack = g_slist_remove(windows_stack, XWINDOW_TO_GPOINTER(c->window));
                    position = g_slist_index(windows_stack, XWINDOW_TO_GPOINTER(sibling->window));
                    DBG("Above with sibling -> inserting window \"%s\" (%#lx) at position %i in stack list\n", c->name, c->window, position + 1);
                    windows_stack = g_slist_insert(windows_stack, XWINDOW_TO_GPOINTER(c->window), position + 1);
                }
            }
        }
        else
        {
            if(wc->stack_mode == Below)
            {
                DBG("Below without sibling -> inserting window \"%s\" (%#lx) at beginning of stack list\n", c->name, c->window);
                windows_stack = g_slist_remove(windows_stack, XWINDOW_TO_GPOINTER(c->window));
                windows_stack = g_slist_prepend(windows_stack, XWINDOW_TO_GPOINTER(c->window));
            }
            else
            {
                DBG("Above without sibling -> inserting window \"%s\" (%#lx) at end of stack list\n", c->name, c->window);
                windows_stack = g_slist_remove(windows_stack, XWINDOW_TO_GPOINTER(c->window));
                windows_stack = g_slist_append(windows_stack, XWINDOW_TO_GPOINTER(c->window));
            }
        }
    }
}

static void _clientConfigure(Client * c, XWindowChanges * wc, int mask)
{
    gboolean transients = FALSE;
    XConfigureEvent ce;
    Client *sibling = NULL;
    Client *c2 = NULL;
    Client *lowest_transient = NULL;
    int i;

    g_return_if_fail(c != NULL);
    g_return_if_fail(c->window != None);
    DBG("entering _clientConfigure (recursive)\n");
    DBG("configuring (recursive) client \"%s\" (%#lx), layer %i\n", c->name, c->window, c->win_layer);

    if(mask & CWX)
    {
        c->x = wc->x;
    }
    if(mask & CWY)
    {
        c->y = wc->y;
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
            case Above:
                DBG("Above\n");
                if(mask & CWSibling)
                {
                    DBG("Sibling specified for \"%s\" (%#lx) is (%#lx)\n", c->name, c->window, wc->sibling);
                    sibling = clientGetFromWindow(wc->sibling, WINDOW);
                    if(!sibling)
                    {
                        DBG("Sibling specified for \"%s\" (%#lx) cannot be found\n", c->name, c->window);
                        sibling = clientGetTopMost(c->win_layer, c);
                    }
                }
                else
                {
                    DBG("No sibling specified for \"%s\" (%#lx)\n", c->name, c->window);
                    sibling = clientGetTopMost(c->win_layer, c);
                }
                if(!sibling)
                {
                    DBG("unable to determine sibling!\n");
                    wc->stack_mode = Below;
                }
                DBG("looking for transients\n");
                for(c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
                {
                    DBG("checking \"%s\" (%#lx)\n", c2->name, c2->window);
                    if((c2->transient_for == c->window) && (c2 != c))
                    {
                        XWindowChanges wc2;
                        DBG("transient \"%s\" (%#lx) found for \"%s\" (%#lx)\n", c2->name, c2->window, c->name, c->window);
                        if(!transients)
                        {
                            transients = TRUE;
                            wc2.stack_mode = Above;
                            DBG("recursive call 1\n");
                            _clientConfigure(c2, &wc2, CWStackMode);
                        }
                        else
                        {
                            wc2.sibling = lowest_transient->window;
                            wc2.stack_mode = Below;
                            DBG("recursive call 2\n");
                            _clientConfigure(c2, &wc2, CWStackMode | CWSibling);
                        }
                        lowest_transient = c2;
                    }
                }
                if(transients && lowest_transient)
                {
                    DBG("Transient is %s (%#lx)\n", sibling->name, sibling->window);
                    sibling = lowest_transient;
                    wc->stack_mode = Below;
                }
                break;
            case Below:
                DBG("Below\n");
                if((mask & CWSibling) && (c->transient_for != wc->sibling))
                {
                    DBG("Sibling specified for \"%s\" (%#lx) is (%#lx)\n", c->name, c->window, wc->sibling);
                    sibling = clientGetFromWindow(wc->sibling, WINDOW);
                    if(!sibling)
                    {
                        DBG("Sibling specified for \"%s\" (%#lx) cannot be found\n", c->name, c->window);
                        sibling = clientGetBottomMost(c->win_layer, c);
                    }
                }
                else if(c->transient_for)
                {
                    wc->sibling = c->transient_for;
                    wc->stack_mode = Above;
                    mask |= (CWSibling | CWStackMode);
                    sibling = clientGetFromWindow(wc->sibling, WINDOW);
                    DBG("lowering transient \"%s\" (%#lx) for \"%s\" (%#lx)\n", c->name, c->window, c->transient_for, sibling->name);
                }
                else
                {
                    DBG("No sibling specified for \"%s\" (%#lx)\n", c->name, c->window);
                    sibling = clientGetBottomMost(c->win_layer, c);
                }
                if(!sibling)
                {
                    DBG("unable to determine sibling!\n");
                    wc->stack_mode = Above;
                    sibling = clientGetTopMost(c->win_layer, c);
                }
                break;
        }
        if(sibling)
        {
            if(sibling != c)
            {
                wc->sibling = sibling->frame;
                mask |= CWSibling;
            }
            else
            {
                mask &= ~(CWSibling | CWStackMode);
            }
        }
        else
        {
            mask &= ~CWSibling;
        }
        clientComputeStackList(c, sibling, wc, mask);
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
    mask &= ~(CWStackMode | CWSibling);
    XConfigureWindow(dpy, c->window, mask, wc);

    if(mask & (CWWidth | CWHeight))
    {
        frameDraw(c);
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

void clientConfigure(Client * c, XWindowChanges * wc, int mask)
{
    DBG("entering clientConfigure\n");
    DBG("configuring client \"%s\" (%#lx)\n", c->name, c->window);
    _clientConfigure(c, wc, mask);
    if((c->managed) && (mask & CWStackMode))
    {
        clientSetNetClientList(net_client_list_stacking, windows_stack);
    }
}

static inline void clientConstraintPos(Client * c)
{
    g_return_if_fail(c != NULL);
    DBG("entering clientConstraintPos\n");
    DBG("client \"%s\" (%#lx)\n", c->name, c->window);
    if((c->x + c->width) < CLIENT_MIN_VISIBLE + (int)margins[MARGIN_LEFT])
    {
        c->x = CLIENT_MIN_VISIBLE + (int)margins[MARGIN_LEFT] - c->width;
    }
    else if((c->x + CLIENT_MIN_VISIBLE) > XDisplayWidth(dpy, screen) - (int)margins[MARGIN_RIGHT])
    {
        c->x = XDisplayWidth(dpy, screen) - (int)margins[MARGIN_RIGHT] - CLIENT_MIN_VISIBLE;
    }
    if((c->y + c->height) < CLIENT_MIN_VISIBLE + (int)margins[MARGIN_TOP])
    {
        c->y = CLIENT_MIN_VISIBLE + (int)margins[MARGIN_TOP] - c->height;
    }
    else if((c->y + CLIENT_MIN_VISIBLE) > XDisplayHeight(dpy, screen) - (int)margins[MARGIN_BOTTOM])
    {
        c->y = XDisplayHeight(dpy, screen) - (int)margins[MARGIN_BOTTOM] - CLIENT_MIN_VISIBLE;
    }
}

/* Compute rectangle overlap area */
static unsigned long overlap(int x0, int y0, int x1, int y1, int tx0, int ty0, int tx1, int ty1)
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
    int xmax, ymax, best_x, best_y, i;
    unsigned long best_overlaps = 0;
    gboolean first = TRUE;

    g_return_if_fail(c != NULL);
    DBG("entering clientInitPosition\n");

    clientGravitate(c, APPLY);

    if(c->size->flags & (PPosition | USPosition))
    {
        if((c->type != WINDOW_DOCK) && (c->type != WINDOW_DESKTOP))
        {
            if((c->x + c->width) > XDisplayWidth(dpy, screen) - (int)margins[MARGIN_RIGHT])
            {
                c->x = XDisplayWidth(dpy, screen) - (int)margins[MARGIN_RIGHT] - c->width;
            }
            if(c->x < (int)margins[MARGIN_LEFT])
            {
                c->x = (int)margins[MARGIN_LEFT];
            }
            if((c->y + c->height) > XDisplayHeight(dpy, screen) - (int)margins[MARGIN_BOTTOM])
            {
                c->y = XDisplayHeight(dpy, screen) - (int)margins[MARGIN_BOTTOM] - c->height;
            }
            if(c->y < (int)margins[MARGIN_TOP])
            {
                c->y = (int)margins[MARGIN_TOP];
            }
        }
        return;
    }

    xmax = XDisplayWidth(dpy, screen) - frameWidth(c) - margins[MARGIN_RIGHT];
    ymax = XDisplayHeight(dpy, screen) - frameHeight(c) - margins[MARGIN_BOTTOM];
    best_x = frameLeft(c) + margins[MARGIN_LEFT];
    best_y = frameTop(c) + margins[MARGIN_TOP];

    for(test_y = frameTop(c) + margins[MARGIN_TOP]; test_y < ymax; test_y += 8)
    {
        for(test_x = frameLeft(c) + margins[MARGIN_LEFT]; test_x < xmax; test_x += 8)
        {
            unsigned long count_overlaps = 0;
            DBG("analyzing %i clients\n", client_count);
            for(c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
            {
                if((c2 != c) && (c->win_workspace == c2->win_workspace) && (c2->visible))
                {
                    c->x = test_x;
                    c->y = test_y;
                    count_overlaps += overlap(frameX(c), frameY(c), frameX(c) + frameWidth(c), frameY(c) + frameHeight(c), frameX(c2), frameY(c2), frameX(c2) + frameWidth(c2), frameY(c2) + frameHeight(c2));
                }
            }
            DBG("overlaps so far is %u\n", count_overlaps);
            if(count_overlaps == 0)
            {
                DBG("overlaps is 0 so it's the best we can get\n");
                return;
            }
            else if((count_overlaps < best_overlaps) || (first))
            {
                DBG("overlaps %u is better than the best we have %u\n", count_overlaps, best_overlaps);
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

void clientFrame(Window w)
{
    XWindowAttributes attr;
    XWindowChanges wc;
    PropMwmHints *mwm_hints;
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
    DBG("entering clientFrame\n");
    DBG("framing client (%#lx)\n", w);

    if(w == gnome_win)
    {
        DBG("Not managing our own window\n");
        return;
    }

    if(!XGetWindowAttributes(dpy, w, &attr))
    {
        DBG("Cannot get window attributes\n");
        return;
    }

    if(attr.override_redirect)
    {
        DBG("Not managing override_redirect windows\n");
        return;
    }

    if(!(c = malloc(sizeof(Client))))
    {
        DBG("Cannot allocate memory for the window structure\n");
        return;
    }

    c->window = w;
    getWindowName(dpy, c->window, &c->name);
    DBG("name \"%s\"\n", c->name);
    getTransientFor(dpy, c->window, &(c->transient_for));
    c->size = XAllocSizeHints();
    XGetWMNormalHints(dpy, w, c->size, &dummy);
    wm_protocols_flags = getWMProtocols(dpy, c->window);
    c->x = attr.x;
    c->y = attr.y;
    c->width = attr.width;
    c->height = attr.height;
    c->border_width = attr.border_width;
    c->cmap = attr.colormap;

    for(i = 0; i < BUTTON_COUNT; i++)
    {
        c->button_pressed[i] = False;
    }

    if(!XGetWMColormapWindows(dpy, c->window, &c->cmap_windows, &c->ncmap))
    {
        c->ncmap = 0;
    }

    c->wmhints = XGetWMHints(dpy, c->window);

    /* Initialize structure */
    c->focus = False;
    c->fullscreen = False;
    c->has_border = True;
    c->has_struts = False;
    c->hidden = (START_ICONIC(c) ? True : False);
    c->ignore_unmap = ((attr.map_state == IsViewable) ? 1 : 0);
    c->managed = False;
    c->maximized = False;
    c->shaded = False;
    c->skip_pager = False;
    c->skip_taskbar = False;
    c->state_modal = False;
    c->sticky = False;
    c->type = UNSET;
    c->type_atom = None;
    c->visible = False;
    c->wm_delete = ((wm_protocols_flags & WM_PROTOCOLS_DELETE_WINDOW) ? True : False);
    c->wm_input = (ACCEPT_INPUT(c->wmhints) ? True : False);
    c->wm_takefocus = ((wm_protocols_flags & WM_PROTOCOLS_TAKE_FOCUS) ? True : False);

    mwm_hints = getMotifHints(dpy, c->window);
    if(mwm_hints)
    {
        if(mwm_hints->flags & MWM_HINTS_DECORATIONS && !(mwm_hints->decorations & MWM_DECOR_ALL))
        {
            c->has_border = ((mwm_hints->decorations & (MWM_DECOR_TITLE |  MWM_DECOR_BORDER)) ? True : False);
        }
        XFree(mwm_hints);
    }

    getGnomeHint(dpy, w, win_hints, &c->win_hints);
    getGnomeHint(dpy, w, win_state, &c->win_state);
    if(!getGnomeHint(dpy, w, win_layer, &c->win_layer))
    {
        c->win_layer = WIN_LAYER_NORMAL;
    }
    c->sticky = ((c->win_state & WIN_STATE_STICKY) ? True : False);
    c->shaded = ((c->win_state & WIN_STATE_SHADED) ? True : False);
    c->maximized = ((c->win_state & (WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED_HORIZ | WIN_STATE_MAXIMIZED)) ? True : False);

    /* Beware, order of calls is important here ! */
    clientGetNetState(c);
    clientGetInitialNetWmDesktop(c);
    clientGetNetWmType(c);
    clientGetNetStruts(c);

    /* Once we know the type of window, we can initialize window position */
    if(attr.map_state != IsViewable)
    {
        clientInitPosition(c);
    }
    else
    {
        clientGravitate(c, APPLY);
    }

    MyXGrabServer();
    if(XGetGeometry(dpy, w, &dummy_root, &dummy_x, &dummy_y, &dummy_width, &dummy_height, &dummy_bw, &dummy_depth) == 0)
    {
        if(c->name)
        {
            free(c->name);
        }
        if(c->size)
        {
            XFree(c->size);
        }
        free(c);
        MyXUngrabServer();
        return;
    }
    valuemask = CWEventMask;
    attributes.event_mask = (FRAME_EVENT_MASK | POINTER_EVENT_MASK);
    c->frame = XCreateWindow(dpy, root, frameX(c), frameY(c), frameWidth(c), frameHeight(c), 0, CopyFromParent, InputOutput, CopyFromParent, valuemask, &attributes);
    DBG("frame id (%#lx)\n", c->frame);
    valuemask = CWEventMask;
    attributes.event_mask = (CLIENT_EVENT_MASK);
    XChangeWindowAttributes(dpy, c->window, valuemask, &attributes);
    if(shape)
    {
        XShapeSelectInput(dpy, c->window, ShapeNotifyMask);
    }
    XSetWindowBorderWidth(dpy, c->window, 0);
    XReparentWindow(dpy, c->window, c->frame, frameLeft(c), frameTop(c));

    clientSetNetActions(c);
    clientAddToList(c);
    clientGrabKeys(c);
    XGrabButton(dpy, AnyButton, AnyModifier, c->window, False, POINTER_EVENT_MASK, GrabModeSync, GrabModeAsync, None, None);
    MyXUngrabServer();

    c->sides[SIDE_LEFT] = XCreateSimpleWindow(dpy, c->frame, 0, 0, 1, 1, 0, 0, 0);
    c->sides[SIDE_RIGHT] = XCreateSimpleWindow(dpy, c->frame, 0, 0, 1, 1, 0, 0, 0);
    c->sides[SIDE_BOTTOM] = XCreateSimpleWindow(dpy, c->frame, 0, 0, 1, 1, 0, 0, 0);
    c->corners[CORNER_BOTTOM_LEFT] = XCreateSimpleWindow(dpy, c->frame, 0, 0, 1, 1, 0, 0, 0);
    c->corners[CORNER_BOTTOM_RIGHT] = XCreateSimpleWindow(dpy, c->frame, 0, 0, 1, 1, 0, 0, 0);
    c->corners[CORNER_TOP_LEFT] = XCreateSimpleWindow(dpy, c->frame, 0, 0, 1, 1, 0, 0, 0);
    c->corners[CORNER_TOP_RIGHT] = XCreateSimpleWindow(dpy, c->frame, 0, 0, 1, 1, 0, 0, 0);
    c->title = XCreateSimpleWindow(dpy, c->frame, 0, 0, 1, 1, 0, 0, 0);

    for(i = 0; i < 4; i++)
    {
        XDefineCursor(dpy, c->corners[i], resize_cursor[i]);
    }
    for(i = 0; i < 3; i++)
    {
        XDefineCursor(dpy, c->sides[i], resize_cursor[4 + i]);
    }
    for(i = 0; i < BUTTON_COUNT; i++)
    {
        c->buttons[i] = XCreateSimpleWindow(dpy, c->frame, 0, 0, 1, 1, 0, 0, 0);
    }

    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    wc.stack_mode = Above;
    clientConfigure(c, &wc, CWX | CWY | CWHeight | CWWidth | CWStackMode);

    if(!(c->hidden))
    {
        clientShow(c, True);
        if(focus_new && clientAcceptFocus(c))
        {
            clientSetFocus(c, True);
        }
        else
        {
            frameDraw(c);
        }
    }
    else
    {
        frameDraw(c);
        setWMState(dpy, c->window, IconicState);
        clientSetNetState(c);
    }

    DBG("client_count=%d\n", client_count);
}

void clientUnframe(Client * c, int remap)
{
    DBG("entering clientUnframe\n");
    DBG("unframing client \"%s\" (%#lx)\n", c->name, c->window);

    g_return_if_fail(c != NULL);
    if(client_focus == c)
    {
        client_focus = NULL;
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
    XReparentWindow(dpy, c->window, root, c->x, c->y);
    XDestroyWindow(dpy, c->frame);
    clientRemoveFromList(c);
    if(c->has_struts)
    {
        workspaceUpdateArea(margins, gnome_margins);
    }
    if(c->name)
    {
        free(c->name);
    }
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
    free(c);
    DBG("client_count=%d\n", client_count);
}

void clientFrameAll()
{
    unsigned int count, i;
    Window w1, w2, *wins = NULL;
    XWindowAttributes attr;

    DBG("entering clientFrameAll\n");

    /* Since this fn is called at startup, it's safe to initialize some vars here */
    client_count = 0;
    clients = NULL;
    client_list = NULL;
    client_list = NULL;
    windows = NULL;
    windows_stack = NULL;
    client_focus = NULL;

    XSync (dpy, 0);
    MyXGrabServer();
    XQueryTree(dpy, root, &w1, &w2, &wins, &count);
    for(i = 0; i < count; i++)
    {
        XGetWindowAttributes(dpy, wins[i], &attr);
        if((!(attr.override_redirect)) && (attr.map_state == IsViewable))
        {
            clientFrame(wins[i]);
        }
    }
    MyXUngrabServer();
    if(wins)
    {
        XFree(wins);
    }
}

void clientUnframeAll()
{
    Client *c;
    unsigned int count, i;
    Window w1, w2, *wins = NULL;

    DBG("entering clientUnframeAll\n");

    MyXGrabServer();
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
    DBG("entering clientGetFromWindow\n");
    DBG("looking for (%#lx)\n", w);

    for(c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        switch (mode)
        {
            case WINDOW:
                if(c->window == w)
                {
                    DBG("found \"%s\" (mode WINDOW)\n", c->name);
                    return (c);
                }
                break;
            case FRAME:
                if(c->frame == w)
                {
                    DBG("found \"%s\" (mode FRAME)\n", c->name);
                    return (c);
                }
                break;
            case ANY:
            default:
                if((c->frame == w) || (c->window == w))
                {
                    DBG("found \"%s\" (mode ANY)\n", c->name);
                    return (c);
                }
                break;
        }
    }
    DBG("no client found\n");

    return NULL;
}

void clientShow(Client * c, int change_state)
{
    int i;
    Client *c2;
    int ws = workspace;

    g_return_if_fail(c != NULL);
    DBG("entering clientShow\n");
    DBG("showing client \"%s\" (%#lx)\n", c->name, c->window);

    /* This is to make sure that transient are shown with their "master" window */
    if ((c->transient_for) && (c2 = clientGetFromWindow(c->transient_for, WINDOW)) && (c2 != c))
    {
        ws = c2->win_workspace;
        if (c->win_workspace != ws)
        {
            clientSetWorkspace (c, ws, FALSE);
        }
    }

    if((c->win_workspace == ws) || (c->sticky))
    {
        for(c2 = c->next, i = 0; i < client_count; c2 = c2->next, i++)
        {
            if((c2->transient_for == c->window) && (!c2->hidden) && (c2 != c))
            {
                clientShow(c2, change_state);
            }
        }
        XMapWindow(dpy, c->window);
        XMapWindow(dpy, c->frame);
        c->visible = True;
    }
    if(change_state)
    {
        c->hidden = False;
        setWMState(dpy, c->window, NormalState);
        workspaceUpdateArea(margins, gnome_margins);
    }
    clientSetNetState(c);
}

void clientHide(Client * c, int change_state)
{
    int i;
    Client *c2;

    g_return_if_fail(c != NULL);
    DBG("entering clientHide\n");
    DBG("hiding client \"%s\" (%#lx)\n", c->name, c->window);

    XUnmapWindow(dpy, c->window);
    XUnmapWindow(dpy, c->frame);
    for(c2 = c->next, i = 0; i < client_count; c2 = c2->next, i++)
    {
        if((c2->transient_for == c->window) && (c2 != c))
        {
            clientHide(c2, change_state);
        }
    }

    c->visible = False;
    if(change_state)
    {
        c->hidden = True;
        setWMState(dpy, c->window, IconicState);
        workspaceUpdateArea(margins, gnome_margins);
    }
    c->ignore_unmap++;
    clientSetNetState(c);
}

void clientHideAll(Client * c)
{
    int i;
    Client *c2;

    DBG("entering clientHideAll\n");

    for(c2 = c->next, i = 0; i < client_count; c2 = c2->next, i++)
    {
        if((c2->type == WINDOW_NORMAL) && (c2->visible) && (c2->has_border) && !(c2->skip_taskbar) && !(c2->transient_for) && (c2 != c))
        {
            if((c) && (c->transient_for != c2->window))
            {
                clientHide(c2, True);
            }
            else if(!c)
            {
                clientHide(c2, True);
            }
        }
    }
}

void clientClose(Client * c)
{
    g_return_if_fail(c != NULL);
    DBG("entering clientClose\n");
    DBG("closing client \"%s\" (%#lx)\n", c->name, c->window);

    if(c->wm_delete)
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
    DBG("entering clientKill\n");
    DBG("killing client \"%s\" (%#lx)\n", c->name, c->window);

    XKillClient(dpy, c->window);
}

void clientRaise(Client * c)
{
    XWindowChanges wc;

    g_return_if_fail(c != NULL);
    DBG("entering clientRaise\n");
    DBG("raising client \"%s\" (%#lx)\n", c->name, c->window);

    if((c->managed) && (c->type != WINDOW_DESKTOP))
    {
        wc.stack_mode = Above;
        clientConfigure(c, &wc, CWStackMode);
    }
}

void clientLower(Client * c)
{
    XWindowChanges wc;

    g_return_if_fail(c != NULL);
    DBG("entering clientLower\n");
    DBG("lowering client \"%s\" (%#lx)\n", c->name, c->window);

    if(c->managed)
    {
        wc.stack_mode = Below;
        clientConfigure(c, &wc, CWStackMode);
    }
}

void clientSetLayer(Client * c, int l)
{
    int old_layer;

    g_return_if_fail(c != NULL);
    DBG("entering clientSetLayer\n");
    DBG("setting client \"%s\" (%#lx) layer to %d\n", c->name, c->window, l);

    old_layer = c->win_layer;
    if(l == old_layer)
    {
        DBG("client \"%s\" (%#lx) already at layer %d\n", c->name, c->window, l);
        return;
    }

    setGnomeHint(dpy, c->window, win_layer, l);
    c->win_layer = l;
    if(l > old_layer)
    {
        clientRaise(c);
    }
    else
    {
        clientLower(c);
    }
}

void clientSetWorkspace(Client * c, int ws, gboolean manage_mapping)
{
    Client *c2;
    int i;
    
    g_return_if_fail(c != NULL);
    DBG("entering clientSetWorkspace\n");
    DBG("setting client \"%s\" (%#lx) to workspace %d\n", c->name, c->window, ws);

    if(c->win_workspace == ws)
    {
        return;
    }
    
    for(c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
    {
        if ((c2->transient_for == c->window) && (c2 != c))
        {
            clientSetWorkspace(c2, ws, manage_mapping);
        }
    }
    
    setGnomeHint(dpy, c->window, win_workspace, ws);
    c->win_workspace = ws;
    setNetHint(dpy, c->window, net_wm_desktop, (unsigned long)c->win_workspace);
    
    if(manage_mapping && !(c->hidden))
    {
        if(c->sticky)
        {
            clientShow(c, False);
        }
        else
        {
            if(ws == workspace)
            {
                clientShow(c, False);
            }
            else
            {
                clientHide(c, False);
            }
        }
    }
}

void clientToggleShaded(Client * c)
{
    XWindowChanges wc;

    g_return_if_fail(c != NULL);
    DBG("entering clientToggleShaded\n");
    DBG("shading/unshading client \"%s\" (%#lx)\n", c->name, c->window);

    if(!(c->shaded) && (!(c->has_border) || (c->fullscreen)))
    {
        DBG("cowardly refusing to shade \"%s\" (%#lx) because it has no border\n", c->name, c->window);
        return;
    }
    c->win_state = c->win_state ^ WIN_STATE_SHADED;
    c->shaded = ((c->win_state & WIN_STATE_SHADED) ? True : False);
    setGnomeHint(dpy, c->window, win_state, c->win_state);
    wc.width = c->width;
    wc.height = c->height;
    clientSetNetState(c);
    clientConfigure(c, &wc, CWWidth | CWHeight);
}

void clientStick(Client * c)
{
    g_return_if_fail(c != NULL);
    DBG("entering clientStick\n");
    DBG("sticking client \"%s\" (%#lx)\n", c->name, c->window);

    c->win_state |= WIN_STATE_STICKY;
    c->sticky = True;
    setGnomeHint(dpy, c->window, win_state, c->win_state);
    clientSetWorkspace(c, workspace, TRUE);
    clientSetNetState(c);
}

void clientUnstick(Client * c)
{
    g_return_if_fail(c != NULL);
    DBG("entering clientUnstick\n");
    DBG("unsticking client \"%s\" (%#lx)\n", c->name, c->window);

    c->win_state &= ~WIN_STATE_STICKY;
    c->sticky = False;
    setGnomeHint(dpy, c->window, win_state, c->win_state);
    clientSetWorkspace(c, workspace, TRUE);
    clientSetNetState(c);
}

void clientToggleSticky(Client * c)
{
    g_return_if_fail(c != NULL);
    DBG("entering clientToggleSticky\n");
    DBG("sticking/unsticking client \"%s\" (%#lx)\n", c->name, c->window);

    if(c->win_state & WIN_STATE_STICKY)
    {
        clientUnstick(c);
    }
    else
    {
        clientStick(c);
    }
}

inline void clientRemoveMaximizeFlag(Client * c)
{
    g_return_if_fail(c != NULL);
    DBG("entering clientRemoveMaximizeFlag\n");
    DBG("Removing maximize flag on client \"%s\" (%#lx)\n", c->name, c->window);
    
    c->win_state &= ~(WIN_STATE_MAXIMIZED | WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED_HORIZ);
    c->maximized = False;
    setGnomeHint(dpy, c->window, win_state, c->win_state);
    clientSetNetState(c);
}

void clientToggleMaximized(Client * c, int mode)
{
    XWindowChanges wc;

    g_return_if_fail(c != NULL);
    DBG("entering clientToggleMaximized\n");
    DBG("maximzing/unmaximizing client \"%s\" (%#lx)\n", c->name, c->window);

    if((c->size->flags & (PMinSize | PMaxSize)) && (c->size->min_width == c->size->max_width) && (c->size->min_height == c->size->max_height))
    {
        return;
    }

    if(c->win_state & WIN_STATE_MAXIMIZED)
    {
        wc.width = c->old_width;
        wc.height = c->old_height;
        wc.x = c->old_x;
        wc.y = c->old_y;
        c->win_state &= ~WIN_STATE_MAXIMIZED;
    }
    else
    {
        c->old_x = c->x;
        c->old_y = c->y;
        c->old_width = c->width;
        c->old_height = c->height;

        if(mode != WIN_STATE_MAXIMIZED_VERT)
        {
            wc.x = frameLeft(c) + margins[MARGIN_LEFT];
            wc.width = XDisplayWidth(dpy, screen) - frameLeft(c) - frameRight(c) - margins[MARGIN_LEFT] - margins[MARGIN_RIGHT];
            c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
        }
        else
        {
            wc.x = c->x;
            wc.width = c->width;
        }
        if(mode != WIN_STATE_MAXIMIZED_HORIZ)
        {
            wc.y = frameTop(c) + margins[MARGIN_TOP];
            wc.height = XDisplayHeight(dpy, screen) - frameTop(c) - frameBottom(c) - margins[MARGIN_TOP] - margins[MARGIN_BOTTOM];
            c->win_state |= WIN_STATE_MAXIMIZED_VERT;
        }
        else
        {
            wc.y = c->y;
            wc.height = c->height;
        }
    }
    c->maximized = ((c->win_state & (WIN_STATE_MAXIMIZED_VERT | WIN_STATE_MAXIMIZED_HORIZ | WIN_STATE_MAXIMIZED)) ? True : False);
    setGnomeHint(dpy, c->window, win_state, c->win_state);
    clientSetNetState(c);
    clientConfigure(c, &wc, CWX | CWY | CWWidth | CWHeight);
}

void clientToggleFullscreen(Client * c)
{
    XWindowChanges wc;

    g_return_if_fail(c != NULL);
    DBG("entering clientToggleFullscreen\n");
    DBG("toggle fullscreen client \"%s\" (%#lx)\n", c->name, c->window);

    if(c->fullscreen)
    {
        c->fullscreen_old_x = c->x;
        c->fullscreen_old_y = c->y;
        c->fullscreen_old_width = c->width;
        c->fullscreen_old_height = c->height;

        wc.x = margins[MARGIN_LEFT];
        wc.y = margins[MARGIN_TOP];
        wc.width = XDisplayWidth(dpy, screen) - margins[MARGIN_LEFT] - margins[MARGIN_RIGHT];
        wc.height = XDisplayHeight(dpy, screen) - margins[MARGIN_TOP] - margins[MARGIN_BOTTOM];
    }
    else
    {
        wc.x = c->fullscreen_old_x;
        wc.y = c->fullscreen_old_y;
        wc.width = c->fullscreen_old_width;
        wc.height = c->fullscreen_old_height;
    }
    clientSetNetState(c);
    clientConfigure(c, &wc, CWX | CWY | CWWidth | CWHeight);
}

void clientUpdateFocus(Client * c)
{
    Client *c2 = ((client_focus != c) ? client_focus : NULL);
    unsigned long data[2];

    DBG("entering clientUpdateFocus\n");

    if((c) && !clientAcceptFocus(c))
    {
        DBG("SKIP_FOCUS set for client \"%s\" (%#lx)\n", c->name, c->window);
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
        DBG("redrawing previous focus client \"%s\" (%#lx)\n", c2->name, c2->window);
        frameDraw(c2);
    }
    data[1] = None;
    XChangeProperty(dpy, root, net_active_window, XA_WINDOW, 32, PropModeReplace, (unsigned char *)data, 2);
}

inline gboolean clientAcceptFocus(Client * c)
{
    g_return_val_if_fail(c != NULL, FALSE);

    DBG("entering clientAcceptFocus\n");

    /* First check GNOME protocol */
    if(c->win_hints & WIN_HINTS_SKIP_FOCUS)
    {
        return FALSE;
    }
    /* then try ICCCM */
    else if(c->wm_takefocus)
    {
        return TRUE;
    }
    /* At last, use wmhints */
    return (c->wm_input);
}

void clientSetFocus(Client * c, int sort)
{
    Client *tmp;
    Client *c2 = ((client_focus != c) ? client_focus : NULL);
    unsigned long data[2];

    DBG("entering clientSetFocus\n");

    if((c) && (c->visible))
    {
        DBG("setting focus to client \"%s\" (%#lx)\n", c->name, c->window);
        if(!clientAcceptFocus(c))
        {
            DBG("SKIP_FOCUS set for client \"%s\" (%#lx)\n", c->name, c->window);
            return;
        }
        client_focus = c;
        clientInstallColormaps(c);
        if(sort)
        {
            DBG("Sorting...\n");
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
        DBG("setting focus to none\n");
        client_focus = NULL;
        XSetInputFocus(dpy, gnome_win, RevertToNone, CurrentTime);
        data[0] = None;
    }
    if(c2)
    {
        DBG("redrawing previous focus client \"%s\" (%#lx)\n", c2->name, c2->window);
        frameDraw(c2);
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
    DBG("entering clientDrawOutline\n");

    XDrawRectangle(dpy, root, box_gc, frameX(c), frameY(c), frameWidth(c) - 1, frameHeight(c) - 1);
    if((c->has_border) && !(c->fullscreen) && !(c->shaded))
    {
        XDrawRectangle(dpy, root, box_gc, c->x, c->y, c->width - 1, c->height - 1);
    }
}

static GtkToXEventFilterStatus clientMove_event_filter(XEvent * xevent, gpointer data)
{
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    MoveResizeData *passdata = (MoveResizeData *) data;
    Client *c = passdata->c;
    gboolean moving = TRUE;
    XWindowChanges wc;
    XEvent ev;

    DBG("entering clientMove_event_filter\n");

    if(xevent->type == KeyPress)
    {
        if(!passdata->grab && box_move)
        {
            MyXGrabServer();
            passdata->grab = TRUE;
            clientDrawOutline(c);
        }
        if(box_move)
        {
            clientDrawOutline(c);
        }
        if(!(c->win_state & WIN_STATE_MAXIMIZED_HORIZ))
        {
            if(xevent->xkey.keycode == keys[KEY_MOVE_LEFT].keycode)
            {
                c->x = c->x - 16;
            }
            if(xevent->xkey.keycode == keys[KEY_MOVE_RIGHT].keycode)
            {
                c->x = c->x + 16;
            }
        }
        if(!(c->win_state & WIN_STATE_MAXIMIZED_VERT))
        {
            if(xevent->xkey.keycode == keys[KEY_MOVE_UP].keycode)
            {
                c->y = c->y - 16;
            }
            if(xevent->xkey.keycode == keys[KEY_MOVE_DOWN].keycode)
            {
                c->y = c->y + 16;
            }
        }
        if((c->type != WINDOW_DOCK) && (c->type != WINDOW_DESKTOP))
        {
            clientConstraintPos(c);
        }
        if(box_move)
        {
            clientDrawOutline(c);
        }
        else
        {
            wc.x = c->x;
            wc.y = c->y;
            clientConfigure(c, &wc, CWX | CWY);
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
        while(XCheckTypedEvent(dpy, MotionNotify, &ev));

        if(!passdata->grab && box_move)
        {
            MyXGrabServer();
            passdata->grab = TRUE;
            clientDrawOutline(c);
        }
        if(box_move)
        {
            clientDrawOutline(c);
        }

        if((workspace_count > 1) && !(c->transient_for))
        {
            int msx, msy;

            getMouseXY(root, &msx, &msy);
            if(msx == 0 && !((workspace == 0) && !wrap_workspaces))
            {
                XEvent e;
                workspaceSwitch(workspace - 1, c);
                XWarpPointer(dpy, None, root, 0, 0, 0, 0, XDisplayWidth(dpy, screen) - 11, msy);
                xevent->xmotion.x_root = XDisplayWidth(dpy, screen) - 11;
                while(XCheckTypedEvent(dpy, MotionNotify, &e));
                XRaiseWindow (dpy, passdata->tmp_event_window);
            }
            else if((msx == XDisplayWidth(dpy, screen) - 1) && !((workspace == workspace_count - 1) && !wrap_workspaces))
            {
                XEvent e;
                workspaceSwitch(workspace + 1, c);
                XWarpPointer(dpy, None, root, 0, 0, 0, 0, 10, msy);
                xevent->xmotion.x_root = 10;
                while(XCheckTypedEvent(dpy, MotionNotify, &e));
                XRaiseWindow (dpy, passdata->tmp_event_window);
            }
        }

        if(!(c->win_state & WIN_STATE_MAXIMIZED_HORIZ))
        {
            c->x = passdata->ox + (xevent->xmotion.x_root - passdata->mx);
            if(snap_to_border)
            {
                if(abs(frameX(c) - margins[MARGIN_LEFT]) < snap_width)
                {
                    c->x = frameLeft(c) + margins[MARGIN_LEFT];
                }
                if(abs(frameX(c) - XDisplayWidth(dpy, screen) + frameWidth(c) + margins[MARGIN_RIGHT]) < snap_width)
                {
                    c->x = (XDisplayWidth(dpy, screen) - frameRight(c) - c->width - margins[MARGIN_RIGHT]);
                }
            }
        }

        if(!(c->win_state & WIN_STATE_MAXIMIZED_VERT))
        {
            c->y = passdata->oy + (xevent->xmotion.y_root - passdata->my);
            if(snap_to_border)
            {
                if(abs(frameY(c) - margins[MARGIN_TOP]) < snap_width)
                {
                    c->y = frameTop(c) + margins[MARGIN_TOP];
                }
                if(abs(frameY(c) - XDisplayHeight(dpy, screen) + frameHeight(c) + margins[MARGIN_BOTTOM]) < snap_width)
                {
                    c->y = (XDisplayHeight(dpy, screen) - margins[MARGIN_BOTTOM] - frameHeight(c) + frameTop(c));
                }
            }
        }
        if((c->type != WINDOW_DOCK) && (c->type != WINDOW_DESKTOP))
        {
            clientConstraintPos(c);
        }
        if(box_move)
        {
            clientDrawOutline(c);
        }
        else
        {
            wc.x = c->x;
            wc.y = c->y;
            clientConfigure(c, &wc, CWX | CWY);
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
    DBG("leaving clientMove_event_filter\n");

    if(!moving)
    {
        DBG("event loop now finished\n");
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
    DBG("entering clientDoMove\n");
    DBG("moving client \"%s\" (%#lx)\n", c->name, c->window);

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
    
    passdata.tmp_event_window = setTmpEventWin(ButtonMotionMask| ButtonReleaseMask);

    if(e->type == KeyPress)
    {
        passdata.use_keys = True;
        timestamp = e->xkey.time;
        passdata.mx = e->xkey.x_root;
        passdata.my = e->xkey.y_root;
        g1 = XGrabKeyboard(dpy, passdata.tmp_event_window, False, GrabModeAsync, GrabModeAsync, timestamp);
    }
    else if (e->type == ButtonPress)
    {
        timestamp = e->xbutton.time;
        passdata.mx = e->xbutton.x_root;
        passdata.my = e->xbutton.y_root;
    }
    else
    {
        timestamp = CurrentTime;
        getMouseXY(root, &passdata.mx, &passdata.my);
    }
    g2 = XGrabPointer(dpy, passdata.tmp_event_window, False, ButtonMotionMask| ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, move_cursor, timestamp);

    if(((passdata.use_keys) && (g1 != GrabSuccess)) || (g2 != GrabSuccess))
    {
        DBG("grab failed in clientMove\n");
        gdk_beep();
        if((passdata.use_keys) && (g1 == GrabSuccess))
        {
            XUngrabKeyboard(dpy, timestamp);
        }
        if(g2 == GrabSuccess)
        {
            XUngrabPointer(dpy, timestamp);
        }
        removeTmpEventWin (passdata.tmp_event_window);
        return;
    }

    if(passdata.use_keys)
    {
        XPutBackEvent(dpy, e);
    }

    DBG("entering move loop\n");
    pushEventFilter(clientMove_event_filter, &passdata);
    gtk_main();
    popEventFilter();
    DBG("leaving move loop\n");

    if(passdata.use_keys)
    {
        XUngrabKeyboard(dpy, CurrentTime);
    }
    XUngrabPointer(dpy, CurrentTime);
    removeTmpEventWin (passdata.tmp_event_window);

    if(passdata.grab && box_move)
    {
        clientDrawOutline(c);
    }
    wc.x = c->x;
    wc.y = c->y;
    clientConfigure(c, &wc, CWX | CWY);

    if(passdata.grab && box_move)
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

    DBG("entering clientResize_event_filter\n");

    if(xevent->type == KeyPress)
    {
        if(!passdata->grab && box_resize)
        {
            MyXGrabServer();
            passdata->grab = TRUE;
            clientDrawOutline(c);
        }
        if(box_resize)
        {
            clientDrawOutline(c);
        }
        if(!(c->win_state & WIN_STATE_MAXIMIZED_VERT))
        {
            if(xevent->xkey.keycode == keys[KEY_MOVE_UP].keycode)
            {
                c->height = c->height - (clientGetHeightInc(c) < 10 ? 10 : clientGetHeightInc(c));
            }
            if(xevent->xkey.keycode == keys[KEY_MOVE_DOWN].keycode)
            {
                c->height = c->height + (clientGetHeightInc(c) < 10 ? 10 : clientGetHeightInc(c));
            }
        }
        if(!(c->win_state & WIN_STATE_MAXIMIZED_HORIZ))
        {
            if(xevent->xkey.keycode == keys[KEY_MOVE_LEFT].keycode)
            {
                c->width = c->width - (clientGetWidthInc(c) < 10 ? 10 : clientGetWidthInc(c));
            }
            if(xevent->xkey.keycode == keys[KEY_MOVE_RIGHT].keycode)
            {
                c->width = c->width + (clientGetWidthInc(c) < 10 ? 10 : clientGetWidthInc(c));
            }
        }
        if(box_resize)
        {
            clientDrawOutline(c);
        }
        else
        {
            wc.x = c->x;
            wc.y = c->y;
            wc.width = c->width;
            wc.height = c->height;
            clientConfigure(c, &wc, CWX | CWY | CWWidth | CWHeight);
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
        while(XCheckTypedEvent(dpy, MotionNotify, xevent));

        if(!passdata->grab && box_resize)
        {
            MyXGrabServer();
            passdata->grab = TRUE;
            clientDrawOutline(c);
        }
        if(box_resize)
        {
            clientDrawOutline(c);
        }
        passdata->oldw = c->width;
        passdata->oldh = c->height;
        if(!(c->win_state & WIN_STATE_MAXIMIZED_HORIZ))
        {
            if((passdata->corner == CORNER_TOP_LEFT) || (passdata->corner == CORNER_BOTTOM_LEFT) || (passdata->corner == 4 + SIDE_LEFT))
            {
                c->width = (c->x + c->width) - xevent->xmotion.x_root + passdata->mx - frameLeft(c);
            }
            if((passdata->corner == CORNER_BOTTOM_RIGHT) || (passdata->corner == CORNER_TOP_RIGHT) || (passdata->corner == 4 + SIDE_RIGHT))
            {
                c->width = (xevent->xmotion.x_root - c->x) + passdata->mx - frameRight(c);
            }
        }
        if(!((c->win_state & WIN_STATE_MAXIMIZED_VERT) || (c->win_state & WIN_STATE_SHADED)))
        {
            if((passdata->corner == CORNER_TOP_LEFT) || (passdata->corner == CORNER_TOP_RIGHT))
            {
                c->height = (c->y + c->height) - xevent->xmotion.y_root + passdata->my - frameTop(c);
            }
            if((passdata->corner == CORNER_BOTTOM_RIGHT) || (passdata->corner == CORNER_BOTTOM_LEFT) || (passdata->corner == 4 + SIDE_BOTTOM))
            {
                c->height = (xevent->xmotion.y_root - c->y) + passdata->my - frameBottom(c);
            }
        }
        clientSetWidth(c, c->width);
        clientSetHeight(c, c->height);
        if(!(c->win_state & WIN_STATE_MAXIMIZED_HORIZ) && ((passdata->corner == CORNER_TOP_LEFT) || (passdata->corner == CORNER_BOTTOM_LEFT) || (passdata->corner == 4 + SIDE_LEFT)))
        {
            c->x = c->x - (c->width - passdata->oldw);
        }
        if(!((c->win_state & WIN_STATE_MAXIMIZED_VERT) || (c->win_state & WIN_STATE_SHADED)) && (passdata->corner == CORNER_TOP_LEFT || passdata->corner == CORNER_TOP_RIGHT))
        {
            c->y = c->y - (c->height - passdata->oldh);
        }
        if(box_resize)
        {
            clientDrawOutline(c);
        }
        else
        {
            wc.x = c->x;
            wc.y = c->y;
            wc.width = c->width;
            wc.height = c->height;
            clientConfigure(c, &wc, CWX | CWY | CWWidth | CWHeight);
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
    DBG("leaving clientResize_event_filter\n");

    if(!resizing)
    {
        DBG("event loop now finished\n");
        gtk_main_quit();
    }

    return status;
}

void clientResize(Client * c, int corner, XEvent *e)
{
    XWindowChanges wc;
    Time timestamp;
    MoveResizeData passdata;
    int g1 = GrabSuccess, g2 = GrabSuccess;

    g_return_if_fail(c != NULL);
    DBG("entering clientResize\n");
    DBG("resizing client \"%s\" (%#lx)\n", c->name, c->window);

    passdata.c = c;
    passdata.corner = CORNER_BOTTOM_RIGHT;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    passdata.corner = corner;
    passdata.tmp_event_window = setTmpEventWin(ButtonMotionMask| ButtonReleaseMask);

    if(c->maximized)
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
    else if (e->type == ButtonPress)
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
    g2 = XGrabPointer(dpy, passdata.tmp_event_window, False, ButtonMotionMask| ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, resize_cursor[passdata.corner], timestamp);

    if(((passdata.use_keys) && (g1 != GrabSuccess)) || (g2 != GrabSuccess))
    {
        DBG("grab failed in clientResize\n");
        gdk_beep();
        if((passdata.use_keys) && (g1 == GrabSuccess))
        {
            XUngrabKeyboard(dpy, timestamp);
        }
        if(g2 == GrabSuccess)
        {
            XUngrabPointer(dpy, timestamp);
        }
        removeTmpEventWin (passdata.tmp_event_window);
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
    DBG("entering resize loop\n");
    pushEventFilter(clientResize_event_filter, &passdata);
    gtk_main();
    popEventFilter();
    DBG("leaving resize loop\n");

    if(passdata.use_keys)
    {
        XUngrabKeyboard(dpy, CurrentTime);
    }
    XUngrabPointer(dpy, CurrentTime);
    removeTmpEventWin (passdata.tmp_event_window);

    if(passdata.grab && box_resize)
    {
        clientDrawOutline(c);
    }

    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    clientConfigure(c, &wc, (CWX | CWY | CWHeight | CWWidth));

    if(passdata.grab && box_resize)
    {
        MyXUngrabServer();
    }
}

Client *clientGetNext(Client * c, int mask)
{
    Client *c2;
    unsigned int i, okay;

    DBG("entering clientGetNext\n");

    if(c)
    {
        for(c2 = c->next, i = 0; i < client_count; c2 = c2->next, i++)
        {
            okay = True;
            if((!clientAcceptFocus(c2)) && !(mask & INCLUDE_SKIP_FOCUS))
            {
                okay = False;
            }
            if((c2->hidden) && !(mask & INCLUDE_HIDDEN))
            {
                okay = False;
            }
            if((c2->skip_pager) && !(mask & INCLUDE_SKIP_PAGER))
            {
                okay = False;
            }
            if((c2->skip_taskbar) && !(mask & INCLUDE_SKIP_TASKBAR))
            {
                okay = False;
            }
            if((c2->win_workspace != workspace) && !(mask & INCLUDE_ALL_WORKSPACES))
            {
                okay = False;
            }
            if(okay)
            {
                return c2;
            }
        }
    }
    return NULL;
}

static GtkToXEventFilterStatus clientCycle_event_filter(XEvent * xevent, gpointer data)
{
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    gboolean cycling = TRUE;
    Client **c2 = (Client **) data;

    DBG("entering clientCycle_event_filter\n");
    switch (xevent->type)
    {
        case KeyPress:
            if(xevent->xkey.keycode == keys[KEY_CYCLE_WINDOWS].keycode)
            {
                if((*c2)->hidden)
                {
                    clientHide(*c2, False);
                }
                *c2 = clientGetNext(*c2, INCLUDE_HIDDEN);
                if(*c2)
                {
                    clientShow(*c2, False);
                    clientRaise(*c2);
                    clientSetFocus(*c2, False);
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
        DBG("event loop now finished\n");
        gtk_main_quit();
    }

    return status;
}

void clientCycle(Client * c)
{
    Client *c2;
    int g1, g2;

    g_return_if_fail(c != NULL);
    DBG("entering clientCycle\n");

    g1 = XGrabKeyboard(dpy, gnome_win, False, GrabModeAsync, GrabModeAsync, CurrentTime);
    g2 = XGrabPointer(dpy, gnome_win, False, NoEventMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    if((g1 != GrabSuccess) || (g2 != GrabSuccess))
    {
        DBG("grab failed in clientCycle\n");
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

    c2 = clientGetNext(c, INCLUDE_HIDDEN);
    if(c2)
    {
        clientShow(c2, False);
        clientRaise(c2);
        clientSetFocus(c2, False);
        DBG("entering cycle loop\n");
        pushEventFilter(clientCycle_event_filter, &c2);
        gtk_main();
        popEventFilter();
        DBG("leaving cycle loop\n");
    }

    XUngrabKeyboard(dpy, CurrentTime);
    XUngrabPointer(dpy, CurrentTime);

    if(c2)
    {
        clientShow(c2, True);
        clientSetFocus(c2, True);
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
        frameDraw(c);
    }
    else if(xevent->type == LeaveNotify)
    {
        c->button_pressed[b] = False;
        frameDraw(c);
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
    {
    }
    else
    {
        status = XEV_FILTER_CONTINUE;
    }

    if(!pressed)
    {
        DBG("event loop now finished\n");
        gtk_main_quit();
    }

    return status;
}

void clientButtonPress(Client * c, Window w, XButtonEvent * bev)
{
    int b, g1;
    ButtonPressData passdata;

    g_return_if_fail(c != NULL);
    DBG("entering clientButtonPress\n");

    for(b = 0; b < BUTTON_COUNT; b++)
    {
        if(c->buttons[b] == w)
        {
            break;
        }
    }

    g1 = XGrabPointer(dpy, w, False, ButtonReleaseMask | EnterWindowMask | LeaveWindowMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

    if(g1 != GrabSuccess)
    {
        DBG("grab failed in clientButtonPress\n");
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
    frameDraw(c);

    DBG("entering button press loop\n");
    pushEventFilter(clientButtonPress_event_filter, &passdata);
    gtk_main();
    popEventFilter();
    DBG("leaving button press loop\n");

    XUngrabPointer(dpy, CurrentTime);

    if(c->button_pressed[b])
    {
        c->button_pressed[b] = False;
        frameDraw(c);
        switch (b)
        {
            case HIDE_BUTTON:
                clientHide(c, True);
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
                break;
            case SHADE_BUTTON:
                clientToggleShaded(c);
                break;
            case STICK_BUTTON:
                clientToggleSticky(c);
                break;
        }
    }
}
