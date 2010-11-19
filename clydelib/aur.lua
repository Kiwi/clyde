module(..., package.seeall)
---downloading AUR stuff---
local lfs = require "lfs"
local http = require "clydelib.http"
local util = require "clydelib.util"
local C = colorize

local AURURL    = "https://aur.archlinux.org/"
local AURPKGFMT = AURURL .. "packages/%s/%s.tar.gz"

local function make_srcpkg_url ( pkgname )
    return string.format( AURPKGFMT, pkgname, pkgname )
end

-- TODO: allow users to override the builddir with config file
function prepare_builddir ( pkgname )
    local user = util.getbuilduser()
    lfs.mkdir("/tmp/clyde-"..user)
    local builddir = string.format( "/tmp/clyde-%s/%s", user, pkgname )
    lfs.mkdir( builddir )
    -- os.execute("chmod 700 " .. builddir)

    return builddir
end

function download ( aurpkg, destdir )
    local url      = make_srcpkg_url( aurpkg )
    local filename = url:gsub( "^.*/", "" )

    io.write(string.format(C.greb("==>")..C.bright(" Downloading %s\n"),
                           filename))
    http.download( url, destdir )
end
