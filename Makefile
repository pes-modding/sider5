# For debug builds, use "debug=1" in command line. For instance,
# to build DLLs in debug mode: nmake dlls debug=1

CC=cl
LINK=link
RC=rc

!if "$(debug)"=="1"
EXTRA_CFLAGS=/DDEBUG
!else
EXTRA_CFLAGS=/DMYDLL_RELEASE_BUILD
!endif

LPZLIB=soft\zlib123-dll\lib
ZLIBDLL=soft\zlib123-dll\zlib1.dll

# 4731: warning about ebp modification
CFLAGS=/nologo /Od /EHsc /wd4731 $(EXTRA_CFLAGS)
LFLAGS=/NOLOGO
LIBS=user32.lib gdi32.lib comctl32.lib version.lib
LIBSDLL=pngdib.obj libpng.a zdll.lib $(LIBS)

LUAINC=/I soft\LuaJIT-2.0.5\src
LUALIBPATH=soft\LuaJIT-2.0.5\src
LUALIB=lua51.lib
LUADLL=lua51.dll
LUAJIT=luajit.exe

all: sider.exe sider.dll

sider.res: sider.rc
	$(RC) -r -fo sider.res sider.rc
sider_main.res: sider_main.rc sider.ico
	$(RC) -r -fo sider_main.res sider_main.rc

common.obj: common.cpp common.h
imageutil.obj: imageutil.cpp imageutil.h
version.obj: version.cpp
memlib.obj: memlib.h memlib_lua.h memlib.cpp
memlib_lua.h: memory.lua makememlibhdr.exe
	makememlibhdr.exe
makememlibhdr.exe: makememlibhdr.c
	$(CC) makememlibhdr.c

$(LUALIBPATH)\$(LUALIB):
	cd $(LUALIBPATH) && msvcbuild.bat

util.obj: util.asm
    ml64 /c util.asm

sider.obj: sider.cpp sider.h patterns.h common.h imageutil.h
sider.dll: sider.obj util.obj imageutil.obj version.obj common.obj memlib.obj sider.res $(LUALIBPATH)\$(LUALIB)
	$(LINK) $(LFLAGS) /out:sider.dll /DLL sider.obj util.obj imageutil.obj version.obj common.obj memlib.obj sider.res /LIBPATH:$(LUALIBPATH) $(LIBS) $(LUALIB) /LIBPATH:"$(LIB)"

sider.exe: main.obj sider.dll sider_main.res $(LUADLL)
	$(LINK) $(LFLAGS) /out:sider.exe main.obj sider_main.res $(LIBS) sider.lib /LIBPATH:"$(LIB)"

$(LUADLL): $(LUALIBPATH)\$(LUALIB)
	copy $(LUALIBPATH)\$(LUADLL) .
	copy $(LUALIBPATH)\$(LUAJIT) .

.cpp.obj:
	$(CC) $(CFLAGS) -c $(INC) $(LUAINC) $<

clean:
	del *.obj *.dll *.exp *.res *.lib *.exe *~ memlib_lua.h

clean-all: clean
    cd $(LUALIBPATH) && del /Q lua51.exp lua51.lib lua51.dll luajit.exe

