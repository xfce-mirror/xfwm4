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

#ifndef __CLIENT_H__
#define __CLIENT_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "misc.h"
#include "hints.h"
#include "keyboard.h"
#include "pixmap.h"
#include "gtktoxevent.h"

#define ANY				0
#define WINDOW				1
#define FRAME				2

#define APPLY				1
#define REMOVE				-1

#define PLACEMENT_MOUSE			0
#define PLACEMENT_ROOT			1

#define INCLUDE_HIDDEN			(1<<0)
#define INCLUDE_SKIP_FOCUS		(1<<1)
#define INCLUDE_ALL_WORKSPACES		(1<<2)
#define INCLUDE_SKIP_PAGER		(1<<3)
#define INCLUDE_SKIP_TASKBAR		(1<<4)

#define ACTIVE				0
#define INACTIVE			1
#define PRESSED				2

#define MARGIN_LEFT			0
#define MARGIN_RIGHT			1
#define MARGIN_TOP			2
#define MARGIN_BOTTOM			3

#define CORNER_TOP_LEFT			0
#define CORNER_TOP_RIGHT		1
#define CORNER_BOTTOM_LEFT		2
#define CORNER_BOTTOM_RIGHT		3

#define SIDE_LEFT			0
#define SIDE_RIGHT			1
#define SIDE_BOTTOM			2

#define TITLE_1				0
#define TITLE_2				1
#define TITLE_3				2
#define TITLE_4				3
#define TITLE_5				4

#define HIDE_BUTTON			0
#define SHADE_BUTTON			1
#define MAXIMIZE_BUTTON			2
#define CLOSE_BUTTON			3
#define STICK_BUTTON			4
#define MENU_BUTTON			5
#define BUTTON_COUNT			6

#define KEY_MOVE_UP			0
#define KEY_MOVE_DOWN			1
#define KEY_MOVE_LEFT			2
#define KEY_MOVE_RIGHT			3
#define KEY_RESIZE_UP			4
#define KEY_RESIZE_DOWN			5
#define KEY_RESIZE_LEFT			6
#define KEY_RESIZE_RIGHT		7
#define KEY_CYCLE_WINDOWS		8
#define KEY_CLOSE_WINDOW		9
#define KEY_HIDE_WINDOW			10
#define KEY_MAXIMIZE_WINDOW		11
#define KEY_MAXIMIZE_VERT		12
#define KEY_MAXIMIZE_HORIZ		13
#define KEY_SHADE_WINDOW		14
#define KEY_RAISE_WINDOW_LAYER		15
#define KEY_LOWER_WINDOW_LAYER		16
#define KEY_NEXT_WORKSPACE		17
#define KEY_PREV_WORKSPACE		18
#define KEY_ADD_WORKSPACE		19
#define KEY_DEL_WORKSPACE		20
#define KEY_STICK_WINDOW		21
#define KEY_WORKSPACE_1		        22
#define KEY_WORKSPACE_2 	        23
#define KEY_WORKSPACE_3        		24
#define KEY_WORKSPACE_4        		25
#define KEY_WORKSPACE_5        		26
#define KEY_WORKSPACE_6        		27
#define KEY_WORKSPACE_7        		28
#define KEY_WORKSPACE_8        		29
#define KEY_WORKSPACE_9        		30
#define KEY_MOVE_NEXT_WORKSPACE		31
#define KEY_MOVE_PREV_WORKSPACE		32
#define KEY_MOVE_WORKSPACE_1		33
#define KEY_MOVE_WORKSPACE_2		34
#define KEY_MOVE_WORKSPACE_3		35
#define KEY_MOVE_WORKSPACE_4		36
#define KEY_MOVE_WORKSPACE_5		37
#define KEY_MOVE_WORKSPACE_6        	38
#define KEY_MOVE_WORKSPACE_7        	39
#define KEY_MOVE_WORKSPACE_8        	40
#define KEY_MOVE_WORKSPACE_9        	41
#define KEY_COUNT			42

#define ALIGN_LEFT			0
#define ALIGN_RIGHT			1
#define ALIGN_CENTER			2

