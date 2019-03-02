-- zlib test module

local m = {}

function m.init(ctx)
    local data = "hello, world!"
    local compressed = zlib.compress(data)
    log(string.format("compressed: %s", memory.hex(compressed)))
    local uncompressed = zlib.uncompress(compressed)
    log(string.format("uncompressed: %s", uncompressed))
    assert(data == uncompressed)
end

return m
