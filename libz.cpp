#define UNICODE

#include <zlib.h>

#include "common.h"
#include "libz.h"
#include "sider.h"

static int libz_compress(lua_State *L)
{
    size_t len = 0;
    const char *data = luaL_checklstring(L, 1, &len);
    if (!data || len == 0) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, "data cannot be of zero-length");
        return 2;
    }

    uLongf dest_len;
    BYTE* dest;

    dest_len = len*3;
    dest = (BYTE*)malloc(dest_len); // big buffer just in case;
    int retval = compress(dest, &dest_len, (const Bytef*)data, len);
    if (retval != Z_OK) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushfstring(L, "compression failed with error code: %d", retval);
        free(dest);
        return 2;
    }

    lua_pop(L, 1);
    lua_pushlstring(L, (char*)dest, dest_len);
    free(dest);
    return 1;
}

static int libz_uncompress(lua_State *L)
{
    size_t len = 0;
    int uncompressed_len = 0;
    const char *data = luaL_checklstring(L, 1, &len);
    if (lua_gettop(L) > 1) {
        uncompressed_len = luaL_checkint(L, 2);
    }
    if (!data || len == 0) {
        lua_pop(L, lua_gettop(L));
        lua_pushnil(L);
        lua_pushstring(L, "compressed data cannot be of zero-length");
        return 2;
    }

    uLongf dest_len;
    BYTE* dest;

    dest_len = (uncompressed_len > 0) ? uncompressed_len : len*3;
    dest = (BYTE*)malloc(dest_len);
    int retval = uncompress(dest, &dest_len, (const Bytef*)data, len);
    if (retval != Z_OK) {
        lua_pop(L, lua_gettop(L));
        lua_pushnil(L);
        lua_pushfstring(L, "decompression failed with error code: %d", retval);
        free(dest);
        return 2;
    }

    lua_pop(L, lua_gettop(L));
    lua_pushlstring(L, (char*)dest, dest_len);
    free(dest);
    return 1;
}

void init_z_lib(lua_State *L)
{
    lua_newtable(L);
    lua_pushstring(L, "compress");
    lua_pushcclosure(L, libz_compress, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "uncompress");
    lua_pushcclosure(L, libz_uncompress, 0);
    lua_settable(L, -3);
}

