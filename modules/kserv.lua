-- kserv.lua
-- Experiment with live kit modifications

local m = {}

m.version = "1.0"

-- some (partial) configs to rotate through
local kits = {
    { KitFile = "u0101p3", BackNumbersFile="epl_nav_back", LegNumbersFile="epl_nav_leg", ChestNumbersFile="", NameFontFile="epl_nav_name",
      ShirtColor1 = "#80cc80", ShirtColor2 = "#80cc80", UndershirtColor = "#80cc80", ShortsColor="#80cc80" },
    { KitFile = "u0100p1", BackNumbersFile="epl_whi_back", LegNumbersFile="epl_whi_leg", ChestNumbersFile="", NameFontFile="epl_whi_name",
      ShirtColor1 = "#cc6060", ShirtColor2 = "#cc6060", UndershirtColor = "#cc6060", ShortsColor="#000000" },
    { KitFile = "u0173p1", BackNumbersFile="epl_whi_back", LegNumbersFile="epl_nav_leg", ChestNumbersFile="", NameFontFile="epl_whi_name",
      ShirtColor1 = "#8080CC", ShirtColor2 = "#8080CC", UndershirtColor = "#8080CC", ShortsColor="#ffffff" },
    { KitFile = "u0101g1", BackNumbersFile="epl_whi_back", LegNumbersFile="epl_whi_leg", ChestNumbersFile="", NameFontFile="epl_whi_name",
      ShirtColor1 = "#404040", ShirtColor2 = "#404040", UndershirtColor = "#404040", ShortsColor="#404040" },
}
local next_kit = 0

local function t2s(t)
    local parts = {}
    for k,v in pairs(t) do
        parts[#parts + 1] = string.format("%s=%s", k, v)
    end
    table.sort(parts) -- sort alphabetically
    return string.format("{%s}", table.concat(parts,", "))
end

local function load_config(filename)
    local cfg = {}
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

function m.set_kits(ctx, home_info, away_info)
    log(string.format("home_info (team=%d): %s", ctx.home_team, t2s(home_info)))
    log(string.format("away_info (team=%d): %s", ctx.away_team, t2s(away_info)))

    -- test change: for Man City 1st kit, away: change back number to white, shorts number to red
    if ctx.away_team == 173 and away_info.kit_id == 0 then
        return nil, { BackNumbersFile = "epl_whi_back", LegNumbersFile = "epl_red_leg" }
    end
end

function m.key_down(ctx, vkey)
    if vkey == 0x36 then
        -- for home team: flip the number to the other leg
        local kit = ctx.kits.get(0)
        kit.ShortsNumberSide = (kit.ShortsNumberSide + 1) % 2
        ctx.kits.refresh(0, kit)
    elseif vkey == 0x37 then
        -- for home team: load next kit from our collection
        ctx.kits.refresh(0, kits[next_kit+1])
        next_kit = (next_kit + 1) % #kits
    elseif vkey == 0x38 then
        -- load France kit for away team
        local france_kit = load_config(ctx.sider_dir .. "content\\kit-server\\France\\config.txt")
        log(string.format("loaded: %s", t2s(france_kit)))
        ctx.kits.refresh(1, france_kit)
    end
end

function m.overlay_on(ctx)
    return "[6] - number flip, [7] - next kit, [8] - load France kit"
end

function m.init(ctx)
    ctx.register("overlay_on", m.overlay_on)
    ctx.register("key_down", m.key_down)
    ctx.register("set_kits", m.set_kits)
end

return m
