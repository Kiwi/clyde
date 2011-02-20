--[[--------------------------------------------------------------------------

ui.lua - Input from and output to the user

--]]--------------------------------------------------------------------------

module( "clydelib.ui", package.seeall )

local C = colorize

-- Enbolden the periods and hyphen in a version string.
function colorize_verstr( verstr, color )
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
