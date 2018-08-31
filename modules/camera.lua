--[[
=========================

camera module v1.0
Game research by: nesa24
Requires: sider.dll 5.0.1

=========================
--]]

local m = {}
local hex = memory.tohexstring

local game_info = {
    --camera_range_zoom   = { 0x1beba54, "f", 4},   --> default: 19.05
    --camera_range_height = { 0x1beba58, "f", 4},   --> default: 0.3
    --camera_range_angle  = { 0x1beba70, "f", 4},   --> default: 1.35

    camera_range_zoom   = { 0x21f7ef4, "f", 4},   --> default: 19.05
    camera_range_height = { 0x21f7ef8, "f", 4},   --> default: 0.3
    camera_range_angle  = { 0x21f7f10, "f", 4},   --> default: 1.35
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
    local pi = memory.get_process_info()
    log(string.format("process base: %s", hex(pi.base)))

    local settings = load_ini(ctx, "camera.ini")

    -- apply settings
    for name,value in pairs(settings) do
        local entry = game_info[name]
        if entry then
            local rva, format, len = unpack(entry)
            local addr = pi.base + rva
            local old_value = memory.unpack(format, memory.read(addr, len))
            memory.write(addr, memory.pack(format, value))
            local new_value = memory.unpack(format, memory.read(addr, len))
            log(string.format("%s: changed at %s: %s --> %s",
                name, hex(addr), old_value, new_value))
        end
    end
end

return m
