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
        Foundation, Inc., Inc., 51 Franklin Street, Fifth Floor, Boston,
        MA 02110-1301, USA.


        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2011 Olivier Fourdan

 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <sys/time.h>
#include <time.h>

#include "screen.h"
#include "misc.h"
#include "hints.h"
#include "keyboard.h"
#include "mypixmap.h"
#include "mywindow.h"
#include "settings.h"

#ifndef INC_CLIENT_H
#define INC_CLIENT_H

#define APPLY                           +1
#define REMOVE                          -1

#define PLACEMENT_MOUSE                 0
#define PLACEMENT_ROOT                  1

#define NO_CFG_FLAG                     0
#define CFG_CONSTRAINED                 (1U<<0)
#define CFG_REQUEST                     (1U<<1)
#define CFG_NOTIFY                      (1U<<2)
#define CFG_KEEP_VISIBLE                (1U<<3)
#define CFG_FORCE_REDRAW                (1U<<4)

#define SEARCH_INCLUDE_HIDDEN           (1U<<0)
#define SEARCH_INCLUDE_SHADED           (1U<<1)
#define SEARCH_INCLUDE_ALL_WORKSPACES   (1U<<2)
#define SEARCH_INCLUDE_SKIP_FOCUS       (1U<<3)
#define SEARCH_INCLUDE_SKIP_PAGER       (1U<<4)
#define SEARCH_INCLUDE_SKIP_TASKBAR     (1U<<5)
#define SEARCH_SAME_APPLICATION         (1U<<6)
#define SEARCH_DIFFERENT_APPLICATION    (1U<<7)

#define NO_UPDATE_FLAG                  0
#define UPDATE_BUTTON_GRABS             (1U<<0)
#define UPDATE_FRAME                    (1U<<1)
#define UPDATE_GRAVITY                  (1U<<2)
#define UPDATE_MAXIMIZE                 (1U<<3)
#define UPDATE_CACHE                    (1U<<4)
#define UPDATE_ALL                      (UPDATE_BUTTON_GRABS | \
                                         UPDATE_FRAME | \
                                         UPDATE_GRAVITY | \
                                         UPDATE_MAXIMIZE | \
                                         UPDATE_CACHE)

#define CLIENT_FILL_VERT                (1U<<0)
#define CLIENT_FILL_HORIZ               (1U<<1)
#define CLIENT_FILL                     (CLIENT_FILL_VERT | \
                                         CLIENT_FILL_HORIZ)

#ifndef CLIENT_MIN_VISIBLE
#define CLIENT_MIN_VISIBLE              15
#endif

#ifndef CLIENT_XSYNC_TIMEOUT
#define CLIENT_XSYNC_TIMEOUT            500  /* ms */
#endif

#ifndef CLIENT_BLINK_TIMEOUT
#define CLIENT_BLINK_TIMEOUT            500  /* ms */
#endif

#ifndef CLIENT_PING_TIMEOUT
#define CLIENT_PING_TIMEOUT             3000 /* ms */
#endif

#ifndef MAX_BLINK_ITERATIONS
#define MAX_BLINK_ITERATIONS            5
#endif

#ifndef MAX_SNAP_DRIFT
#define MAX_SNAP_DRIFT                  15
#endif

#define XFWM_FLAG_HAS_BORDER            (1U<<0)
#define XFWM_FLAG_HAS_MENU              (1U<<1)
#define XFWM_FLAG_HAS_MAXIMIZE          (1U<<2)
#define XFWM_FLAG_HAS_CLOSE             (1U<<3)
#define XFWM_FLAG_HAS_HIDE              (1U<<4)
#define XFWM_FLAG_HAS_MOVE              (1U<<5)
#define XFWM_FLAG_HAS_RESIZE            (1U<<6)
#define XFWM_FLAG_HAS_STICK             (1U<<7)
#define XFWM_FLAG_FOCUS                 (1U<<8)
#define XFWM_FLAG_IS_RESIZABLE          (1U<<9)
#define XFWM_FLAG_MAP_PENDING           (1U<<10)
#define XFWM_FLAG_VISIBLE               (1U<<11)
#define XFWM_FLAG_MANAGED               (1U<<13)
#define XFWM_FLAG_SESSION_MANAGED       (1U<<14)
#define XFWM_FLAG_WORKSPACE_SET         (1U<<15)
#define XFWM_FLAG_WAS_SHOWN             (1U<<16)
#define XFWM_FLAG_DRAW_ACTIVE           (1U<<17)
#define XFWM_FLAG_SEEN_ACTIVE           (1U<<18)
#define XFWM_FLAG_FIRST_MAP             (1U<<19)
#define XFWM_FLAG_SAVED_POS             (1U<<20)
#define XFWM_FLAG_MOVING_RESIZING       (1U<<21)
#define XFWM_FLAG_NEEDS_REDRAW          (1U<<22)
#define XFWM_FLAG_OPACITY_LOCKED        (1U<<23)

