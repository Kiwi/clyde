# -*- Mode: shell-script; sh-basic-offset: 2 -*-
# Contributor: Daniel J Griffiths <ghost1227@archlinux.us>
# Contributor: Justin Davis <jrcd83@gmail.com>
pkgname='clyde'
pkgver='0.03.07'
pkgrel='1'
pkgdesc="Next-generation libalpm/makepkg wrapper."
arch=('i686' 'x86_64')
url="http://clyde.archuser.com"
license=('custom')
makedepends=('make')
depends=('lua-lzlib' 'lua-yajl-git' 'luasocket' 'luafilesystem'
         'pacman>=3.4' 'pacman<3.5' 'luasec')
provides=('lualpm=0.02')
conflicts=('clyde-git')

build() {
  cd "$startdir"
  msg 'Building clyde...'
  make || return 1
  msg 'Packaging clyde...'
  make DESTDIR="$pkgdir" install || return 1
  install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}
