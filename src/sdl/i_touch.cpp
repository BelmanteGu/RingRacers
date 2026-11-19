// DR. ROBOTNIK'S RING RACERS — Android port
//-----------------------------------------------------------------------------
// Copyright (C) 2026 by the Ring Racers Android contributors.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  i_touch.cpp
/// \brief Native on-screen touch controls.
///
/// The overlay is drawn by the engine itself with the game's own tooltip
/// button graphics (TLB_*), so it matches the rest of the UI, and it feeds
/// the game exactly like a keyboard would: every control posts the default
/// keyboard bind of its game control, so menus, profiles and remapping all
/// keep working unchanged.
///
/// Two layouts:
///   race  — D-pad + GO / DRIFT / ITEM / BRAKE / SPIN / LOOK / BAIL (+ VOTE
///           and CHAT online) and a pause button;
///   menus — D-pad + OK / BACK / EXTRA, swipe anywhere to move, and tap a
///           menu line to select it (tap it again to confirm, or tap the
///           left/right half of an option to change its value).

#include "i_touch.h"

#if defined(__ANDROID__) || defined(ANDROID)

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "../doomdef.h"
#include "../doomstat.h"
#include "../command.h"
#include "../console.h"
#include "../d_event.h"
#include "../d_main.h"
#include "../g_game.h"
#include "../g_input.h"
#include "../g_state.h"
#include "../hu_stuff.h"
#include "../i_system.h"
#include "../k_hud.h"
#include "../k_menu.h"
#include "../keys.h"
#include "../s_sound.h"
#include "../screen.h"
#include "../v_video.h"
#include "../w_wad.h"
#include "../z_zone.h"

extern "C" consvar_t cv_touch_controls, cv_touch_opacity, cv_touch_size,
	cv_touch_lefty, cv_touch_labels, cv_touch_menuswipe;

