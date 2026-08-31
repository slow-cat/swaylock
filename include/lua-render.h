#ifndef _SWAYLOCK_LUA_RENDER_H
#define _SWAYLOCK_LUA_RENDER_H

#include <stdbool.h>
#include <cairo/cairo.h>

struct lua_renderer;
struct swaylock_state;
struct swaylock_surface;

struct lua_renderer *lua_renderer_create(const char *path);
void lua_renderer_destroy(struct lua_renderer *renderer);
bool lua_renderer_draw(struct lua_renderer *renderer, cairo_t *cairo,
		int width, int height, const struct swaylock_surface *surface);

#endif
