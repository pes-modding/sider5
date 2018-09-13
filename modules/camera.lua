--[[
=========================

camera module v1.3
Game research by: nesa24
Requires: sider.dll 5.1.0

=========================
--]]

local m = {}
local hex = memory.hex
local base_addr
local settings

local PREV_PROP_KEY = 0x39
local NEXT_PROP_KEY = 0x30
local PREV_VALUE_KEY = 0xbd
local NEXT_VALUE_KEY = 0xbb

local overlay_curr = 1
local overlay_states = {
    { prv = "", curr_name = "zoom range", curr_prop = "camera_range_zoom", nxt = " >", decr = -0.1, incr = 0.1 },
    { prv = "< ", curr_name = "height range ", curr_prop = "camera_range_height", nxt = " >", decr = -0.05, incr = 0.05 },
    { prv = "< ", curr_name = "angle range ", curr_prop = "camera_range_angle", nxt = "", decr = -1, incr = 1 },
}

local game_info = {
    camera_range_zoom   = { 0x04, "f", 4},   --> default: 19.05
    camera_range_height = { 0x08, "f", 4},   --> default: 0.3
    camera_range_angle  = { 0x20, "f", 4},   --> default: 1.35
}

local function load_ini(ctx, filename)
    local t = {}
    for line in io.lines(ctx.sider_dir .. "\\" .. filename) do
        local name, value = string.match(line, "^([%w_]+)%s*=%s*([-%d.]+)")
        if name and value then
            t[name] = tonumber(value)
            log(string.format("Using setting: %s = %s", name, value))
        end
    end
    return t
end

local function apply_settings(log_it)
    if not base_addr then
        return
    end
    for name,value in pairs(settings) do
        local entry = game_info[name]
        if entry then
            offset, format, len = unpack(entry)
            local addr = base_addr + offset
            local old_value = memory.unpack(format, memory.read(addr, len))
            memory.write(addr, memory.pack(format, value))
            local new_value = memory.unpack(format, memory.read(addr, len))
            if log_it then
                log(string.format("%s: changed at %s: %s --> %s",
                    name, hex(addr), old_value, new_value))
            end
        end
    end
end

function m.set_teams(ctx, home, away)
    apply_settings(true)
end

function m.overlay_on(ctx)
    local s = overlay_states[overlay_curr]
    local curr_value = settings[s.curr_prop]
    return string.format("%s%s:%0.2f%s", s.prv, s.curr_name, curr_value, s.nxt)
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
        s = overlay_states[overlay_curr]
        settings[s.curr_prop] = settings[s.curr_prop] + s.incr;
        apply_settings()
    elseif vkey == PREV_VALUE_KEY then
        s = overlay_states[overlay_curr]
        settings[s.curr_prop] = settings[s.curr_prop] + s.decr;
        apply_settings()
    end
end

function m.init(ctx)
    -- find the base address of the block of camera settings
    local pattern = "\x84\xc0\x41\x0f\x45\xfe\xf3\x0f\x10\x5b\x0c"
    local loc = memory.search_process(pattern)
    if not loc then
        log("problem: unable to find code pattern. No tweaks done")
        return
    end
    loc = loc + #pattern
    local rel_offset = memory.unpack("i32", memory.read(loc + 3, 4))
    base_addr = loc + rel_offset + 7
    log(string.format("Camera block base address: %s", hex(base_addr)))

    settings = load_ini(ctx, "camera.ini")

    -- register for events
    ctx.register("set_teams", m.set_teams)
    ctx.register("overlay_on", m.overlay_on)
    ctx.register("key_down", m.key_down)
end

return m