namespace
{

enum class Kind
{
	button,  // presses `key` while a finger is on it
	dpad,    // 8-way digital pad (arrow keys)
	keyboard // toggles the Android soft keyboard
};

struct Element
{
	Kind kind;
	float cx, cy;     // centre, screen pixels (vid.width x vid.height)
	float d;          // visual diameter, pixels
	INT32 key;        // key posted (button)
	patch_t **patch;  // [0] = up, [1] = pressed (game's TLB_* graphics)
	const char *label;
	bool primary;     // drawn a bit brighter (GO / OK)
};

struct Finger
{
	INT64 id;
	float x, y;        // current, pixels
	float sx, sy;      // where it went down
	float swx, swy;    // last swipe step origin
	UINT32 downtime;
	bool on_element;   // started on a control (vs free area: swipe/tap)
	bool swiped;
};

struct MenuHit
{
	INT32 item;
	INT32 x, y, w, h; // base 320x200, centred
};

std::vector<Element> g_layout;
std::vector<Finger> g_fingers;
std::vector<MenuHit> g_hits_building, g_hits;
bool g_pressed[NUMKEYS];
std::vector<std::pair<INT32, UINT32>> g_pulses; // key, release time
UINT32 g_last_touch = 0, g_last_other = 0;
bool g_menu_layout = false;

INT32 W() { return vid.width > 0 ? vid.width : 1; }
INT32 H() { return vid.height > 0 ? vid.height : 1; }

void post_key(INT32 key, bool down)
{
	event_t ev = {};
	ev.type = down ? ev_keydown : ev_keyup;
	ev.data1 = key;
	ev.data2 = 0;
	ev.device = 0; // same device as the keyboard
	D_PostEvent(&ev);
}

void pulse(INT32 key)
{
	I_OutputMsg("touch: pulse key %d\n", key);
	if (!g_pressed[key])
		post_key(key, true);
	g_pulses.push_back({key, SDL_GetTicks() + 70});
}

bool in_menu_context()
{
	if (menuactive || chat_on || con_destlines > 0)
		return true;
	return (gamestate != GS_LEVEL);
}

bool overlay_visible()
{
	if (!graphics_started || gamestate == GS_NULL)
		return false;
	switch (cv_touch_controls.value)
	{
		case 0: return false;
		case 1: return true;
		default: // Auto: hide once a controller/keyboard is used, until the next touch
			return !(g_last_other > g_last_touch);
	}
}

// Layout ------------------------------------------------------------------

// The keyboard key bound to a game control in the player's profile, so the
// touch button presses exactly what the game's on-screen prompts show (and
// follows the player's remapping).
INT32 kb(INT32 gc, INT32 fallback)
{
	for (INT32 i = 0; i < MAXINPUTMAPPING; i++)
	{
		const INT32 k = gamecontrol[0][gc][i];
		if (k > 0 && k < KEY_JOY1)
			return k;
	}
	return fallback;
}

void add(Kind kind, float cx, float cy, float d, INT32 key, patch_t **patch, const char *label, bool primary = false)
{
	const float scale = cv_touch_size.value / 100.f;
	if (cv_touch_lefty.value)
		cx = W() - cx;
	g_layout.push_back({kind, cx, cy, d * scale, key, patch, label, primary});
}

void build_layout()
{
	g_layout.clear();
	g_menu_layout = in_menu_context();

	const float w = (float)W();
	const float h = (float)H();
	const float u = h; // everything is sized from the screen height
	const float s = cv_touch_size.value / 100.f;

	// D-pad: anchored bottom-left, grows inward with the size option
	const float dpx = 0.25f * u * s + 0.04f * u;
	const float dpy = h - 0.25f * u * s - 0.05f * u;
	add(Kind::dpad, dpx, dpy, 0.42f * u, 0, nullptr, nullptr);

	auto R = [&](float fx) { return w - fx * u * s - 0.03f * u; };
	auto B = [&](float fy) { return h - fy * u * s - 0.03f * u; };

	if (g_menu_layout)
	{
		add(Kind::button, R(0.13f), B(0.15f), 0.22f * u, kb(gc_a, 'a'), kp_button_a[0], "OK", true);
		add(Kind::button, R(0.38f), B(0.10f), 0.17f * u, kb(gc_b, KEY_LSHIFT), kp_button_b[0], "BACK");
		add(Kind::button, R(0.15f), B(0.43f), 0.14f * u, kb(gc_c, 'q'), kp_button_c[0], "EXTRA");
		if (chat_on || con_destlines > 0 || menutyping.active)
			add(Kind::keyboard, w * 0.5f, 0.09f * u, 0.12f * u, 0, kp_button_start[0], "KEYBOARD");
		return;
	}

	// Race: big GO under the thumb, DRIFT next to it (sliding a thumb
	// between them holds both), ITEM above, BRAKE on the far side.
	add(Kind::button, R(0.13f), B(0.15f), 0.24f * u, kb(gc_a, 'a'), kp_button_a[0], "GO", true);
	add(Kind::button, R(0.40f), B(0.10f), 0.20f * u, kb(gc_r, 's'), kp_button_r[0], "DRIFT");
	add(Kind::button, R(0.30f), B(0.40f), 0.17f * u, kb(gc_l, KEY_SPACE), kp_button_l[0], "ITEM");
	add(Kind::button, R(0.06f), B(0.44f), 0.15f * u, kb(gc_x, 'd'), kp_button_x[0], "BRAKE");
	add(Kind::button, R(0.63f), B(0.08f), 0.14f * u, kb(gc_c, 'q'), kp_button_c[0], "SPIN");
	add(Kind::button, R(0.58f), B(0.33f), 0.12f * u, kb(gc_b, KEY_LSHIFT), kp_button_b[0], "LOOK");
	add(Kind::button, R(0.10f), B(0.68f), 0.12f * u, kb(gc_y, 'v'), kp_button_y[0], "BAIL");
	if (netgame)
	{
		add(Kind::button, R(0.33f), B(0.66f), 0.11f * u, kb(gc_z, 'z'), kp_button_z[0], "VOTE");
		add(Kind::button, w * 0.5f - 0.10f * u, 0.08f * u, 0.10f * u, kb(gc_talk, 't'), kp_button_y[0], "CHAT");
	}
	add(Kind::button, w * 0.5f + (netgame ? 0.10f * u : 0.f), 0.08f * u, 0.10f * u, kb(gc_start, KEY_ESCAPE), kp_button_start[0], "PAUSE");
}

// Hit testing ---------------------------------------------------------------

void dpad_keys(const Element &e, float x, float y, std::vector<INT32> &out)
{
	const float dx = x - e.cx, dy = y - e.cy;
	const float dist = std::sqrt(dx * dx + dy * dy);
	if (dist < e.d * 0.10f) // dead zone in the middle
		return;
	const float ang = std::atan2(-dy, dx) * 180.f / (float)M_PI; // 0 = right, 90 = up
	auto near = [&](float a) { float d = std::fabs(std::fmod(ang - a + 540.f, 360.f) - 180.f); return d <= 67.5f; };
	if (near(0)) out.push_back(KEY_RIGHTARROW);
	if (near(90)) out.push_back(KEY_UPARROW);
	if (near(180)) out.push_back(KEY_LEFTARROW);
	if (near(-90)) out.push_back(KEY_DOWNARROW);
}

bool hit(const Element &e, float x, float y, float slack)
{
	const float dx = x - e.cx, dy = y - e.cy;
	const float r = e.d * (e.kind == Kind::dpad ? 0.62f : 0.5f) * slack;
	return dx * dx + dy * dy <= r * r;
}

// Fingers that started on a control keep steering/pressing as they slide
// (with generous slack), so a thumb can roll from GO onto DRIFT.
void keys_for_finger(const Finger &f, std::vector<INT32> &out)
{
	if (!f.on_element)
		return;
	bool any = false;
	for (const Element &e : g_layout)
	{
		if (e.kind == Kind::dpad)
		{
			if (hit(e, f.sx, f.sy, 1.0f) && hit(e, f.x, f.y, 1.6f))
			{
				dpad_keys(e, f.x, f.y, out);
				return; // the d-pad owns this finger
			}
			continue;
		}
		if (e.kind == Kind::button && hit(e, f.x, f.y, any ? 1.0f : 1.15f))
		{
			out.push_back(e.key);
			any = true;
		}
	}
}

void sync_keys()
{
	bool want[NUMKEYS] = {};
	std::vector<INT32> keys;
	for (const Finger &f : g_fingers)
	{
		keys.clear();
		keys_for_finger(f, keys);
		for (INT32 k : keys)
			want[k] = true;
	}
	const UINT32 now = SDL_GetTicks();
	for (auto &p : g_pulses)
		if (now < p.second)
			want[p.first] = true;
	g_pulses.erase(std::remove_if(g_pulses.begin(), g_pulses.end(),
		[now](const auto &p) { return now >= p.second; }), g_pulses.end());

	for (INT32 k = 0; k < NUMKEYS; k++)
	{
		if (want[k] != g_pressed[k])
		{
			post_key(k, want[k]);
			g_pressed[k] = want[k];
		}
	}
}

void release_all()
{
	g_fingers.clear();
	g_pulses.clear();
	sync_keys();
}

// Menus ---------------------------------------------------------------------

void base_from_pixels(float px, float py, float &bx, float &by)
{
	const float dupx = (float)vid.dupx, dupy = (float)vid.dupy;
	bx = (px - (W() - BASEVIDWIDTH * dupx) * 0.5f) / dupx;
	by = (py - (H() - BASEVIDHEIGHT * dupy) * 0.5f) / dupy;
}

bool item_is_cvar(INT32 i)
{
	if (!currentMenu || i < 0 || i >= currentMenu->numitems)
		return false;
	return (currentMenu->menuitems[i].status & IT_TYPE) == IT_CVAR
		&& (currentMenu->menuitems[i].status & IT_CVARTYPE) != IT_CV_STRING;
}

void menu_tap(float px, float py)
{
	if (!menuactive)
	{
		// Title screen, intermission, cutscenes: a tap is "any input".
		pulse('a');
		return;
	}

	float bx, by;
	base_from_pixels(px, py, bx, by);
	I_OutputMsg("touch: tap px=%.0f,%.0f base=%.1f,%.1f hits=%d itemOn=%d dup=%d,%d vid=%dx%d\n",
		px, py, bx, by, (int)g_hits.size(), itemOn, vid.dupx, vid.dupy, vid.width, vid.height);
	for (const MenuHit &m : g_hits)
		I_OutputMsg("touch:   hit item %d at %d,%d %dx%d\n", m.item, m.x, m.y, m.w, m.h);
	for (const MenuHit &m : g_hits)
	{
		if (bx < m.x || bx >= m.x + m.w || by < m.y || by >= m.y + m.h)
			continue;
		if (m.item >= TOUCH_VKB_BASE)
		{
			// Virtual keyboard key: put the cursor there and press it.
			const INT32 k = m.item - TOUCH_VKB_BASE;
			menutyping.keyboardy = k / 64;
			menutyping.keyboardx = k % 64;
			menutyping.keyboardtyping = false;
			pulse('a');
			return;
		}
		if (m.item != itemOn)
		{
			itemOn = m.item;
			S_StartSound(NULL, sfx_s3k5b);
		}
		else if (item_is_cvar(m.item))
		{
			pulse(bx >= BASEVIDWIDTH / 2 ? KEY_RIGHTARROW : KEY_LEFTARROW);
		}
		else
		{
			pulse('a');
		}
		return;
	}
}

void menu_swipe(Finger &f)
{
	if (!cv_touch_menuswipe.value)
		return;
	const float step = 0.09f * H();
	const float dx = f.x - f.swx, dy = f.y - f.swy;
	if (std::fabs(dx) < step && std::fabs(dy) < step)
		return;
	f.swiped = true;
	if (std::fabs(dy) >= std::fabs(dx))
		pulse(dy > 0 ? KEY_UPARROW : KEY_DOWNARROW); // drag content: finger down = previous item
	else
		pulse(dx > 0 ? KEY_LEFTARROW : KEY_RIGHTARROW);
	f.swx = f.x;
	f.swy = f.y;
}

void toggle_keyboard()
{
	if (SDL_IsTextInputActive())
		SDL_StopTextInput();
	else
		SDL_StartTextInput();
}

// Drawing -------------------------------------------------------------------

INT32 trans_flags()
{
	INT32 op = std::clamp<INT32>(cv_touch_opacity.value, 1, 10);
	return (op >= 10) ? 0 : ((10 - op) << V_ALPHASHIFT);
}

// Base-space drawing snapped to the top-left corner, so 1 base unit is
// exactly vid.dupx pixels from the screen's left/top edge.
void draw_patch_centered(float px, float py, float d, patch_t *p, INT32 flags)
{
	if (!p)
		return;
	const fixed_t dup = vid.dupx;
	const INT32 pw = SHORT(p->width), ph = SHORT(p->height);
	const fixed_t scale = FloatToFixed(d / (pw * (float)dup));
	const fixed_t x = FloatToFixed(px / dup) - FixedMul((pw / 2 - SHORT(p->leftoffset)) * FRACUNIT, scale);
	const fixed_t y = FloatToFixed(py / dup) - FixedMul((ph / 2 - SHORT(p->topoffset)) * FRACUNIT, scale);
	V_DrawFixedPatch(x, y, scale, V_SNAPTOLEFT | V_SNAPTOTOP | flags, p, NULL);
}

void draw_label(float px, float py, const char *text, INT32 flags)
{
	if (!text || !cv_touch_labels.value)
		return;
	V_DrawCenteredThinString((INT32)(px / vid.dupx), (INT32)(py / vid.dupy),
		V_SNAPTOLEFT | V_SNAPTOTOP | flags, text);
}

bool key_held(INT32 k) { return g_pressed[k]; }



} // namespace

