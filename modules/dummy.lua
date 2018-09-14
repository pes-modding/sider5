--[[
=========================

dummy module
Requires: sider.dll 5.1.0

Demonstrates usage of: "overlay_on" and "key_down" events

=========================
--]]

local m = {}
local PREV_VALUE_KEY = 0xbd
local NEXT_VALUE_KEY = 0xbb

local text = ""
local lines_count = 0

function m.overlay_on(ctx)
    return string.format("Press [+][-] buttons to add/remove text%s", text)
end

function m.key_down(ctx, vkey)
    if vkey == PREV_VALUE_KEY then
        if lines_count > 0 then
            lines_count = lines_count - 1
            local t = {}
            for i=1,lines_count do t[i] = "\n" .. tostring(i) .. " some text" end
            text = table.concat(t)
        end
    elseif vkey == NEXT_VALUE_KEY then
        if lines_count < 100 then
            lines_count = lines_count + 1
            local t = {}
            for i=1,lines_count do t[i] = "\n" .. tostring(i) .. " some text" end
            text = table.concat(t)
        end
    end
end

function m.init(ctx)
    -- register for events
    ctx.register("overlay_on", m.overlay_on)
    ctx.register("key_down", m.key_down)
end

return m
