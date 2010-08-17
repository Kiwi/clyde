-- luaur.util package

local lfs = require "lfs"

function rec_mkdir ( dirpath )
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

function absdir ( path )
    if path:match( "^/" ) then
        return path
    end

    return lfs.currentdir() .. "/" .. path
end

function chdir ( path )
    local oldir, err = lfs.currentdir()
    assert( oldir, err )

    local success, err = lfs.chdir( path )
    assert( success, err )

    return oldir
end
