-- read tracer

local m = {}
local hex = memory.hex

function m.read(ctx, filename, addr, len, total_size, offset)
    local ts = total_size and hex(total_size) or "unknown"
    local offs = offset and hex(offset) or "unknown"
    log(string.format("%s: (buffer:%s, len:%s) offset:%s, total:%s bytes", filename, addr, hex(len), offs, ts))
end

function m.init(ctx)
    ctx.register("livecpk_read", m.read)
end

return m
