/*  gxfce
 *  Copyright (C) 2002 Olivier Fourdan (fourdan@xfce.org)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifndef __XINERAMA_H__
#define __XINERAMA_H__

/* Define EMULATE_XINERAMA to emulate Xinerama on a systems without Xinerama
 * native support. For DEBUG purpose ONLY !
 */
#undef EMULATE_XINERAMA

#ifdef EMULATE_XINERAMA
#ifndef HAVE_X11_EXTENSIONS_XINERAMA_H
/* Define XineramaScreenInfo for systems missing Xinerama support */
typedef struct
{
    int screen_number;
    short x_org;
    short y_org;
    short width;
    short height;
}
XineramaScreenInfo;
#endif
#endif

gboolean xineramaInit(Display * dpy);
void xineramaFree(void);

inline int xineramaGetHeads(void);
inline int MyDisplayHeight(Display * dpy, int screen, int x, int y);
inline int MyDisplayWidth(Display * dpy, int screen, int x, int y);
inline int MyDisplayX(int x, int y);
inline int MyDisplayY(int x, int y);
inline int MyDisplayMaxX(Display * dpy, int screen, int x, int y);
inline int MyDisplayMaxY(Display * dpy, int screen, int x, int y);
inline gboolean isRightMostHead(Display * dpy, int screen, int x, int y);
inline gboolean isLeftMostHead(Display * dpy, int screen, int x, int y);
inline gboolean isTopMostHead(Display * dpy, int screen, int x, int y);
inline gboolean isBottomMostHead(Display * dpy, int screen, int x, int y);

#endif
