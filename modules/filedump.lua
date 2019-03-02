-- Dump a file to disk, after the game read it

local m = {}

local data_chunks = {}

function m.read(ctx, filename, addr, len, total_size, offset)
    if filename == "shaders\\dx11\\GrModelShaders_dx11.fsop" then
        -- addr is actually a pointer to data in memory, so if we want
        -- to use this data later, we need to make a copy of it now:
        local bytes = memory.read(addr, len)

        -- accumulate data chunks in a table, in case the file is large and gets loaded in mulitiple reads
        data_chunks[#data_chunks + 1] = bytes

        if offset + len >= total_size then
            -- got everything: now save all chunks
            local f = assert(io.open(ctx.sider_dir .. "GrModelShaders_dx11.fsop", "wb"))
            for i,chunk in ipairs(data_chunks) do
                f:write(chunk)
            end
            f:close()

            -- release memory held by chunks table
            data_chunks = {}
        end
    end
end

function m.init(ctx)
    ctx.register("livecpk_read", m.read)
end

return m