#define CLIENT_FLAG_HAS_STRUT           (1U<<0)
#define CLIENT_FLAG_HAS_STRUT_PARTIAL   (1U<<1)
#define CLIENT_FLAG_HAS_USER_TIME       (1U<<2)
#define CLIENT_FLAG_HAS_STARTUP_TIME    (1U<<3)
#define CLIENT_FLAG_ABOVE               (1U<<4)
#define CLIENT_FLAG_BELOW               (1U<<5)
#define CLIENT_FLAG_FULLSCREEN          (1U<<6)
#define CLIENT_FLAG_ICONIFIED           (1U<<7)
#define CLIENT_FLAG_MAXIMIZED_VERT      (1U<<8)
#define CLIENT_FLAG_MAXIMIZED_HORIZ     (1U<<9)
#define CLIENT_FLAG_MAXIMIZED           (CLIENT_FLAG_MAXIMIZED_VERT | \
                                         CLIENT_FLAG_MAXIMIZED_HORIZ)
#define CLIENT_FLAG_SHADED              (1U<<10)
#define CLIENT_FLAG_SKIP_PAGER          (1U<<11)
#define CLIENT_FLAG_SKIP_TASKBAR        (1U<<12)
#define CLIENT_FLAG_STATE_MODAL         (1U<<13)
#define CLIENT_FLAG_STICKY              (1U<<15)
#define CLIENT_FLAG_NAME_CHANGED        (1U<<16)
#define CLIENT_FLAG_DEMANDS_ATTENTION   (1U<<17)
#define CLIENT_FLAG_HAS_SHAPE           (1U<<18)
#define CLIENT_FLAG_FULLSCREEN_MONITORS (1U<<19)
#define CLIENT_FLAG_HAS_FRAME_EXTENTS   (1U<<20)
#define CLIENT_FLAG_HIDE_TITLEBAR       (1U<<21)
#define CLIENT_FLAG_XSYNC_WAITING       (1U<<22)
#define CLIENT_FLAG_XSYNC_ENABLED       (1U<<23)
#define CLIENT_FLAG_XSYNC_EXT_COUNTER   (1U<<24)
#define CLIENT_FLAG_RESTORE_SIZE_POS    (1U<<25)

#define WM_FLAG_DELETE                  (1U<<0)
#define WM_FLAG_INPUT                   (1U<<1)
#define WM_FLAG_TAKEFOCUS               (1U<<2)
#define WM_FLAG_CONTEXT_HELP            (1U<<3)
#define WM_FLAG_URGENT                  (1U<<4)
#define WM_FLAG_PING                    (1U<<5)

#define XFWM_FLAG_INITIAL_VALUES        XFWM_FLAG_HAS_BORDER | \
                                        XFWM_FLAG_HAS_MENU | \
                                        XFWM_FLAG_HAS_MAXIMIZE | \
                                        XFWM_FLAG_HAS_STICK | \
                                        XFWM_FLAG_HAS_HIDE | \
                                        XFWM_FLAG_HAS_CLOSE | \
                                        XFWM_FLAG_HAS_MOVE | \
                                        XFWM_FLAG_HAS_RESIZE | \
                                        XFWM_FLAG_FIRST_MAP | \
                                        XFWM_FLAG_NEEDS_REDRAW

#define ALL_WORKSPACES                  (guint) 0xFFFFFFFF

#define CONSTRAINED_WINDOW(c)           ((c->win_layer > WIN_LAYER_DESKTOP) && \
                                        !(c->props.type & (WINDOW_DESKTOP | WINDOW_DOCK)))

#define WINDOW_TYPE_DIALOG              (WINDOW_DIALOG | \
                                         WINDOW_MODAL_DIALOG)
