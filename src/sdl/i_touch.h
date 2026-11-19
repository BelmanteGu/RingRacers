// DR. ROBOTNIK'S RING RACERS — Android port
//-----------------------------------------------------------------------------
// Copyright (C) 2026 by the Ring Racers Android contributors.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  i_touch.h
/// \brief Native on-screen touch controls, drawn with the game's own button
///        graphics, plus touch support for menus (tap items, swipe to move).

#ifndef __I_TOUCH_H__
#define __I_TOUCH_H__

#include "../doomtype.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	TOUCH_DOWN,
	TOUCH_MOVE,
	TOUCH_UP,
};

// Feed a raw finger event. nx/ny are normalised (0..1) window coordinates.
void I_TouchEvent(INT32 type, INT64 finger, float nx, float ny);

// A controller/keyboard was used: with "Auto" the overlay steps aside.
void I_TouchNotifyOtherInput(void);

// Draw the overlay on top of everything (after the console).
void I_TouchDrawer(void);

// Per-frame tick (key repeat for held menu directions, auto-hide timer).
void I_TouchTicker(void);

// Menus call this while drawing, so a tap can select/confirm that item.
// Coordinates are in BASEVIDWIDTH x BASEVIDHEIGHT space, centred like
// regular menu drawing (no snap flags).
void I_TouchRegisterMenuItem(INT32 item, INT32 x, INT32 y, INT32 w, INT32 h);

// Virtual keyboard keys register as TOUCH_VKB_ITEM(row, column).
#define TOUCH_VKB_BASE 0x4000
#define TOUCH_VKB_ITEM(row, col) (TOUCH_VKB_BASE + (row) * 64 + (col))

// True when the last input came from the touch screen (not a keyboard or
// controller): the virtual keyboard then stays in "navigate" mode, so the
// on-screen D-pad / OK move over its keys instead of typing letters.
boolean I_TouchOwnsInput(void);

// Whether touch controls are compiled in and enabled right now.
boolean I_TouchActive(void);

#ifdef __cplusplus
}
#endif

#endif
