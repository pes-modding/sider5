--[[
=========================

camera module v1.1
Game research by: nesa24
Requires: sider.dll 5.0.1

=========================
--]]

local m = {}
local hex = memory.hex

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

function m.init(ctx)
    local settings = load_ini(ctx, "camera.ini")

    -- find the base address of the block of camera settings
    local pattern = "\x84\xc0\x41\x0f\x45\xfe\xf3\x0f\x10\x5b\x0c"
    local loc = memory.find(pattern)
    if not loc then
        log("problem: unable to find code pattern. No tweaks done")
        return
    end
    loc = loc + #pattern
    local rel_offset = memory.unpack("i32", memory.read(loc + 3, 4))
    local base_addr = loc + rel_offset + 7
    log(string.format("Camera block base address: %s", hex(base_addr)))

    -- apply settings
    for name,value in pairs(settings) do
        local entry = game_info[name]
        if entry then
            offset, format, len = unpack(entry)
            local addr = base_addr + offset
            local old_value = memory.unpack(format, memory.read(addr, len))
            memory.write(addr, memory.pack(format, value))
            local new_value = memory.unpack(format, memory.read(addr, len))
            log(string.format("%s: changed at %s: %s --> %s",
                name, hex(addr), old_value, new_value))
        end
    end
end

return m
