Sider 4 for Pro Evolution Soccer 2018
=====================================
Copyright (C) 2018 juce
Version 4.1.2



INTRODUCTION
------------

The "LiveCPK" feature makes it possible to replace game content
at run-time with content from files stored on disk, instead of
having to pack everything into CPK-archives. (This feature
is similar to Kitserver's AFS2FS and to FileLoader for earler
versions of PES).

If you know a little bit how to program, you can write your own
game logic using Sider's scripting engine, which uses Lua.
This requires some reading and understanding of how the game
works, but it's really not that hard ;-)
See scripting.txt - for detailed documentation on that.



HOW TO USE:
-----------

Run sider.exe, it will open a small window, which you can
minimize if you want, but do not close it.

Run the game.
Sider should automatically attach to the game process.

If you don't see the effects of Sider in the game, check the
sider.log file (in the same folder where sider.exe is) - it should
contain some helpful information on what went wrong.



SETTINGS (SIDER.INI)
--------------------

There are several settings you can set in sider.ini:


exe.name = "\PES2018.exe"
exe.name = "\PES2018patched.exe"

- this sets the pattern(s) that the Sider program will use
to identify which of the running processes is the game.
You can have multiple "exe.name" lines in your sider.ini,
which is useful, for example, if you have several exe
files with slightly different names that you use for
online/offline play.


livecpk.enabled = 1

- Turns on the LiveCPK functionality of Sider. See below for a more
detailed explanation in cpk.root option section.


debug = 0

- Setting this to values > 0 will make Sider output some additional information
into the log file (sider.log). This is useful primarily for troubleshooting.
Extra logging may slow the game down, so normally you would want to keep
this setting set to 0. (Defaults to 0: some info, but no extra output)


close.on.exit = 0

- If this setting is set to 1, then Sider will close itself, when the
game exits. This can be handy, if you use a batch file to start sider
automatically right before the game is launched.
(Defaults to 0: do not close)


start.minimized = 0

- If you set this to 1, then Sider will start with a minimized window.
Again, like the previous option, this setting can be helpful, if you
use a batch file to auto-start sider, just before the game launches.
(Defaults to 0: normal window)


cpk.root = "c:\cpk-roots\balls-root"
cpk.root = "c:\cpk-roots\kits-root"
cpk.root = ".\another-root\stadiums"

- Specifies root folder (or folders), where the game files are stored that
will be used for content replacing at run-time. It works like this:
For example, the game wants to load a file that is stored in some CPK, with
the relative path of "common/render/thumbnail/ball/ball_001.dds". Sider
will intercept that action and check if one of the root folders have this
file. If so, Sider will make the game read the content from that file instead
of using game's original content. If multiple roots are specified, then
they are checked in order that they are listed in sider.ini. As soon as there
is a filename match, the lookup stops. (So, higher root will win, if both of
them have the same file). You can use either absolute paths or relative.
Relative paths will be calculated relative to the folder where sider.exe is
located.


lua.enabled = 1

- This turns on/off the scripting support. Extension modules can be
written in Lua 5.1 (LuaJIT), using a subset of standard libraries and
also objects and events provides by sider. See "scripting.txt" file for
a programmer's guide to writing lua modules for sider.


lua.module = "kitrewrite.lua"
lua.module = "timeaccel.lua"

- Specifies the order in which the extension modules are loaded. These
modules must be in "modules" folder inside the sider root directory.



CREDITS:
--------
Game research: nesa24, juce, digitalfoxx
Programming: juce
Alpha testing: zlac, nesa24, Chuny, Hawke

