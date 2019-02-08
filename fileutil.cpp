#include <windows.h>
#include "lua.hpp"
#include "sider.h"
#include "fileutil.h"
#include "fileutil_lua.h"

void init_fileutil(lua_State *L)
{
    int r = luaL_loadbuffer(L, fileutil_lua, strlen(fileutil_lua), "fileutil");
    if (r != 0) {
        const char *err = lua_tostring(L, -1);
        logu_("PROBLEM loading fileutil: %s. "
              "Skipping it\n", err);
        lua_pop(L, 1);
        return;
    }

    // run the module
    if (lua_pcall(L, 0, 1, 0) != 0) {
        const char *err = lua_tostring(L, -1);
        logu_("PROBLEM initializing fileutil: %s. "
              "Skipping it\n", err);
        lua_pop(L, 1);
        return;
    }
}

