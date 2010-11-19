module(..., package.seeall)
local C = colorize
local curl = require "cURL"
local zlib = require "zlib"

local function http_handle ( method, url )
    local easyh = curl.easy_init()
    easyh:setopt_verbose( 0 )
    easyh:setopt_url( url )

    if method == "HEAD" then
        easyh:setopt_nobody( 1 )
    elseif method == "POST" then
        easyh:setopt_post( 1 )
    end

    return easyh
end

local function http_check ( easyh, method )
    local code = easyh:getinfo_response_code()
    if code ~= 200 then
        local url = easyh:getinfo_effective_url()
        url = url:gsub( "^https?://aur[.]archlinux[.]org/", "/" )
        error( string.format( "HTTP %s %s failed (%d)",
                              method, url, code ))
    end
    return
end

function size ( url )
    local easyh = http_handle( 'HEAD', url )
    easyh:perform()
    http_check( easyh, "HEAD" )

    return easyh:getinfo_content_length_download()
end

function get ( url )
    local easyh = http_handle( 'GET', url )

    local accum = ""
    local function callback ( data ) accum = accum .. data end
    easyh:perform{ writefunction = callback }
    http_check( easyh, "GET" )

    return accum
end

function getgzip ( url )
    local easyh = http_handle( 'GET', url )

    easyh:setopt_httpheader{ "Accept-Encoding: gzip" };

    local accum = ""
    local function callback ( data ) accum = accum .. data end

    local is_gzipped = false
    local function enc_check ( header )
        if header == "Content-Encoding: gzip\13\10" then
            is_gzipped = true
        end
    end

    easyh:perform{ writefunction  = callback,
                   headerfunction = enc_check }
    http_check( easyh, "GET" )

    local content_type = easyh:getinfo_content_type()
    if is_gzipped then
        return zlib.inflate( accum ):read( "*a" )
    end

    return accum
end

function download ( url, destdir, downloadcb )
    -- Prepare arguments
    local destfile = url:gsub( "^.*/", "" )
    if destdir then destfile = destdir .. "/" .. destfile end
    local fh       = assert( io.open( destfile, 'w' ))
    local easyh    = http_handle( "GET", url )

    local writercb
    if downloadcb then
        if type( downloadcb ) ~= "function" then
            error( "Argument three must be a callback function" )
        end

        local totalsize = http_size( url )
        local dlsize    = 0
        writercb = function ( data )
                       fh:write( data )
                       dlsize = dlsize + #data
                       downloadcb( dlsize, totalsize )
                   end
    else
        writercb = function ( data ) fh:write( data ) end
    end

    easyh:perform{ writefunction = writercb }
    http_check( easyh, "GET" )
    fh:close() -- very important to close the file and flush the buffer!

    return
end
