--[[--------------------------------------------------------------------------

ui.lua - Input from and output to the user

--]]--------------------------------------------------------------------------

module( "clydelib.ui", package.seeall )

local C = require "clydelib.colorize"

local utilcore = require "clydelib.utilcore"
local G = utilcore.gettext

--- Creates a closure that convert pkginfo to a colorized line/string.
-- This colorizes the repo name and package name as well as the
-- version string.
--
-- @param col_funcs A table of functions who are passed the pkginfo
--                  table at runtime. These return a string they would
--                  like added to the package line or nil if they
--                  don't want to add anything.
function mk_pkg_colorizer ( ... )
    local col_funcs = arg
    local function pkg_colorizer ( pkg )

        -- Every colorizer made shows at least the package name and
        -- version...

        -- The package name printed is dbname/pkgname...
        local words    = {}
        local dbname   = colorize_dbname( pkg.dbname )
        local longname = dbname .. "/" .. C.bright( pkg.name )
        table.insert( words, longname )

        -- Package version is the next word...
        table.insert( words, colorize_verstr( pkg.version, C.gre ))

        -- Now we iterate through any custom "word" generators...
        for i, func in ipairs( col_funcs ) do
            table.insert( words, func( pkg ))
        end

        return table.concat( words, " " )
    end

    return pkg_colorizer
end

-- Returns the colorized group list, as a string to be printed
function groups_tag ( pkg )
    local group_names = pkg.groups
    if not group_names or not next( group_names ) then return nil end
    return C.blub( "(" )
        .. C.blu( table.concat( group_names, " " ))
        .. C.blub( ")" )
end

function colorize_dbname ( dbname )
    local dbcolors = {
        extra = C.greb;
        core = C.redb;
        community = C.magb;
        testing = C.yelb;
        aur = C.cyab;
        ["local"] = C.yelb;
    }

    local dbcolor = dbcolors[ dbname ] or C.bright
    return dbcolor( dbname )
end

-- Enbolden the periods and hyphen in a version string.
function colorize_verstr ( verstr, color )
    local verstr = color( verstr )
    verstr = verstr:gsub( "-(%d+)\27%[0m$",
                          function ( pkgrel )
                              -- I tried to use C.dim here but GNU Screen
                              -- is buggy and thought it was underlined...
                              -- TODO: patch GNU Screen?
                              return C.bright .. "-" .. C.reset
                                  .. color( pkgrel )
                          end )
    verstr = verstr:gsub( "%.", C.bright( "." ) .. color )
    return verstr
end

-- Prompts the user for numbers. Many choices can be separated by whitespace.
-- The numbers chosen are returned as a list. If no numbers were
-- chosen (i.e. only ENTER was typed) then nil is returned.
function prompt_for_numbers ( prompt, max )
    local function ask_nums ( maxnum )
        io.write( prompt .. " " )
        local nums_choice = io.read()

        if nums_choice == "" then return nil end

        input_numbers = {}
        for num in nums_choice:gmatch( "(%S+)" ) do
            if num:match( "%D" ) then
                error( "Invalid input", 0 )
            else
                num = tonumber( num )
                if ( num < 1 or num > maxnum ) then
                    error( "Out of range", 0 )
                end
                table.insert( input_numbers, num )
            end
        end

        return input_numbers
    end

    local chosen_nums = {}
    while ( true ) do
        local success, answer
            = pcall( ask_nums, max )

        if ( success ) then
            return answer -- answer may be nil
        elseif ( answer == "Invalid input" ) then
            print( "Please enter only digits and/or whitespace." )
        elseif ( answer == "Out of range" ) then
            print( "Input numbers must be between 1 and " .. max .. "." )
        else
            -- Rethrow an unknown error.
            error( answer, 0 )
        end
    end
end

--- A more sophisticated translator and colorizer
-- We must translate before we colorize or else gettext won't work properly.
function format ( fmt, ... )
    local fmt = G( fmt )
    fmt = string.format( fmt, unpack( arg ))
    if fmt:match( "^::" ) then
        fmt = fmt:gsub( "^::", "" )
        fmt = C.yelb( "::" ) .. C.bright( fmt )
    end
    return fmt
end
