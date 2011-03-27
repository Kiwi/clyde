# -*- Mode: shell-script; sh-basic-offset: 2 -*-
# Contributor: Daniel J Griffiths <ghost1227@archlinux.us>
# Contributor: Justin Davis <jrcd83@gmail.com>
pkgname='clyde'
pkgver='0.03.15'
pkgrel='1'
pkgdesc="Next-generation libalpm/makepkg wrapper."
arch=('i686' 'x86_64')
url='https://github.com/Kiwi/clyde'
license=('custom')
makedepends=('make')
depends=('lua-lzlib' 'lua-yajl-git' 'luasocket'
         'luafilesystem' 'luasec')
provides=('lualpm=0.03')
conflicts=('clyde-git')

build() {
  cd "$startdir"
  msg 'Building clyde...'
  make
  msg 'Packaging clyde...'
  make DESTDIR="$pkgdir" install
  install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}
