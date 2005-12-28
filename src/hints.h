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
        Metacity - (c) 2001 Havoc Pennington
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifndef INC_HINTS_H
#define INC_HINTS_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <X11/Xatom.h>
#include <glib.h>
#include <gdk/gdk.h>

#include "display.h"

#define MWM_HINTS_ELEMENTS                      3L
#define MAX_TITLE_LENGTH                        255

#define MWM_HINTS_FUNCTIONS                     (1L << 0)
#define MWM_HINTS_DECORATIONS                   (1L << 1)
#define MWM_HINTS_INPUT_MODE                    (1L << 2)
#define MWM_HINTS_STATUS                        (1L << 3)

#define MWM_FUNC_ALL                            (1L << 0)
#define MWM_FUNC_RESIZE                         (1L << 1)
#define MWM_FUNC_MOVE                           (1L << 2)
#define MWM_FUNC_MINIMIZE                       (1L << 3)
#define MWM_FUNC_MAXIMIZE                       (1L << 4)
#define MWM_FUNC_CLOSE                          (1L << 5)

#define MWM_DECOR_ALL                           (1L << 0)
#define MWM_DECOR_BORDER                        (1L << 1)
#define MWM_DECOR_RESIZEH                       (1L << 2)
#define MWM_DECOR_TITLE                         (1L << 3)
#define MWM_DECOR_MENU                          (1L << 4)
#define MWM_DECOR_MINIMIZE                      (1L << 5)
#define MWM_DECOR_MAXIMIZE                      (1L << 6)

#define MWM_INPUT_MODELESS                      0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL     1
#define MWM_INPUT_SYSTEM_MODAL                  2
#define MWM_INPUT_FULL_APPLICATION_MODAL        3
#define MWM_INPUT_APPLICATION_MODAL             MWM_INPUT_PRIMARY_APPLICATION_MODAL

#define MWM_TEAROFF_WINDOW                      (1L<<0)

#define WIN_STATE_STICKY                        (1L<<0)
#define WIN_STATE_MAXIMIZED_VERT                (1L<<2)
#define WIN_STATE_MAXIMIZED_HORIZ               (1L<<3)
#define WIN_STATE_MAXIMIZED                     ((1L<<2)|(1L<<3))
#define WIN_STATE_SHADED                        (1L<<5)

#define WIN_HINTS_SKIP_FOCUS                    (1L<<0)
#define WIN_HINTS_SKIP_TASKBAR                  (1L<<2)

#define WM_PROTOCOLS_TAKE_FOCUS                 (1L<<0)
#define WM_PROTOCOLS_DELETE_WINDOW              (1L<<1)
#define WM_PROTOCOLS_CONTEXT_HELP               (1L<<2)

#define WIN_LAYER_DESKTOP                       0
#define WIN_LAYER_BELOW                         2
#define WIN_LAYER_NORMAL                        4
#define WIN_LAYER_ONTOP                         6
#define WIN_LAYER_DOCK                          8
#define WIN_LAYER_ABOVE_DOCK                    10

#define NET_WM_MOVERESIZE_SIZE_TOPLEFT          0
#define NET_WM_MOVERESIZE_SIZE_TOP              1
#define NET_WM_MOVERESIZE_SIZE_TOPRIGHT         2
#define NET_WM_MOVERESIZE_SIZE_RIGHT            3
#define NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT      4
#define NET_WM_MOVERESIZE_SIZE_BOTTOM           5
#define NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT       6
#define NET_WM_MOVERESIZE_SIZE_LEFT             7
#define NET_WM_MOVERESIZE_MOVE                  8

#define NET_WM_STATE_REMOVE                     0
#define NET_WM_STATE_ADD                        1
#define NET_WM_STATE_TOGGLE                     2

#define NET_WM_ORIENTATION_HORZ                 0
#define NET_WM_ORIENTATION_VERT                 1

#define NET_WM_TOPLEFT                          0
#define NET_WM_TOPRIGHT                         1
#define NET_WM_BOTTOMRIGHT                      2
#define NET_WM_BOTTOMLEFT                       3

#define NET_WM_OPAQUE                           0xffffffff

typedef struct
{
    unsigned long orientation;
    unsigned long start;
    unsigned long rows;
    unsigned long cols;
}
NetWmDesktopLayout;

#define LEFT                            0
#define RIGHT                           1
#define TOP                             2
#define BOTTOM                          3
#define LEFT_START_Y                    4
#define LEFT_END_Y                      5
#define RIGHT_START_Y                   6
#define RIGHT_END_Y                     7
#define TOP_START_X                     8
#define TOP_END_X                       9
#define BOTTOM_START_X                  10
#define BOTTOM_END_X                    11

typedef struct
{
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
}
PropMwmHints;

unsigned long getWMState (DisplayInfo *, Window);
void setWMState (DisplayInfo *, Window, unsigned long);
PropMwmHints *getMotifHints (DisplayInfo *, Window);
unsigned int getWMProtocols (DisplayInfo *, Window);
gboolean getHint (DisplayInfo *, Window, int, long *);
void setHint (DisplayInfo *, Window, int, long);
void getDesktopLayout (DisplayInfo *, Window, int, NetWmDesktopLayout *);
void getGnomeDesktopMargins (DisplayInfo *, Window, int *);
void setGnomeProtocols (DisplayInfo *, Window, Window);
void setNetSupportedHint (DisplayInfo *, Window, Window);
gboolean getAtomList (DisplayInfo *, Window, int, Atom **, int *);
gboolean getCardinalList (DisplayInfo *, Window, int, unsigned long **, int *);
void setNetWorkarea (DisplayInfo *, Window, int, int, int, int *);
void setNetFrameExtents (DisplayInfo *, Window, int, int, int, int);
void initNetDesktopInfo (DisplayInfo *, Window, int, int, int);
void setUTF8StringHint (DisplayInfo *, Window, int, const char *);
void getTransientFor (DisplayInfo *, Window, Window, Window *);
void getWindowName (DisplayInfo *, Window, char **);
gboolean getUTF8String (DisplayInfo *, Window, int, char **, int *);
void getWindowName (DisplayInfo *, Window, char **);
gboolean checkKdeSystrayWindow(DisplayInfo *, Window);
void sendSystrayReqDock(DisplayInfo *, Window, Window);
Window getSystrayWindow (DisplayInfo *, Atom);
gboolean getWindowRole (DisplayInfo *, Window, char **);
Window getClientLeader (DisplayInfo *, Window);
gboolean getNetWMUserTime (DisplayInfo *, Window, Time *);
gboolean getClientID (DisplayInfo *, Window, char **);
gboolean getWindowCommand (DisplayInfo *, Window, char ***, int *);
gboolean getKDEIcon (DisplayInfo *, Window, Pixmap *, Pixmap *);
gboolean getRGBIconData (DisplayInfo *, Window, unsigned long **, unsigned long *);
gboolean getOpacity (DisplayInfo *, Window, guint *);

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
gboolean getWindowStartupId (DisplayInfo *, Window, char **);
#endif

#endif /* INC_HINTS_H */
