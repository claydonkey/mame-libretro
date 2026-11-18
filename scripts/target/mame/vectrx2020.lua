-- license:BSD-3-Clause
-- copyright-holders:Vectrx2020

---------------------------------------------------------------------------
--
--   vectrx2020.lua
--
--   Vector-only SUBTARGET for MAME
--   Use: make SUBTARGET=vectrx2020
--
---------------------------------------------------------------------------

---------------------------------------------------------------------------
-- Load device selector definitions from core scripts
---------------------------------------------------------------------------
 

local function selectors_get(path)
    local selector = ""
    for l in io.lines(path) do
        if l:sub(1, 3) == "--@" then
            local pos = l:find(",")
            selector = selector .. l:sub(pos + 1) .. "\n"
        end
    end
    return selector
end

local selectors =
        selectors_get(MAME_DIR .. "scripts/src/cpu.lua") ..
        selectors_get(MAME_DIR .. "scripts/src/sound.lua") ..
        selectors_get(MAME_DIR .. "scripts/src/video.lua") ..
        selectors_get(MAME_DIR .. "scripts/src/machine.lua") ..
        selectors_get(MAME_DIR .. "scripts/src/bus.lua") ..
        selectors_get(MAME_DIR .. "scripts/src/formats.lua")

load(selectors)()

---------------------------------------------------------------------------
-- Device families needed for vector games (tiny.lua-style selectors)
---------------------------------------------------------------------------

-- CPUs used by vector platforms (Atari, Cinematronics, Sega G80, etc.)
CPUS["M6502"]   = true
CPUS["M68000"]  = true
CPUS["M6809"]   = true
CPUS["Z80"]     = true
CPUS["M6805"]   = true
CPUS["M6800"]   = true
CPUS["MCS48"]   = true
CPUS["MCS51"]   = true
CPUS["TMS9900"] = true
CPUS["TMS32025"] = true
SOUNDS["C140"] = true

-- Sound devices commonly used by vector games
SOUNDS["POKEY"]    = true
SOUNDS["TMS5220"]  = true
SOUNDS["YM2151"]   = true
SOUNDS["SN76496"]  = true
SOUNDS["SAMPLES"]  = true
SOUNDS["DAC"]      = true
SOUNDS["DISCRETE"] = true
SOUNDS["ASTROCADE"]= true
SOUNDS["SEGAUSB"]  = true

-- Video: vector core is essential
VIDEOS["VECTOR"] = true
VIDEOS["VECTRX2020"] = true

-- Machine devices for Atari/Cinematronics/Sega vector hardware, IO, etc.
MACHINES["MATHBOX"]     = true
MACHINES["AVGDVG"]      = true
MACHINES["ATARI_VG"]    = true
MACHINES["CINEMAT"]     = true
MACHINES["SEGAG80V"]    = true
MACHINES["6821PIA"]     = true
MACHINES["INPUT_MERGER"]= true
MACHINES["WATCHDOG"]    = true
MACHINES["NETLIST"]     = true
MACHINES["TICKET"]      = true
MACHINES["Z80CTC"]      = true
MACHINES["Z80PIO"]      = true
MACHINES["Z80DAISY"]    = true

-- Buses: vector games use very little here; keep minimal
BUSES["CENTRONICS"] = true

---------------------------------------------------------------------------
-- Helpers
---------------------------------------------------------------------------

local function new_set()
    return { _items = {} }
end

local function set_add(s, v)
    if not s._items[v] then
        s._items[v] = true
        table.insert(s, v)
    end
end


---------------------------------------------------------------------------
-- Expand each root to include companion files (same prefix, same dir)
---------------------------------------------------------------------------

local function expand_with_companions(vector_roots)
    local files_set  = {}
    local files_list = {}



    for _, file in ipairs(vector_roots) do
        -- always include the root file itself
        if not files_set[file] then
            files_set[file] = true
            table.insert(files_list, file)
        end

        -- derive directory and prefix
        local dir  = file:match("(.+)/[^/]+$")
        local base = file:match("([^/]+)%.cpp$")
        if dir and base then
            local prefix = base

            -- include all .cpp files in this directory whose names start with prefix
            local cmd = 'find "' .. dir .. '" -type f -name "' .. prefix .. '*.cpp"'
            local p = io.popen(cmd)
            for fpath in p:lines() do
                if not files_set[fpath] then
                    files_set[fpath] = true
                    table.insert(files_list, fpath)
                end
            end
        end
    end

    return files_list
end

---------------------------------------------------------------------------
-- Shortname filter
---------------------------------------------------------------------------

