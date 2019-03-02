-- zlib test module

local m = {}

function m.init(ctx)
    local data = "hello, world!"
    local compressed = zlib.compress(data)
    log(string.format("compressed: %s", memory.hex(compressed)))
    local uncompressed = zlib.uncompress(compressed)
    log(string.format("uncompressed: %s", uncompressed))
    assert(data == uncompressed)

    -- test: bogus compressed data
    local result,err = zlib.uncompress("plaintext")
    log(string.format("expected problem:: result: %s, error: %s", result, err))

    -- test: uncompressed buffer too small
    local result,err = zlib.uncompress(compressed, 4)
    log(string.format("expected problem:: result: %s, error: %s", result, err))
end

return m