#define ACTION_NONE			0
#define ACTION_MAXIMIZE			1
#define ACTION_SHADE			2
#define ACTION_HIDE			3

#define CLIENT_MIN_VISIBLE              5

typedef enum
{
    UNSET,
    WINDOW_NORMAL,
    WINDOW_DESKTOP,
    WINDOW_DOCK,
    WINDOW_DIALOG,
    WINDOW_MODAL_DIALOG,
    WINDOW_TOOLBAR,
    WINDOW_MENU,
    WINDOW_UTILITY,
    WINDOW_SPLASHSCREEN
}
WindowType;

#define CAN_HIDE_WINDOW(c)	((c->type == WINDOW_NORMAL) && \
				(c->visible) && \
				(c->has_border) && \
				!(c->skip_taskbar))

typedef struct _Client Client;

struct _Client
{
    Window window;
    Window frame;
    Window transient_for;
    Window *cmap_windows;
    Window title;
    Window sides[3];
    Window corners[4];
    Window buttons[BUTTON_COUNT];
    Colormap cmap;
    unsigned long win_hints;
    unsigned long win_state;
    unsigned long win_layer;
    int win_workspace;
    Atom type_atom;
    XSizeHints *size;
    XWMHints *wmhints;
    Client *next;
    Client *prev;
    WindowType type;
    int x;
    int y;
    int width;
    int height;
    int border_width;
    int old_x;
    int old_y;
    int old_width;
    int old_height;
    int fullscreen_old_x;
    int fullscreen_old_y;
    int fullscreen_old_width;
    int fullscreen_old_height;
    int fullscreen_old_layer;
    int ncmap;
    int button_pressed[BUTTON_COUNT];
    int struts[4];
    char *name;
    unsigned int focus:1;
    unsigned int fullscreen:1;
    unsigned int has_border:1;
    unsigned int has_struts:1;
    unsigned int hidden:1;
    unsigned int ignore_unmap;
    unsigned int managed:1;
    unsigned int maximized:1;
    unsigned int shaded:1;
    unsigned int skip_pager:1;
    unsigned int skip_taskbar:1;
    unsigned int state_modal:1;
    unsigned int sticky:1;
    unsigned int visible:1;
    unsigned int wm_delete:1;
    unsigned int wm_input:1;
    unsigned int wm_takefocus:1;
};

extern Client *clients;
extern Window *client_list;
extern unsigned int client_count;

void clientSetNetState(Client *);
void clientUpdateNetState(Client *, XClientMessageEvent *);
void clientGetNetWmType(Client * c);
void clientCoordGravitate(Client *, int, int *, int *);
void clientGravitate(Client *, int);
void clientConfigure(Client *, XWindowChanges *, int);
void clientFrame(Window);
void clientUnframe(Client *, int);
void clientFrameAll();
void clientUnframeAll();
void clientGetNetStruts(Client *);
void clientInstallColormaps(Client *);
void clientUpdateColormaps(Client *);
void clientGrabKeys(Client *);
void clientUngrabKeys(Client *);
Client *clientGetFromWindow(Window, int);
void clientShow(Client *, int);
void clientHide(Client *, int);
void clientHideAll(Client *);
void clientClose(Client *);
void clientKill(Client *);
void clientRaise(Client *);
void clientLower(Client *);
void clientSetLayer(Client *, int);
void clientSetWorkspace(Client *, int, gboolean);
void clientToggleShaded(Client *);
void clientStick(Client *);
void clientUnstick(Client *);
void clientToggleSticky(Client *);
inline void clientRemoveMaximizeFlag(Client *);
void clientToggleMaximized(Client *, int);
void clientToggleFullscreen(Client *);
void clientUpdateFocus(Client *);
inline gboolean clientAcceptFocus(Client * c);
void clientSetFocus(Client *, int);
Client *clientGetFocus();
void clientMove(Client *, XEvent *);
void clientResize(Client *, int, XEvent *);
Client *clientGetNext(Client *, int);
void clientCycle(Client *);
void clientButtonPress(Client *, Window, XButtonEvent *);

#endif /* __CLIENT_H__ */
