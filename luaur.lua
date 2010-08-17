--[[

LuAUR - Lua interface to the ArchLinux User Repository
by Justin Davis <jrcd83@gmail.com>

Adapted from clydes builting AUR code.

]]--

local lfs  = require "lfs"
local zlib = require "zlib"
local yajl = require "yajl"
local http = require "socket.http"

-- CONSTANTS -----------------------------------------------------------------

local AUR_BASEURI   = "http://aur.archlinux.org"
local AUR_PKGFMT    = "/packages/%s/%s.tar.gz"
local AUR_USERAGENT = "LuAUR/v0.01"

-- UTILITY FUNCTIONS ---------------------------------------------------------

-- Copied from "Programming in Lua"
local function each ( tbl )
    local i   = 0
    local max = table.maxn( tbl )
    return function ()
               i = i + 1
               if i <= max then return tbl[i] end
               return nil
           end
end

local function filter ( f, tbl )
    local result = {}
    for elem in each( tbl ) do
        if f( elem ) then table.insert( result, elem ) end
    end

    return result
end

local function map ( f, tbl )
    local result = {}
    for key, val in pairs( tbl ) do
        result[ key ] = f( val )
    end
    return result
end

local function reverse ( tbl )
    local result = {}
    local i = table.maxn( tbl )
    while i > 0 do
        table.insert( result, tbl[i] )
        i = i - 1
    end
    return result
end

------------------------------------------------------------------------------

AUR = { basepath = "/tmp/luaur/" }
AUR.__index = AUR

function AUR:new ( params )
    local obj = params or { }
    setmetatable( obj, self )
    return obj
end

function AUR:search ( query )
    
end

local function lookup_pkg ( name )
    local pkgbuildtext = retrieve_pkgbuild( name )
    if not pkgbuildtext then return nil end
    return pkgbuild_fields( pkgbuildtext )
end

function AUR:get ( package )
    
end

------------------------------------------------------------------------------

AURPackage = { }
AURPackage.__index = AURPackage

function AURPackage:new ( params ) --name, basepath, proxy )n
    assert( params.name, "Parameter 'name' must be specified" )
    assert( ( params.dlpath and params.extpath and params.destpath )
            or params.basepath, [[
Parameter 'basepath' must be specified unless all other paths are provided
]] )

    local dlpath   = params.dlpath   or params.basepath .. "/src"
    local extpath  = params.extpath  or params.basepath .. "/build"
    local destpath = params.destpath or params.basepath .. "/cache"

    local obj    = params
    obj.tgzpath  = ""
    obj.pkgname  = obj.name .. ".src.tar.gz"
    obj.dlpath   = dlpath
    obj.extpath  = extpath
    obj.destpath = destpath

    setmetatable( obj, self )
    return obj
end

function AURPackage:rec_mkdir ( dirpath )
    local abs = ( dirpath:match( "^/" ) ~= nil )

    local comps = {}
    for comp in dirpath:gmatch( "[^/]+" ) do
        table.insert( comps, comp )
    end

    local max = table.maxn( comps )
    assert( max > 0 )

    local j = 1
    while j <= max do
        local checkpath = table.concat( comps, "/", 1, j )
        if abs then checkpath = "/" .. checkpath end

        if not lfs.attributes( checkpath ) then
            assert( lfs.mkdir( checkpath ))
        end

        j = j + 1
    end

    return
end

function AURPackage:download ( )
    local pkgname      = self.pkgname
    local pkgurl       = AUR_BASEURI .. string.format( AUR_PKGFMT,
                                                       self.name, self.name )
    local pkgpath      = self.dlpath .. "/" .. pkgname

    -- Make sure the destination directory exists...
    self:rec_mkdir( self.dlpath )

    local pkgfile, err = io.open( pkgpath, "wb" )
    assert( pkgfile, err )

    USERAGENT = AUR_USERAGENT
    local good, status   = http.request{ url    = pkgurl,
                                         proxy  = self.proxy,
                                         sink   = ltn12.sink.file( pkgfile ) }

    if not good or status ~= 200 then
        local err
        if status ~= 200 then
            err = "HTTP error status " .. status
        else
            err = status
        end
        error( string.format( "Failed to download %s: %s", pkgurl, err ))
    end

    self.tgzpath = pkgpath
    return
end

function AURPackage:get_srcpkg_path ( )
    if self.tgzpath == "" then self:download() end
    return self.tgzpath
end

function AURPackage:chdir ( path )
    local oldir, err = lfs.currentdir()
    assert( oldir, err )

    local success, err = lfs.chdir( path )
    assert( success, err )

    return oldir
end

function AURPackage:extract ( destdir )
    local pkgpath = self:get_srcpkg_path()

    -- Do not extract files redundantly...
    if self.pkgdir then return self.pkgdir end

    if destdir == nil then
        destdir = self.extpath
    else
        destdir:gsub( "/+$", "" )
    end

    self:rec_mkdir( destdir )
    local cmd = string.format( "bsdtar -zxf %s -C %s", pkgpath, destdir )
    local ret = os.execute( cmd )
    if ret ~= 0 then
        error( string.format( "bsdtar returned error code %d", ret ))
    end

    self.pkgdir = destdir .. "/" .. self.name
    return self.pkgdir
end

local function unquote_bash ( quoted_text )
    -- Convert bash arrays (surrounded by parens) into tables
    local noparen, subcount = quoted_text:gsub( "^%((.+)%)$", "%1" )
    if subcount > 0 then
        local wordlist = {}
        for word in noparen:gmatch( "(%S+)" ) do
            table.insert( wordlist, unquote_bash( word ))
        end
        return wordlist
    end

    -- Remove double or single quotes from bash strings
    local text = quoted_text
    text = text:gsub( '^"(.+)"$', "%1" )
    text = text:gsub( "^'(.+)'$", "%1" )
    return text
end

local function pkgbuild_fields ( text )
    local results = {}

    -- First find all fields without quoting characters...
    for name, value in text:gmatch( "([%l%d]+)=(%w%S+)" ) do
        results[ name ] = value
    end

    -- Now handle all quoted field values...
    local quoters = { '""', "''", "()" }
    local fmt     = '([%%l%%d]+)=(%%b%s)'

    for i, quotes in ipairs( quoters ) do
        local regexp = string.format( fmt, quotes )
        for name, value in text:gmatch( regexp ) do
            results[ name ] = unquote_bash( value )
        end
    end

    return results
end

-- Downloads, extracts tarball (if needed) and then parses the PKGBUILD...
function AURPackage:get_pkgbuild ( )
    local extdir = self:extract()

    if self.pkgbuild_info then return self.pkgbuild_info end

    local pbpath      = extdir .. "/PKGBUILD"
    local pbfile, err = io.open( pbpath, "r" )
    assert( pbfile, err )

    local pbtext       = pbfile:read( "*a" )
    self.pkgbuild_info = pkgbuild_fields( pbtext )

    pbfile:close()

    return self.pkgbuild_info
end

function AURPackage:build ( buildbase, pkgdest )
    local extdir = self:extract( buildbase )
    local oldir  = self:chdir( extdir )

    if pkgdest == nil then
        pkgdest = self.destpath
    else
        pkgdest:gsub( "/+$", "" )
    end

    

    self:chdir( oldir )
end