require "luaur"

function LUAURPackage:edit_pkgbuild ( editcmd )
    local status = os.execute( editcmd
                               .. " "
                               .. self:srcdir_path( "PKGBUILD" ))
end