local function is_valid_shortname(name)
    if not name then return false end

    -- trim whitespace
    name = name:match("^%s*(.-)%s*$")
    if not name or name == "" then return false end

    -- hard blacklist of known non-vector or junk shortnames that sneak in
    local non_vector = {
        breakckt = true, breakout = true, c64gs = true,
        cktpong = true, consolet = true, coupedav = true,
        drpong = true, pong = true, pongbarl = true, pongd = true,
        pupppong = true, snoopong = true, suprpong = true, spike = true, volleyball = true,
        ninjakd2 = true, ninjakd2a = true, ninjakd2b = true, ninjakd2c = true,
        omegaf = true, omegafa = true, omegafs = true,
        jt104 = true, mnight = true, mnightj = true,
        my1stddr = true, pwrshovl = true, rdaction = true,
        rebound = true, senspid = true, shtscore = true,
        styphp = true, topgunnr = true, trkfldch = true, trkfldchj = true,
    }
    if non_vector[name] then
        return false
    end

    -- reject numeric or zero
    if name == "0" or tonumber(name) ~= nil then return false end

    -- reject common macros / flags
    local bad = {
        ROT0 = true, ROT90 = true, ROT180 = true, ROT270 = true,
        ORIENTATION_FLIP_X = true, ORIENTATION_FLIP_Y = true,
        MACHINE_SUPPORTS_SAVE = true,
    }
    if bad[name] then return false end

    -- reject state classes
    if name:match("_state$") then return false end

    -- reject all-caps identifiers (likely macros)
    if name:match("^[A-Z_]+$") then return false end

    return true
end

---------------------------------------------------------------------------
-- Detect ROOT vector driver source files
-- (only files that clearly use vector rendering)
---------------------------------------------------------------------------

local function detect_vector_driver_roots(base_dir)
    local vector_roots = new_set()

    local cmd = 'find "' .. base_dir .. '" -type f -name "*.cpp"'
    local p = io.popen(cmd)

    for file in p:lines() do
        local f = io.open(file, "r")
        if f then
            local content = f:read("*all")
            f:close()

            -- Precise detection: actual vector hardware includes vector.h
            if content:match('#include%s+"video/vector%.h"') then
                set_add(vector_roots, file)
            end
        end
    end

    return vector_roots
end
---------------------------------------------------------------------------
-- Extract driver shortnames ONLY from ROOT vector driver files
---------------------------------------------------------------------------

local function extract_vector_shortnames(vector_roots)
    local names_set  = {}
    local names_list = {}

    for _, file in ipairs(vector_roots) do
        local f = io.open(file, "r")
        if f then
            local content = f:read("*all")
            f:close()

            -- GAME(...)
            for args in content:gmatch("GAME%s*%((.-)%)") do
                local i = 0
                for token in args:gmatch("[^,]+") do
                    i = i + 1
                    if i == 2 then
                        local name = token:match("^%s*(.-)%s*$")
                        if is_valid_shortname(name) and not names_set[name] then
                            names_set[name] = true
                            table.insert(names_list, name)
                        end
                        break
                    end
                end
            end

            -- GAMEL(...)
            for args in content:gmatch("GAMEL%s*%((.-)%)") do
                local i = 0
                for token in args:gmatch("[^,]+") do
                    i = i + 1
                    if i == 2 then
                        local name = token:match("^%s*(.-)%s*$")
                        if is_valid_shortname(name) and not names_set[name] then
                            names_set[name] = true
                            table.insert(names_list, name)
                        end
                        break
                    end
                end
            end

            -- CONS(...)
            for args in content:gmatch("CONS%s*%((.-)%)") do
                local i = 0
                for token in args:gmatch("[^,]+") do
                    i = i + 1
                    if i == 2 then
                        local name = token:match("^%s*(.-)%s*$")
                        if is_valid_shortname(name) and not names_set[name] then
                            names_set[name] = true
                            table.insert(names_list, name)
                        end
                        break
                    end
                end
            end
        end
    end

    table.sort(names_list)
    return names_list
end

---------------------------------------------------------------------------
-- Generate vectrx2020.lst
---------------------------------------------------------------------------

local function generate_vectrx2020_lst(drivernames)
    local lst_path = path.join(MAME_DIR, "src/mame/vectrx2020.lst")
    local out = assert(io.open(lst_path, "w"))

    out:write([[
 // license:BSD-3-Clause
 // copyright-holders:Vectrx2020
 /*******************************************************************************
 
     vectrx2020.lst
 
     List of all enabled vector drivers for this compile target.
 
 *******************************************************************************/
 
 ]])

    for _, name in ipairs(drivernames) do
        out:write(name, "\n")
    end

    out:close()
    print("Generated vectrx2020.lst with " .. #drivernames .. " vector drivers at " .. lst_path)
end

---------------------------------------------------------------------------
-- SUBTARGET project creation
---------------------------------------------------------------------------

function createProjects_mame_vectrx2020(_target, _subtarget)
    -----------------------------------------------------------------------
    -- Discover vector drivers and generate vectrx2020.lst
    -----------------------------------------------------------------------



    local base_mame_dir = path.join(MAME_DIR, "src/mame")

    -- 1) detect root vector driver .cpp files
    local vector_roots   = detect_vector_driver_roots(base_mame_dir)
