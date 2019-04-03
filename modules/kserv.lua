-- kserv.lua
-- Experimental kitserver with GDB

local m = {}

m.version = "1.1"

local kroot = ".\\content\\kit-server\\"
local kmap
local home_kits
local away_kits

local home_next_kit
local away_next_kit

local patterns = {
    ["\\(k%d+p%d+)%.ftex$"] = "KitFile",
    ["\\(k%d+p%d+_c)%.ftex$"] = "ChestNumbersFile",
    ["\\(k%d+p%d+_l)%.ftex$"] = "LegNumbersFile",
    ["\\(k%d+p%d+_b)%.ftex$"] = "BackNumbersFile",
    ["\\(k%d+p%d+_n)%.ftex$"] = "NameFontFile",
}
local ks_player_formats = {
    KitFile = "k%dp%d",
    ChestNumbersFile = "k%dp%d_c",
    LegNumbersFile = "k%dp%d_l",
    BackNumbersFile = "k%dp%d_b",
    NameFontFile = "k%dp%d_n",
}
local filename_keys = {
    "KitFile", "ChestNumbersFile", "LegNumbersFile",
    "BackNumbersFile", "NameFontFile",
}

local kfile_remap = {}

local function file_exists(filename)
    local f = io.open(filename)
    if f then
        f:close()
        return true
    end
end

local function t2s(t)
    local parts = {}
    for k,v in pairs(t) do
        parts[#parts + 1] = string.format("%s=%s", k, v)
    end
    table.sort(parts) -- sort alphabetically
    return string.format("{%s}", table.concat(parts,", "))
end

local function table_copy(t)
    local new_t = {}
    for k,v in pairs(t) do
        new_t[k] = v
    end
    return new_t
end

local function config_update_filenames(team_id, ord, ki)
    local path = kmap[team_id]
    if not path then
        return
    end
    local kit_path, kit_attrs = ki[1], ki[2]
    for _,k in ipairs(filename_keys) do
        local attr = kit_attrs[k]
        if attr and attr ~= "" then
            local fullpath = string.format("%s%s\\%s\\%s.ftex", kroot, path, kit_path, attr)
            log("checking: " .. fullpath)
            if file_exists(fullpath) then
                local fmt = ks_player_formats[k]
                if fmt then
                    local fkey = string.format(fmt, team_id, ord)
                    log("storing with fkey: " .. fkey)
                    kfile_remap[fkey] = fullpath
                    kit_attrs[k] = fkey
                end
            end
        end
    end
end

local function load_map(filename)
    local map = {}
    for line in io.lines(filename) do
        -- strip comment
        line = string.gsub(line, "#.*", "")
        tid, path = string.match(line, "%s*(%d+)%s*,%s*[\"]?([^\"]*)")
        tid = tonumber(tid)
        if tid and path then
            map[tid] = path
            log(string.format("tid: %d ==> content path: %s", tid, path))
        end
    end
    return map
end

local function load_config(filename)
    local cfg = {}
    local key, value
    local f = io.open(filename)
    if not f then
        return
    end
    for line in io.lines(filename) do
        -- strip comment
        line = string.gsub(line, ";.*", "")
        key, value = string.match(line, "%s*(%w+)%s*=%s*[\"]?([^\"]*)")
        value = tonumber(value) or value
        if key and value then
            cfg[key] = value
        end
    end
    return cfg
end

local function load_configs(team_id)
    local path = kmap[team_id]
    if not path then
        -- no kits for this team
        log(string.format("no kitserver kits for: %s", team_id))
        return
    end
    log(string.format("looking for configs for: %s", path))
    local t = {}
    for line in io.lines(kroot .. path .. "\\order.txt") do
        line = string.gsub(string.gsub(line,"%s*$", ""), "^%s*", "")
        local filename = kroot .. path .. "\\" .. line .. "\\config.txt"
        local cfg = load_config(filename)
        if cfg then
            t[#t + 1] = { line, cfg }
        else
            log("WARNING: unable to load kit config from: " .. filename)
        end
    end
    return t
end

local function dump_config(filename, t)
    local f = assert(io.open(filename,"wt"))
    f:write("; Kit config dumped by kserv.lua\n\n")
    local keys = {}
    for k,v in pairs(t) do
        keys[#keys + 1] = k
    end
    table.sort(keys)
    for i,k in ipairs(keys) do
        f:write(string.format("%s=%s\n", k, t[k]))
    end
    f:close()
end

function m.set_kits(ctx, home_info, away_info)
    log(string.format("home_info (team=%d): %s", ctx.home_team, t2s(home_info)))
    log(string.format("away_info (team=%d): %s", ctx.away_team, t2s(away_info)))
    --dump_config(string.format("%s%d-%s-config.txt", ctx.sider_dir, ctx.home_team, home_info.kit_id), home_info)
    --dump_config(string.format("%s%d-%s-config.txt", ctx.sider_dir, ctx.away_team, away_info.kit_id), away_info)

    -- see what kits are available
    home_kits = load_configs(ctx.home_team)
    away_kits = load_configs(ctx.away_team)

    -- initialize iterators
    home_next_kit = home_kits and 0 or nil
    away_next_kit = away_kits and 0 or nil
end

function m.make_key(ctx, filename)
    for patt,attr in pairs(patterns) do
        local fkey = string.match(filename, patt)
        if fkey then
            local key = kfile_remap[fkey]
            if key then
                log(string.format("fkey: {%s}, key: {%s}", fkey, key))
            end
            return key
        end
    end
    -- no key for this file
    return ""
end

function m.get_filepath(ctx, filename, key)
    if key ~= "" then
        return key
    end
end

function m.key_down(ctx, vkey)
    if vkey == 0x39 then -- next home kit
        home_kits = load_configs(ctx.home_team)
        if home_kits and #home_kits > 0 then
            -- advance the iter
            home_next_kit = (home_next_kit % #home_kits) + 1
            log("home_next_kit: " .. home_next_kit)
            local curr = home_kits[home_next_kit]
            local cfg = { curr[1], table_copy(curr[2]) }
            -- trick: mangle the filenames so that we can livecpk them later
            -- (only do that for files that actually exist in kitserver content)
            config_update_filenames(ctx.home_team, home_next_kit, cfg)
            -- trigger refresh
            ctx.kits.refresh(0, cfg[2])
        end
    elseif vkey == 0x30 then -- next away kit
        away_kits = load_configs(ctx.away_team)
        if away_kits and #away_kits > 0 then
            -- advance the iter
            away_next_kit = (away_next_kit % #away_kits) + 1
            log("away_next_kit: " .. away_next_kit)
            local curr = away_kits[away_next_kit]
            local cfg = { curr[1], table_copy(curr[2]) }
            -- trick: mangle the filenames so that we can livecpk them later
            -- (only do that for files that actually exist in kitserver content)
            config_update_filenames(ctx.away_team, away_next_kit, cfg)
            -- trigger refresh
            ctx.kits.refresh(1, cfg[2])
        end
    end
end

function m.overlay_on(ctx)
    return "[9] - home: next kitserver kit, [0] - away: next kitserver kit"
end

function m.init(ctx)
    if kroot:sub(1,1) == "." then
        kroot = ctx.sider_dir .. kroot
    end
    kmap = load_map(kroot .. "\\map.txt")
    ctx.register("overlay_on", m.overlay_on)
    ctx.register("key_down", m.key_down)
    ctx.register("set_kits", m.set_kits)
    ctx.register("livecpk_make_key", m.make_key)
    ctx.register("livecpk_get_filepath", m.get_filepath)
end

return m
