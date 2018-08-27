Sider 5 for Pro Evolution Soccer 2019
=====================================
Copyright (C) 2019 juce
Version 5.0.1



INTRODUCTION
------------

Sider is a helper program fro Pro Evolution Soccer.
But you already knew that :-)



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


exe.name = "\PES2019.exe"

- this sets the pattern(s) that the Sider program will use
to identify which of the running processes is the game.
You can have multiple "exe.name" lines in your sider.ini,
which is useful, for example, if you have several exe
files with slightly different names that you use for
online/offline play.


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


match.minutes = 10

- force match time to this number of minutes.
You can set any time between 1 and 255 minutes, but don't go crazy.



CREDITS:
--------
Game research and programming: juce, nesa24
Example ball: barcerojas and digitalfoxx