#define WINDOW_TYPE_DONT_PLACE          (WINDOW_DESKTOP | \
                                         WINDOW_DOCK | \
                                         WINDOW_UTILITY | \
                                         WINDOW_SPLASHSCREEN)
#define WINDOW_REGULAR_FOCUSABLE        (WINDOW_NORMAL | \
                                         WINDOW_TYPE_DIALOG | \
                                         WINDOW_UTILITY)
#define WINDOW_TYPE_DONT_FOCUS          (WINDOW_SPLASHSCREEN | \
                                         WINDOW_DOCK)
#define WINDOW_TYPE_STATE_FOCUSED       (WINDOW_SPLASHSCREEN | \
                                         WINDOW_DOCK )

/* Which bits of opacity are applied */
#define OPACITY_MOVE                    (1U<<0)
#define OPACITY_RESIZE                  (1U<<1)
#define OPACITY_INACTIVE                (1U<<2)

/* Convenient macros */
#define FLAG_TEST(flag,bits)                   ((flag) & (bits))
#define FLAG_TEST_ALL(flag,bits)               (((flag) & (bits)) == (bits))
#define FLAG_TEST_AND_NOT(flag,bits1,bits2)    (((flag) & ((bits1) | (bits2))) == (bits1))
#define FLAG_SET(flag,bits)                    (flag |= (bits))
#define FLAG_UNSET(flag,bits)                  (flag &= ~(bits))
#define FLAG_TOGGLE(flag,bits)                 (flag ^= (bits))

#define CLIENT_CAN_HIDE_WINDOW(c)       (FLAG_TEST(c->xfwm_flags, XFWM_FLAG_HAS_HIDE) && \
                                         !FLAG_TEST(c->flags, CLIENT_FLAG_SKIP_TASKBAR))
#define CLIENT_CAN_MAXIMIZE_WINDOW(c)   (FLAG_TEST_ALL(c->xfwm_flags, XFWM_FLAG_HAS_MAXIMIZE | \
                                                                      XFWM_FLAG_IS_RESIZABLE) && \
                                         !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
#define CLIENT_CAN_FILL_WINDOW(c)       (FLAG_TEST(c->xfwm_flags, XFWM_FLAG_HAS_RESIZE | \
                                                                  XFWM_FLAG_IS_RESIZABLE) && \
                                         !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN | \
                                                               CLIENT_FLAG_MAXIMIZED | \
                                                               CLIENT_FLAG_SHADED))
#define CLIENT_CAN_TILE_WINDOW(c)       (CLIENT_CAN_MAXIMIZE_WINDOW(c) && \
                                         !FLAG_TEST (c->flags, CLIENT_FLAG_SHADED) && \
                                         (c->props.type & WINDOW_NORMAL))
#define CLIENT_HAS_FRAME(c)             (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER) && \
                                         !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN) && \
                                         (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED) || \
                                          !FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED) ||  \
                                          !((FLAG_TEST (c->flags, CLIENT_FLAG_HIDE_TITLEBAR) || \
                                            (c->screen_info->params->titleless_maximize)) && \
                                            (c->screen_info->params->borderless_maximize))))

typedef enum
{
    UNSET = 0,
    WINDOW_NORMAL       = (1 << 0),
    WINDOW_DESKTOP      = (1 << 1),
    WINDOW_DOCK         = (1 << 2),
    WINDOW_DIALOG       = (1 << 3),
    WINDOW_MODAL_DIALOG = (1 << 4),
    WINDOW_TOOLBAR      = (1 << 5),
    WINDOW_MENU         = (1 << 6),
    WINDOW_UTILITY      = (1 << 7),
    WINDOW_SPLASHSCREEN = (1 << 8),
    WINDOW_NOTIFICATION = (1 << 9)
}
netWindowType;

typedef enum
{
    TILE_NONE = 0,
    TILE_LEFT,
    TILE_RIGHT,
    TILE_DOWN,
    TILE_UP,
    TILE_DOWN_LEFT,
    TILE_DOWN_RIGHT,
    TILE_UP_LEFT,
    TILE_UP_RIGHT
}
tilePositionType;

