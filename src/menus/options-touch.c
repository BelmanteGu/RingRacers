// DR. ROBOTNIK'S RING RACERS — Android port
//-----------------------------------------------------------------------------
// Copyright (C) 2026 by the Ring Racers Android contributors.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  menus/options-touch.c
/// \brief Touch & Display options (Android): on-screen controls, ultrawide

#include "../k_menu.h"

extern consvar_t cv_touch_controls, cv_touch_opacity, cv_touch_size,
	cv_touch_lefty, cv_touch_labels, cv_touch_menuswipe, cv_ultrawide, cv_hudsafearea;

menuitem_t OPTIONS_Touch[] =
{
	{IT_HEADER, "On-Screen Controls...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Touch Controls", "Auto hides them while you use a controller or keyboard.",
		NULL, {.cvar = &cv_touch_controls}, 0, 0},

	{IT_STRING | IT_CVAR, "Button Size", "How big the on-screen buttons are.",
		NULL, {.cvar = &cv_touch_size}, 0, 0},

	{IT_STRING | IT_CVAR | IT_CV_SLIDER, "Opacity", "How solid the on-screen buttons look.",
		NULL, {.cvar = &cv_touch_opacity}, 0, 0},

	{IT_STRING | IT_CVAR, "Left-Handed", "Swap the D-Pad and the buttons.",
		NULL, {.cvar = &cv_touch_lefty}, 0, 0},

	{IT_STRING | IT_CVAR, "Button Labels", "Show what each button does under it.",
		NULL, {.cvar = &cv_touch_labels}, 0, 0},

	{IT_STRING | IT_CVAR, "Swipe in Menus", "Drag a finger in menus to move the cursor.",
		NULL, {.cvar = &cv_touch_menuswipe}, 0, 0},

	{IT_HEADER, "Display...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Ultrawide", "Fill your phone's whole screen instead of 16:9.",
		NULL, {.cvar = &cv_ultrawide}, 0, 0},

	{IT_STRING | IT_CVAR, "HUD Safe Area", "Keep the HUD away from the camera cutout and rounded corners.",
		NULL, {.cvar = &cv_hudsafearea}, 0, 0},
};

menu_t OPTIONS_TouchDef = {
	sizeof (OPTIONS_Touch) / sizeof (menuitem_t),
	&OPTIONS_MainDef,
	1,
	OPTIONS_Touch,
	48, 80,
	SKINCOLOR_BLUEBERRY, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	NULL,
	NULL,
	NULL,
};
