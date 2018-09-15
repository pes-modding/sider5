--[[
=========================

camera module
Game research by: nesa24
Requires: sider.dll 5.1.0

=========================
--]]

local m = {}
m.version = "1.4"
local hex = memory.hex
local settings

local RESTORE_KEY = 0x38
local PREV_PROP_KEY = 0x39
local NEXT_PROP_KEY = 0x30
local PREV_VALUE_KEY = 0xbd
local NEXT_VALUE_KEY = 0xbb

local overlay_curr = 1
local overlay_states = {
    { ui = "camera zoom range: %0.2f", prop = "camera_range_zoom", decr = -0.1, incr = 0.1, def = 19.05 },
    { ui = "camera height range: %0.2f", prop = "camera_range_height", decr = -0.05, incr = 0.05, def = 0.3 },
    { ui = "camera angle range: %0.2f", prop = "camera_range_angle", decr = -1, incr = 1, def = 1.35 },
    { ui = "replays: %s", prop = "replays", def = "on",
        nextf = function(v)
            return (v == "on") and "off" or "on"
        end,
        prevf = function(v)
            return (v == "on") and "off" or "on"
        end,
    },
}
local ui_lines = {}

local bases = {
    camera = nil,
    replays = nil,
}
local game_info = {
    camera_range_zoom   = { "camera", 0x04, "f", 4},   --> default: 19.05
    camera_range_height = { "camera", 0x08, "f", 4},   --> default: 0.3
    camera_range_angle  = { "camera", 0x20, "f", 4},   --> default: 1.35
    replays = { "replays", 0x04, "", 1, {on='\x04', off='\x06'}},
}

local function load_ini(ctx, filename)
    local t = {}
    for line in io.lines(ctx.sider_dir .. "\\" .. filename) do
        local name, value = string.match(line, "^([%w_]+)%s*=%s*([-%w%d.]+)")
        if name and value then
            value = tonumber(value) or value
            t[name] = value
            log(string.format("Using setting: %s = %s", name, value))
        end
    end
    return t
end

local function save_ini(ctx, filename)
    local f,err = io.open(ctx.sider_dir .. "\\" .. filename, "wt")
    if not f then
        log(string.format("PROBLEM saving settings: %s", tostring(err)))
        return
    end
    f:write(string.format("# Camera settings. Written by camera.lua " .. m.version .. "\n\n"))
    f:write(string.format("camera_range_zoom = %0.2f\n", settings.camera_range_zoom))
    f:write(string.format("camera_range_height = %0.2f\n", settings.camera_range_height))
    f:write(string.format("camera_range_angle = %0.2f\n", settings.camera_range_angle))
    f:write(string.format("replays = %s\n\n", settings.replays))
    f:close()
end

local function apply_settings(ctx, log_it, save_it)
    for name,value in pairs(settings) do
        local entry = game_info[name]
        if entry then
            local base_name, offset, format, len, value_mapping = unpack(entry)
            local base = bases[base_name]
            if base then
                if value_mapping then
                    value = value_mapping[value]
                end
                local addr = base + offset
                local old_value, new_value
                if format ~= "" then
                    old_value = memory.unpack(format, memory.read(addr, len))
                    memory.write(addr, memory.pack(format, value))
                    new_value = memory.unpack(format, memory.read(addr, len))
                else
                    old_value = memory.read(addr, len)
                    memory.write(addr, value)
                    new_value = memory.read(addr, len)
                end
                if log_it then
                    log(string.format("%s: changed at %s: %s --> %s",
                        name, hex(addr), old_value, new_value))
                end
            end
        end
    end
    if (save_it) then
        save_ini(ctx, "camera.ini")
    end
end

function m.set_teams(ctx, home, away)
    apply_settings(ctx, true)
end

function m.overlay_on(ctx)
    for i,v in ipairs(overlay_states) do
        local s = overlay_states[i]
        local setting = string.format(s.ui, settings[s.prop])
        if i == overlay_curr then
            ui_lines[i] = string.format("\n---> %s <---", setting)
        else
            ui_lines[i] = string.format("\n     %s", setting)
        end
    end
    return string.format("version %s\nKeys: [9][0] - choose setting, [-][+] - modify value, [8] - restore defaults%s",
        m.version, table.concat(ui_lines))
end

function m.key_down(ctx, vkey)
    if vkey == NEXT_PROP_KEY then
        if overlay_curr < #overlay_states then
            overlay_curr = overlay_curr + 1
        end
    elseif vkey == PREV_PROP_KEY then
        if overlay_curr > 1 then
            overlay_curr = overlay_curr - 1
        end
    elseif vkey == NEXT_VALUE_KEY then
        local s = overlay_states[overlay_curr]
        if s.incr ~= nil then
            settings[s.prop] = settings[s.prop] + s.incr
        elseif s.nextf ~= nil then
            settings[s.prop] = s.nextf(settings[s.prop])
        end
        apply_settings(ctx, false, true)
    elseif vkey == PREV_VALUE_KEY then
        local s = overlay_states[overlay_curr]
        if s.decr ~= nil then
            settings[s.prop] = settings[s.prop] + s.decr
        elseif s.prevf ~= nil then
            settings[s.prop] = s.prevf(settings[s.prop])
        end
        apply_settings(ctx, false, true)
    elseif vkey == RESTORE_KEY then
        for i,s in ipairs(overlay_states) do
            settings[s.prop] = s.def
        end
        apply_settings(ctx, false, true)
    end
end

function m.init(ctx)
    -- find the base address of the block of camera settings
    local pattern = "\x84\xc0\x41\x0f\x45\xfe\xf3\x0f\x10\x5b\x0c"
    local loc = memory.search_process(pattern)
    if loc then
        loc = loc + #pattern
        local rel_offset = memory.unpack("i32", memory.read(loc + 3, 4))
        bases.camera = loc + rel_offset + 7
        log(string.format("Camera block base address: %s", hex(bases.camera)))
    else
        log("problem: unable to find code pattern for camera block")
    end

    settings = load_ini(ctx, "camera.ini")

    -- find replays opcode
    local pattern = "\x41\xc6\x45\x08\x04"
    local loc = memory.search_process(pattern)
    if loc then
        bases.replays = loc
        log(string.format("Replays op-code address: %s", hex(bases.replays)))
    else
        log("problem: unable to find code pattern for replays switch")
    end

    -- register for events
    ctx.register("set_teams", m.set_teams)
    ctx.register("overlay_on", m.overlay_on)
    ctx.register("key_down", m.key_down)
end

return m
