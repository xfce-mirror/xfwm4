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
 
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifndef INC_STACKING_H
#define INC_STACKING_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include "client.h"

extern          GList *windows_stack;

void            clientApplyStackList (void);
gboolean        clientTransientOrModalHasAncestor (Client *, int);
Client         *clientGetLowestTransient (Client *);
Client         *clientGetHighestTransientOrModalFor (Client *);
Client         *clientGetTopMostForGroup (Client *);
Client         *clientGetNextTopMost (int, Client *);
Client         *clientGetBottomMost (int, Client *);
Client         *clientAtPosition (int, int, Client *);
void            clientRaise (Client *);
void            clientLower (Client *);
void            clientAddToList (Client *);
void            clientRemoveFromList (Client *);
GList          *clientGetStackList (void);
void            clientSetLastRaise (Client *);
Client         *clientGetLastRaise (void);
void            clientClearLastRaise (void);

#endif /* INC_STACKING_H */