extern "C" {

boolean I_TouchActive(void)
{
	return overlay_visible();
}

boolean I_TouchOwnsInput(void)
{
	return cv_touch_controls.value != 0 && g_last_touch != 0 && g_last_touch >= g_last_other;
}

void I_TouchNotifyOtherInput(void)
{
	g_last_other = SDL_GetTicks();
	if (cv_touch_controls.value == 2 && !g_fingers.empty())
		release_all();
}

void I_TouchEvent(INT32 type, INT64 id, float nx, float ny)
{
	if (cv_touch_controls.value == 0)
	{
		if (!g_fingers.empty())
			release_all();
		return;
	}

	g_last_touch = SDL_GetTicks();
	if (g_layout.empty() || g_menu_layout != in_menu_context())
		build_layout();

	const float x = nx * W(), y = ny * H();
	auto it = std::find_if(g_fingers.begin(), g_fingers.end(), [id](const Finger &f) { return f.id == id; });

	switch (type)
	{
		case TOUCH_DOWN:
		{
			Finger f = {id, x, y, x, y, x, y, SDL_GetTicks(), false, false};
			for (const Element &e : g_layout)
			{
				if (hit(e, x, y, 1.15f))
				{
					f.on_element = true;
					if (e.kind == Kind::keyboard)
					{
						toggle_keyboard();
						f.on_element = false;
						f.swiped = true; // consume
					}
					break;
				}
			}
			if (it != g_fingers.end())
				*it = f;
			else
				g_fingers.push_back(f);
			break;
		}
		case TOUCH_MOVE:
			if (it == g_fingers.end())
				return;
			it->x = x;
			it->y = y;
			if (!it->on_element && in_menu_context())
				menu_swipe(*it);
			break;
		case TOUCH_UP:
			if (it == g_fingers.end())
				return;
			if (!it->on_element && !it->swiped && in_menu_context()
				&& SDL_GetTicks() - it->downtime < 500)
			{
				menu_tap(it->x, it->y);
			}
			g_fingers.erase(it);
			break;
	}

	sync_keys();
}

void I_TouchTicker(void)
{
	if (!g_pulses.empty())
		sync_keys();
}

void I_TouchRegisterMenuItem(INT32 item, INT32 x, INT32 y, INT32 w, INT32 h)
{
	g_hits_building.push_back({item, x, y, w, h});
}

void I_TouchDrawer(void)
{
	// Menu hit boxes collected while this frame's menu was drawn.
	g_hits.swap(g_hits_building);
	g_hits_building.clear();
	if (!menuactive)
		g_hits.clear();

	I_TouchTicker();

	if (SDL_IsTextInputActive() && !(chat_on || con_destlines > 0 || menutyping.active))
		SDL_StopTextInput();

	if (!overlay_visible())
		return;

	build_layout();
	const INT32 trans = trans_flags();

	for (const Element &e : g_layout)
	{
		switch (e.kind)
		{
			case Kind::dpad:
			{
				const float off = e.d * 0.30f, bd = e.d * 0.30f;
				struct { float dx, dy; patch_t **p; INT32 k; } arrows[] = {
					{0, -off, kp_button_up[0], KEY_UPARROW},
					{0, off, kp_button_down[0], KEY_DOWNARROW},
					{-off, 0, kp_button_left[0], KEY_LEFTARROW},
					{off, 0, kp_button_right[0], KEY_RIGHTARROW},
				};
				for (auto &a : arrows)
				{
					const bool down = key_held(a.k);
					draw_patch_centered(e.cx + a.dx, e.cy + a.dy, bd * (down ? 0.92f : 1.f), a.p[down ? 1 : 0], down ? 0 : trans);
				}
				break;
			}
			case Kind::button:
			case Kind::keyboard:
			{
				const bool down = e.kind == Kind::button && key_held(e.key);
				const INT32 t = (down || e.primary) ? std::max(0, trans - (2 << V_ALPHASHIFT)) : trans;
				draw_patch_centered(e.cx, e.cy, e.d * (down ? 0.92f : 1.f), e.patch[down ? 1 : 0], down ? 0 : t);
				const float ly = e.cy + e.d * 0.52f;
				const bool above = ly + 9.f * vid.dupy > H();
				draw_label(e.cx, above ? e.cy - e.d * 0.52f - 8.f * vid.dupy : ly, e.label, (down ? V_YELLOWMAP : 0) | t);
				break;
			}
		}
	}
}

} // extern "C"

#else // not Android: compiled out

extern "C" {
void I_TouchEvent(INT32, INT64, float, float) {}
void I_TouchNotifyOtherInput(void) {}
void I_TouchDrawer(void) {}
void I_TouchTicker(void) {}
void I_TouchRegisterMenuItem(INT32, INT32, INT32, INT32, INT32) {}
boolean I_TouchActive(void) { return false; }
boolean I_TouchOwnsInput(void) { return false; }
}

#endif
