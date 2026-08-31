#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <cairo/cairo.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <luajit.h>
#include "log.h"
#include "lua-render.h"
#include "swaylock.h"

#define CAIRO_CONTEXT_METATABLE "swaylock.cairo"
#define LUA_HOOK_INSTRUCTION_INTERVAL 10000
#define LUA_DRAW_TIMEOUT_NS 250000000L

struct lua_renderer {
	lua_State *lua;
	int draw_ref;
};

struct lua_cairo_context {
	cairo_t *cairo;
};

static cairo_t *check_cairo(lua_State *lua) {
	struct lua_cairo_context *context =
		luaL_checkudata(lua, 1, CAIRO_CONTEXT_METATABLE);
	if (!context->cairo) {
		luaL_error(lua, "Cairo context is no longer valid");
	}
	return context->cairo;
}

static int lua_cairo_save(lua_State *lua) {
	cairo_save(check_cairo(lua));
	return 0;
}

static int lua_cairo_restore(lua_State *lua) {
	cairo_restore(check_cairo(lua));
	return 0;
}

static int lua_cairo_set_source_rgb(lua_State *lua) {
	cairo_set_source_rgb(check_cairo(lua), luaL_checknumber(lua, 2),
		luaL_checknumber(lua, 3), luaL_checknumber(lua, 4));
	return 0;
}

static int lua_cairo_set_source_rgba(lua_State *lua) {
	cairo_set_source_rgba(check_cairo(lua), luaL_checknumber(lua, 2),
		luaL_checknumber(lua, 3), luaL_checknumber(lua, 4),
		luaL_checknumber(lua, 5));
	return 0;
}

static int lua_cairo_set_line_width(lua_State *lua) {
	cairo_set_line_width(check_cairo(lua), luaL_checknumber(lua, 2));
	return 0;
}

static int lua_cairo_paint(lua_State *lua) {
	cairo_paint(check_cairo(lua));
	return 0;
}

static int lua_cairo_rectangle(lua_State *lua) {
	cairo_rectangle(check_cairo(lua), luaL_checknumber(lua, 2),
		luaL_checknumber(lua, 3), luaL_checknumber(lua, 4),
		luaL_checknumber(lua, 5));
	return 0;
}

static int lua_cairo_arc(lua_State *lua) {
	cairo_arc(check_cairo(lua), luaL_checknumber(lua, 2),
		luaL_checknumber(lua, 3), luaL_checknumber(lua, 4),
		luaL_checknumber(lua, 5), luaL_checknumber(lua, 6));
	return 0;
}

static int lua_cairo_move_to(lua_State *lua) {
	cairo_move_to(check_cairo(lua), luaL_checknumber(lua, 2),
		luaL_checknumber(lua, 3));
	return 0;
}

static int lua_cairo_line_to(lua_State *lua) {
	cairo_line_to(check_cairo(lua), luaL_checknumber(lua, 2),
		luaL_checknumber(lua, 3));
	return 0;
}

static int lua_cairo_curve_to(lua_State *lua) {
	cairo_curve_to(check_cairo(lua), luaL_checknumber(lua, 2),
		luaL_checknumber(lua, 3), luaL_checknumber(lua, 4),
		luaL_checknumber(lua, 5), luaL_checknumber(lua, 6),
		luaL_checknumber(lua, 7));
	return 0;
}

static int lua_cairo_close_path(lua_State *lua) {
	cairo_close_path(check_cairo(lua));
	return 0;
}

static int lua_cairo_new_path(lua_State *lua) {
	cairo_new_path(check_cairo(lua));
	return 0;
}

static int lua_cairo_fill(lua_State *lua) {
	cairo_fill(check_cairo(lua));
	return 0;
}

static int lua_cairo_fill_preserve(lua_State *lua) {
	cairo_fill_preserve(check_cairo(lua));
	return 0;
}

static int lua_cairo_stroke(lua_State *lua) {
	cairo_stroke(check_cairo(lua));
	return 0;
}

static int lua_cairo_translate(lua_State *lua) {
	cairo_translate(check_cairo(lua), luaL_checknumber(lua, 2),
		luaL_checknumber(lua, 3));
	return 0;
}

