#ifndef SIDER_KITINFO_H
#define SIDER_KITINFO_H

#include <windows.h>

#include <lua.hpp>

static void str_to_rgb(BYTE *dst, char *src);
static void set_word_bits(void *dst, int value, int bit_from, int bit_to);
static int get_word_bits(void *dst, int bit_from, int bit_to);

void set_kit_info_from_lua_table(lua_State *L, int index, BYTE *dst, BYTE *radar_color);
void get_kit_info_to_lua_table(lua_State *L, int index, BYTE *src);

#endif
