/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

        xfwm4    - (c) 2003 Olivier Fourdan

 */

#ifndef __SESSION_H__
#define __SESSION_H__

#include <glib.h>
#include "client.h"

/*
**  Save window states to file which name is given in argument.
*/
gboolean sessionSaveWindowStates(gchar *);

/*
**  Load window states to file which name is given in argument.
*/
gboolean sessionLoadWindowStates(gchar *);

/*
** Free allocated structure. Should be called before xfwm4 dies 
*/
void sessionFreeWindowStates(void);

/*
** Search for existing client in saved session and update
** relevant client fields if found. 
*/
gboolean sessionMatchWinToSM(Client *);

#endif /* __CLIENT_H__ */