static int lua_cairo_scale(lua_State *lua) {
	cairo_scale(check_cairo(lua), luaL_checknumber(lua, 2),
		luaL_checknumber(lua, 3));
	return 0;
}

static int lua_cairo_rotate(lua_State *lua) {
	cairo_rotate(check_cairo(lua), luaL_checknumber(lua, 2));
	return 0;
}

static int lua_cairo_select_font_face(lua_State *lua) {
	const char *family = luaL_checkstring(lua, 2);
	cairo_font_slant_t slant = luaL_optinteger(lua, 3, CAIRO_FONT_SLANT_NORMAL);
	cairo_font_weight_t weight = luaL_optinteger(lua, 4, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_select_font_face(check_cairo(lua), family, slant, weight);
	return 0;
}

static int lua_cairo_set_font_size(lua_State *lua) {
	cairo_set_font_size(check_cairo(lua), luaL_checknumber(lua, 2));
	return 0;
}

static int lua_cairo_show_text(lua_State *lua) {
	cairo_show_text(check_cairo(lua), luaL_checkstring(lua, 2));
	return 0;
}

static int lua_cairo_text_extents(lua_State *lua) {
	cairo_text_extents_t extents;
	cairo_text_extents(check_cairo(lua), luaL_checkstring(lua, 2), &extents);
	lua_pushnumber(lua, extents.width);
	lua_pushnumber(lua, extents.height);
	lua_pushnumber(lua, extents.x_bearing);
	lua_pushnumber(lua, extents.y_bearing);
	lua_pushnumber(lua, extents.x_advance);
	lua_pushnumber(lua, extents.y_advance);
	return 6;
}

static int lua_cairo_font_extents(lua_State *lua) {
	cairo_font_extents_t extents;
	cairo_font_extents(check_cairo(lua), &extents);
	lua_pushnumber(lua, extents.ascent);
	lua_pushnumber(lua, extents.descent);
	lua_pushnumber(lua, extents.height);
	lua_pushnumber(lua, extents.max_x_advance);
	lua_pushnumber(lua, extents.max_y_advance);
	return 5;
}

static const luaL_Reg cairo_methods[] = {
	{"save", lua_cairo_save},
	{"restore", lua_cairo_restore},
	{"set_source_rgb", lua_cairo_set_source_rgb},
	{"set_source_rgba", lua_cairo_set_source_rgba},
	{"set_line_width", lua_cairo_set_line_width},
	{"paint", lua_cairo_paint},
	{"rectangle", lua_cairo_rectangle},
	{"arc", lua_cairo_arc},
	{"move_to", lua_cairo_move_to},
	{"line_to", lua_cairo_line_to},
	{"curve_to", lua_cairo_curve_to},
	{"close_path", lua_cairo_close_path},
	{"new_path", lua_cairo_new_path},
	{"fill", lua_cairo_fill},
	{"fill_preserve", lua_cairo_fill_preserve},
	{"stroke", lua_cairo_stroke},
	{"translate", lua_cairo_translate},
	{"scale", lua_cairo_scale},
	{"rotate", lua_cairo_rotate},
	{"select_font_face", lua_cairo_select_font_face},
	{"set_font_size", lua_cairo_set_font_size},
	{"show_text", lua_cairo_show_text},
	{"text_extents", lua_cairo_text_extents},
	{"font_extents", lua_cairo_font_extents},
	{NULL, NULL},
};

static void register_cairo_context(lua_State *lua) {
	luaL_newmetatable(lua, CAIRO_CONTEXT_METATABLE);
	lua_pushvalue(lua, -1);
	lua_setfield(lua, -2, "__index");
	luaL_register(lua, NULL, cairo_methods);
	lua_pop(lua, 1);
}

static void remove_unsafe_globals(lua_State *lua) {
	const char *globals[] = {
		"debug", "dofile", "io", "jit", "load", "loadfile", "loadstring",
		"os", "package", "require", NULL,
	};
	for (size_t i = 0; globals[i]; ++i) {
		lua_pushnil(lua);
		lua_setglobal(lua, globals[i]);
	}
}

static struct timespec draw_deadline;

static void drawing_timeout(lua_State *lua, lua_Debug *debug) {
	(void)debug;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (now.tv_sec > draw_deadline.tv_sec ||
			(now.tv_sec == draw_deadline.tv_sec &&
			now.tv_nsec > draw_deadline.tv_nsec)) {
		luaL_error(lua, "drawing exceeded the time limit");
	}
}

static const char *auth_state_name(enum auth_state state) {
	switch (state) {
	case AUTH_STATE_VALIDATING:
		return "verifying";
	case AUTH_STATE_INVALID:
		return "wrong";
	case AUTH_STATE_IDLE:
	default:
		return "idle";
	}
}

static const char *input_state_name(enum input_state state) {
	switch (state) {
	case INPUT_STATE_CLEAR:
		return "clear";
	case INPUT_STATE_LETTER:
		return "letter";
	case INPUT_STATE_BACKSPACE:
		return "backspace";
	case INPUT_STATE_NEUTRAL:
		return "neutral";
	case INPUT_STATE_IDLE:
	default:
		return "idle";
	}
}

static void push_color(lua_State *lua, uint32_t color) {
	lua_createtable(lua, 4, 0);
	lua_pushnumber(lua, ((color >> 24) & 0xFF) / 255.0);
	lua_rawseti(lua, -2, 1);
	lua_pushnumber(lua, ((color >> 16) & 0xFF) / 255.0);
	lua_rawseti(lua, -2, 2);
	lua_pushnumber(lua, ((color >> 8) & 0xFF) / 255.0);
	lua_rawseti(lua, -2, 3);
	lua_pushnumber(lua, (color & 0xFF) / 255.0);
	lua_rawseti(lua, -2, 4);
}

static void push_colorset(lua_State *lua, const struct swaylock_colorset *colors) {
	lua_createtable(lua, 0, 5);
	push_color(lua, colors->input);
	lua_setfield(lua, -2, "input");
	push_color(lua, colors->cleared);
	lua_setfield(lua, -2, "clear");
	push_color(lua, colors->caps_lock);
	lua_setfield(lua, -2, "caps_lock");
	push_color(lua, colors->verifying);
	lua_setfield(lua, -2, "verifying");
	push_color(lua, colors->wrong);
	lua_setfield(lua, -2, "wrong");
}

static void push_colors(lua_State *lua, const struct swaylock_colors *colors) {
	lua_createtable(lua, 0, 12);
	push_color(lua, colors->background);
	lua_setfield(lua, -2, "background");
	push_color(lua, colors->bs_highlight);
	lua_setfield(lua, -2, "backspace_highlight");
	push_color(lua, colors->key_highlight);
	lua_setfield(lua, -2, "key_highlight");
	push_color(lua, colors->caps_lock_bs_highlight);
	lua_setfield(lua, -2, "caps_lock_backspace_highlight");
	push_color(lua, colors->caps_lock_key_highlight);
	lua_setfield(lua, -2, "caps_lock_key_highlight");
	push_color(lua, colors->separator);
	lua_setfield(lua, -2, "separator");
	push_color(lua, colors->layout_background);
	lua_setfield(lua, -2, "layout_background");
	push_color(lua, colors->layout_border);
	lua_setfield(lua, -2, "layout_border");
	push_color(lua, colors->layout_text);
	lua_setfield(lua, -2, "layout_text");
	push_colorset(lua, &colors->inside);
	lua_setfield(lua, -2, "inside");
	push_colorset(lua, &colors->line);
	lua_setfield(lua, -2, "line");
	push_colorset(lua, &colors->ring);
	lua_setfield(lua, -2, "ring");
	push_colorset(lua, &colors->text);
	lua_setfield(lua, -2, "text");
}

struct lua_renderer *lua_renderer_create(const char *path) {
	struct lua_renderer *renderer = calloc(1, sizeof(*renderer));
	if (!renderer) {
		return NULL;
	}

	renderer->lua = luaL_newstate();
	if (!renderer->lua) {
		free(renderer);
		return NULL;
	}
	luaL_openlibs(renderer->lua);
	luaJIT_setmode(renderer->lua, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_OFF);
	remove_unsafe_globals(renderer->lua);
	register_cairo_context(renderer->lua);

	if (luaL_loadfile(renderer->lua, path) != 0 ||
			lua_pcall(renderer->lua, 0, 1, 0) != 0) {
		swaylock_log(LOG_ERROR, "Failed to load Lua drawing script: %s",
			lua_tostring(renderer->lua, -1));
		lua_renderer_destroy(renderer);
		return NULL;
	}

	if (!lua_isfunction(renderer->lua, -1)) {
		swaylock_log(LOG_ERROR, "Lua drawing script must return a function");
		lua_renderer_destroy(renderer);
		return NULL;
	}

	renderer->draw_ref = luaL_ref(renderer->lua, LUA_REGISTRYINDEX);
	return renderer;
}

void lua_renderer_destroy(struct lua_renderer *renderer) {
	if (!renderer) {
		return;
	}
	if (renderer->lua) {
		lua_close(renderer->lua);
	}
	free(renderer);
}

bool lua_renderer_draw(struct lua_renderer *renderer, cairo_t *cairo,
		int width, int height, const struct swaylock_surface *surface) {
	if (!renderer) {
		return true;
	}

	lua_State *lua = renderer->lua;
	lua_rawgeti(lua, LUA_REGISTRYINDEX, renderer->draw_ref);
	struct lua_cairo_context *context = lua_newuserdata(lua, sizeof(*context));
	context->cairo = cairo;
	luaL_getmetatable(lua, CAIRO_CONTEXT_METATABLE);
	lua_setmetatable(lua, -2);
	lua_pushinteger(lua, width);
	lua_pushinteger(lua, height);

	const struct swaylock_state *state = surface->state;
	lua_createtable(lua, 0, 7);
	lua_pushstring(lua, auth_state_name(state->auth_state));
	lua_setfield(lua, -2, "auth");
	lua_pushstring(lua, input_state_name(state->input_state));
	lua_setfield(lua, -2, "input");
	lua_pushboolean(lua, state->xkb.caps_lock);
	lua_setfield(lua, -2, "caps_lock");
	lua_pushinteger(lua, state->failed_attempts);
	lua_setfield(lua, -2, "failed_attempts");
	lua_pushinteger(lua, state->highlight_start);
	lua_setfield(lua, -2, "highlight_start");
	lua_pushinteger(lua, surface->scale);
	lua_setfield(lua, -2, "scale");
	lua_pushstring(lua, surface->output_name ? surface->output_name : "");
	lua_setfield(lua, -2, "output");
	lua_pushinteger(lua, state->args.radius);
	lua_setfield(lua, -2, "radius");
	lua_pushinteger(lua, state->args.thickness);
	lua_setfield(lua, -2, "thickness");
	lua_pushstring(lua, state->args.font);
	lua_setfield(lua, -2, "font");
	lua_pushinteger(lua, state->args.font_size);
	lua_setfield(lua, -2, "font_size");
	lua_pushboolean(lua, state->args.show_caps_lock_text);
	lua_setfield(lua, -2, "show_caps_lock_text");
	lua_pushboolean(lua, state->args.show_caps_lock_indicator);
	lua_setfield(lua, -2, "show_caps_lock_indicator");
	lua_pushboolean(lua, state->args.show_failed_attempts);
	lua_setfield(lua, -2, "show_failed_attempts");
	lua_pushboolean(lua, state->args.indicator_idle_visible);
	lua_setfield(lua, -2, "indicator_idle_visible");
	push_colors(lua, &state->args.colors);
	lua_setfield(lua, -2, "colors");

	clock_gettime(CLOCK_MONOTONIC, &draw_deadline);
	draw_deadline.tv_nsec += LUA_DRAW_TIMEOUT_NS;
	if (draw_deadline.tv_nsec >= 1000000000L) {
		draw_deadline.tv_sec++;
		draw_deadline.tv_nsec -= 1000000000L;
	}
	lua_sethook(lua, drawing_timeout, LUA_MASKCOUNT | LUA_MASKLINE,
		LUA_HOOK_INSTRUCTION_INTERVAL);
	int result = lua_pcall(lua, 4, 0, 0);
	lua_sethook(lua, NULL, 0, 0);
	context->cairo = NULL;
	if (result != 0) {
		swaylock_log(LOG_ERROR, "Lua drawing failed: %s", lua_tostring(lua, -1));
		lua_pop(lua, 1);
		return false;
	}
	return true;
}
