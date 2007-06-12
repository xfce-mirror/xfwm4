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
        xfwm4    - (c) 2002-2007 Olivier Fourdan

 */

#ifndef INC_FRAME_H
#define INC_FRAME_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "screen.h"
#include "mypixmap.h"
#include "client.h"

struct _FramePixmap
{
    xfwmPixmap pm_title;
    xfwmPixmap pm_sides[SIDE_COUNT];
};

typedef struct _FramePixmap FramePixmap;

int                      frameDecorationLeft                    (ScreenInfo *);
int                      frameDecorationRight                   (ScreenInfo *);
int                      frameDecorationTop                     (ScreenInfo *);
int                      frameDecorationBottom                  (ScreenInfo *);
int                      frameLeft                              (Client *);
int                      frameRight                             (Client *);
int                      frameTop                               (Client *);
int                      frameBottom                            (Client *);
int                      frameX                                 (Client *);
int                      frameY                                 (Client *);
int                      frameWidth                             (Client *);
int                      frameHeight                            (Client *);
void                     frameDraw                              (Client *,
                                                                 gboolean);
void                     frameClearQueueDraw                    (Client *);
void                     frameQueueDraw                         (Client *);

#endif /* INC_FRAME_H */