struct _Client
{
    /* Pointers (8 bytes each on 64-bit) */
    ScreenInfo   *screen_info;
    Window       *cmap_windows;
    Visual       *visual;
    XSizeHints   *size;
    XWMHints     *wmhints;
    Client       *next;
    Client       *prev;
    PropMwmHints *mwm_hints;
    gchar        *hostname;
    gchar        *name;
    XClassHint    class;
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    gchar        *startup_id;
#endif /* HAVE_LIBSTARTUP_NOTIFICATION */

    /* 64-bit types (on LP64) */
    Window        window;
    Window        frame;
    Window        transient_for;
    Window        user_time_win;
    Window        client_leader;
    Window        group_leader;
    Colormap      cmap;
    unsigned long win_layer;
    unsigned long serial;
    unsigned long initial_layer;
    Atom          type_atom;
#ifdef HAVE_XSYNC
    XSyncAlarm    xsync_alarm;
    XSyncCounter  xsync_counter;
    XSyncValue    xsync_value;
    XSyncValue    next_xsync_value;
#endif /* HAVE_XSYNC */

    /* 32-bit types */
    guint32       flags;
    guint32       wm_flags;
    guint32       xfwm_flags;
    gint          ncmap;
    gint          x;
    gint          y;
    gint          width;
    gint          height;
    gint          depth;
    gint          border_width;
    gint          gravity;
    guint         win_workspace;
    gint          pre_fullscreen_layer;
    gint          pre_relayout_x;
    gint          pre_relayout_y;
    gint          frame_cache_width;
    gint          frame_cache_height;
    guint32       user_time;
    GPid          pid;
    guint32       ping_time;
    gint          dialog_pid;
    gint          dialog_fd;
    guint         dialog_watch_id;
    guint         icon_timeout_id;
    guint         frame_timeout_id;
    guint         blink_timeout_id;
    guint         ping_timeout_id;
    guint32       opacity;
    guint32       opacity_applied;
#ifdef HAVE_XSYNC
    guint         xsync_timeout_id;
#endif /* HAVE_XSYNC */

    /* Bit fields */
    struct {
        netWindowType    type : 10;
        tilePositionType tile_mode : 4;
        guint            blink_iterations : 3;
        guint            opacity_flags : 3;
        /* 12 bits free for future use */
    } props;

    /* Other integer types */
    unsigned int  ignore_unmap;

    /* Arrays and other structs */
    xfwmWindow    title;
    xfwmWindow    sides[SIDE_COUNT];
    xfwmWindow    corners[CORNER_COUNT];
    xfwmWindow    buttons[BUTTON_COUNT];
    xfwmPixmap    appmenu[STATE_TOGGLED];
    GdkRectangle  applied_geometry;
    GdkRectangle  saved_geometry;
    GdkRectangle  pre_fullscreen_geometry;
    gint          button_status[BUTTON_COUNT];
    gint          struts[STRUTS_SIZE];
    gint          fullscreen_monitors[4];
    gint          frame_extents[SIDE_COUNT];
};

extern Client *clients;
extern unsigned int client_count;

Display                 *clientGetXDisplay                      (Client *);
void                     clientUpdateUrgency                    (Client *);
void                     clientCoordGravitate                   (Client *,
                                                                 int,
                                                                 int,
                                                                 int *,
                                                                 int *);
void                     clientAdjustCoordGravity               (Client *,
                                                                 int,
                                                                 XWindowChanges *,
                                                                 unsigned long *);
void                     clientSendConfigureNotify              (Client *);
void                     clientConfigure                        (Client *,
                                                                 XWindowChanges *,
                                                                 unsigned long,
                                                                 unsigned short);
void                     clientReconfigure                      (Client *,
                                                                 unsigned short);
void                     clientMoveResizeWindow                 (Client *,
                                                                 XWindowChanges *,
                                                                 unsigned long);
void                     clientGetMWMHints                      (Client *);
void                     clientApplyMWMHints                    (Client *,
                                                                 gboolean);
void                     clientGetWMNormalHints                 (Client *,
                                                                 gboolean);
void                     clientGetWMProtocols                   (Client *);
void                     clientUpdateIcon                       (Client *,
                                                                 gboolean);
void                     clientSaveSizePos                      (Client *);
gboolean                 clientRestoreSizePos                   (Client *);
Client                  *clientFrame                            (DisplayInfo *,
                                                                 Window,
                                                                 gboolean);
