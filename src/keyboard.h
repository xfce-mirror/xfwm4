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

#ifndef INC_KEYBOARD_H
#define INC_KEYBOARD_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/keysym.h>

typedef struct
{
    KeyCode keycode;
    int modifier;
}
MyKey;

extern unsigned int KeyMask;
extern unsigned int ButtonMask;
extern unsigned int ButtonKeyMask;
extern unsigned int AltMask;
extern unsigned int MetaMask;
extern unsigned int NumLockMask;
extern unsigned int ScrollLockMask;
extern unsigned int CapsLockMask;
extern unsigned int SuperMask;
extern unsigned int HyperMask;

void parseKeyString(Display *, MyKey *, char *);
void grabKey(Display *, MyKey *, Window);
void ungrabKeys(Display *, Window);
void initModifiers(Display *);

#endif /* INC_KEYBOARD_H */