-- Add Cinematronics vector drivers manually (they don't include video/vector.h)
    local cinematronics_drivers = {
       MAME_DIR .. "src/mame/cinematronics/cinemat.cpp",
        MAME_DIR .. "src/mame/cinematronics/cinemat_a.cpp",
        MAME_DIR .. "src/mame/cinematronics/cinemat_v.cpp",
            MAME_DIR .. "src/mame/sega/segag80v.cpp",
    }

    for _, f in ipairs(cinematronics_drivers) do
        set_add(vector_roots, f)
    end

    -- 2) expand each root with its companion .cpp files
    local vector_sources = expand_with_companions(vector_roots)

    -- 3) extract shortnames only from root vector drivers
    local drivernames    = extract_vector_shortnames(vector_roots)

    -- Starblade is polygon hardware with an optional hidden-line capture path.
    table.insert(drivernames, "starblad")
    table.insert(drivernames, "starbladj")
    table.sort(drivernames)

    -- 4) write vectrx2020.lst
    generate_vectrx2020_lst(drivernames)

    -----------------------------------------------------------------------
    -- Define the vectrx2020 driver library
    -----------------------------------------------------------------------
        project ("mame_vectrx2020")
        targetsubdir(_target .. "_" .. _subtarget)
        kind (LIBTYPE)
        uuid (os.uuid("drv-mame_vectrx2020"))
   


        addprojectflags()
        precompiledheaders_novs()
      

        includedirs {
            MAME_DIR .. "src/osd",
            MAME_DIR .. "src/emu",
            MAME_DIR .. "src/devices",
            MAME_DIR .. "src/mame",
            MAME_DIR .. "src/mame/shared",
            MAME_DIR .. "src/lib",
            MAME_DIR .. "src/lib/util",
            MAME_DIR .. "3rdparty",
            MAME_DIR .. "src/mame/atari",
            MAME_DIR .. "src/mame/sega",
            GEN_DIR .. "mame/layout"
        }
 
        includedirs {
            ext_includedir("asio"),
            ext_includedir("flac"),
            ext_includedir("glm"),
            ext_includedir("jpeg"),
            ext_includedir("rapidjson"),
            ext_includedir("zlib"),
			MAME_DIR .. "../../vectrx2020_lib",
		    MAME_DIR .. "../../vectrx2020_Device",
        }
 

        -- Vector driver sources (+ their companions)
        files(vector_sources)
        files {
            MAME_DIR .. "src/mame/namco/namcos21_c67.cpp",
            MAME_DIR .. "src/mame/namco/namcos21_dsp_c67.cpp",
            MAME_DIR .. "src/mame/namco/namcos21_3d.cpp",
            MAME_DIR .. "src/mame/namco/namco_dsp.cpp",
            MAME_DIR .. "src/mame/namco/namco68.cpp",
            MAME_DIR .. "src/mame/namco/namco_c139.cpp",
            MAME_DIR .. "src/mame/namco/namco_c148.cpp",
            MAME_DIR .. "src/mame/shared/namco_c355spr.cpp",
            MAME_DIR .. "src/mame/atari/redbaron.cpp",
            MAME_DIR .. "src/mame/atari/mathbox.cpp",
            MAME_DIR .. "src/mame/atari/slapstic.cpp",
            MAME_DIR .. "src/mame/sega/segag80.cpp",
            MAME_DIR .. "src/mame/sega/segag80v.cpp",
            MAME_DIR .. "src/mame/sega/segag80r.cpp",
            MAME_DIR .. "src/mame/sega/segag80_m.cpp",
            MAME_DIR .. "src/mame/sega/segaspeech.cpp",
            MAME_DIR .. "src/mame/sega/segausb.cpp", 
            MAME_DIR .. "src/mame/sega/nl_astrob.cpp",
            MAME_DIR .. "src/mame/sega/nl_elim.cpp",
            MAME_DIR .. "src/mame/sega/nl_spacfury.cpp",
        }
        files {
            MAME_DIR .. "src/mame/cinematronics/nl_armora.cpp",
            MAME_DIR .. "src/mame/cinematronics/nl_barrier.cpp",
            MAME_DIR .. "src/mame/cinematronics/nl_boxingb.cpp",
            MAME_DIR .. "src/mame/cinematronics/nl_ripoff.cpp",
            MAME_DIR .. "src/mame/cinematronics/nl_solarq.cpp",
            MAME_DIR .. "src/mame/cinematronics/nl_spacewar.cpp",
            MAME_DIR .. "src/mame/cinematronics/nl_speedfrk.cpp",
            MAME_DIR .. "src/mame/cinematronics/nl_starcas.cpp",
            MAME_DIR .. "src/mame/cinematronics/nl_starhawk.cpp",
            MAME_DIR .. "src/mame/cinematronics/nl_sundance.cpp",
            MAME_DIR .. "src/mame/cinematronics/nl_tailg.cpp",
            MAME_DIR .. "src/mame/cinematronics/nl_warrior.cpp",
       
        }
        defines {
         --   "NOMINMAX",
            "VECTRX2020_MAME"
        }
end

---------------------------------------------------------------------------
-- Linking hook
---------------------------------------------------------------------------

function linkProjects_mame_vectrx2020(_target, _subtarget)
   links {
        
        ext_lib("mame_vectrx2020"),
    }

end