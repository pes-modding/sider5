-- Magic by nesa24
--[[

Fur [ FackYouReplay]
opcode address = 7F61DEDCD63
original opcode 41C6450804 (mov byte ptr [r13+08],04)
patched opcode 41C6450806 (mov byte ptr [r13+08],06)

Camera [ FLOAT VALUES USED IN CAMERA POSSITION CALCULATION]
Wide-Custom...
Zoom 19.04 offset=7F61046BA54
Setting zoom to 25 zoom out
10 zoomes in

--]]

local m = {}
m.camera_zoom = 25

local hex = memory.tohexstring

function m.init(ctx)
    -- log process section information
    local pi = memory.get_process_info()
    log(string.format("process base: %s", hex(pi.base)))
    for i,sec in ipairs(pi.sections) do
        log(string.format("process section: %s --> [%s - %s]", sec.name, hex(sec.start), hex(sec.finish)))
    end

    -- Disable replays.
    -- Find location in memory by searching all sections of PES2019.exe for a specific pattern
    local addr, section = memory.find("\x41\xc6\x45\x08\x04")
    if addr then
        memory.write(addr, "\x41\xc6\x45\x08\x06")
        log(string.format("Replays patched at %s (section: %s [%s - %s])",
            hex(addr), section.name, hex(section.start), hex(section.finish)))
    end

    -- Alter camera zoom.
    -- Instead of searching, use a known memory offset
    -- Relative to base addr, the zoom float is at: 0x1beba54
    local addr = pi.base + 0x1beba54
    local old_value = memory.unpack("f", memory.read(addr, 4))
    memory.write(addr, memory.pack("f", m.camera_zoom))
    local new_value = memory.unpack("f", memory.read(addr, 4))
    log(string.format("Camera zoom changed at %s: %0.3f --> %0.3f",
        hex(addr), old_value, new_value))
end

return m