void                     clientUnframe                          (Client *,
                                                                 gboolean);
void                     clientFrameAll                         (ScreenInfo *);
void                     clientUnframeAll                       (ScreenInfo *);
void                     clientInstallColormaps                 (Client *);
void                     clientUpdateColormaps                  (Client *);
void                     clientUpdateName                       (Client *);
void                     clientUpdateAllFrames                  (ScreenInfo *,
                                                                 gboolean);
void                     clientGrabButtons                      (Client *);
void                     clientUngrabButtons                    (Client *);
Client                  *clientGetFromWindow                    (Client *,
                                                                 Window,
                                                                 unsigned short);
void                     clientShow                             (Client *,
                                                                 gboolean);
void                     clientWithdraw                         (Client *,
                                                                 guint,
                                                                 gboolean);
void                     clientWithdrawAll                      (Client *,
                                                                 guint);
void                     clientClearAllShowDesktop              (ScreenInfo *);
void                     clientToggleShowDesktop                (ScreenInfo *);
void                     clientActivate                         (Client *,
                                                                 guint32,
                                                                 gboolean);
void                     clientClose                            (Client *);
void                     clientKill                             (Client *);
void                     clientTerminate                        (Client *);
void                     clientEnterContextMenuState            (Client *);
void                     clientSetLayer                         (Client *,
                                                                 guint);
void                     clientSetWorkspace                     (Client *,
                                                                 guint,
                                                                 gboolean);
void                     clientShade                            (Client *);
void                     clientUnshade                          (Client *);
void                     clientToggleShaded                     (Client *);
void                     clientStick                            (Client *,
                                                                 gboolean);
void                     clientUnstick                          (Client *,
                                                                 gboolean);
void                     clientToggleSticky                     (Client *,
                                                                 gboolean);
void                     clientUpdateFullscreenSize             (Client *);
void                     clientToggleFullscreen                 (Client *);
void                     clientSetFullscreenMonitor             (Client *,
                                                                 gint,
                                                                 gint,
                                                                 gint,
                                                                 gint);
void                     clientToggleLayerAbove                 (Client *);
void                     clientToggleLayerBelow                 (Client *);
void                     clientSetLayerNormal                   (Client *);
void                     clientRemoveMaximizeFlag               (Client *);
void                     clientUpdateMaximizeSize               (Client *);
gboolean                 clientToggleMaximized                  (Client *,
                                                                 int,
                                                                 gboolean);
gboolean                 clientToggleMaximizedAtPoint           (Client *,
                                                                 gint,
                                                                 gint,
                                                                 int,
                                                                 gboolean);
gboolean                 clientMoveToMonitorByDirectionPossible (Client *,
                                                                 gint);
void                     clientMoveToMonitorByDirection         (Client *,
                                                                 gint);
gboolean                 clientTile                             (Client *,
                                                                 gint,
                                                                 gint,
                                                                 tilePositionType,
                                                                 gboolean,
                                                                 gboolean);
void                     clientUntile                           (Client *);
gboolean                 clientToggleTile                       (Client *,
                                                                 tilePositionType);
void                     clientUpdateTileSize                   (Client *);
void                     clientUpdateOpacity                    (Client *);
void                     clientUpdateAllOpacity                 (ScreenInfo *);
void                     clientSetOpacity                       (Client *,
                                                                 guint32,
                                                                 guint32,
                                                                 guint32);
void                     clientIncOpacity                       (Client *);
void                     clientDecOpacity                       (Client *);
void                     clientUpdateCursor                     (Client *);
void                     clientUpdateAllCursor                  (ScreenInfo *);
void                     clientScreenResize                     (ScreenInfo *,
                                                                 gboolean,
                                                                 gboolean);
void                     clientButtonPress                      (Client *,
                                                                 Window,
                                                                 XfwmEventButton *);
xfwmPixmap *             clientGetButtonPixmap                  (Client *,
                                                                 int,
                                                                 int);
int                      clientGetButtonState                   (Client *,
                                                                 int,
                                                                 int);
gboolean                 clientGetGtkFrameExtents               (Client *);
gboolean                 clientGetGtkHideTitlebar               (Client *);
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
char                    *clientGetStartupId                     (Client *);
#endif /* HAVE_LIBSTARTUP_NOTIFICATION */

#endif /* INC_CLIENT_H */
