# -*- Mode: shell-script; sh-basic-offset: 2 -*-
# Contributor: Daniel J Griffiths <ghost1227@archlinux.us>
# Contributor: Justin Davis <jrcd83@gmail.com>
pkgname='clyde-git'
pkgver='20100623'
pkgrel='1'
pkgdesc="Next-generation libalpm/makepkg wrapper."
arch=('i686' 'x86_64')
url="http://clyde.archuser.com"
license=('custom')
makedepends=('git' 'make')
depends=('lua-lzlib' 'lua-yajl-git' 'luasocket' 'luafilesystem'
         'pacman>=3.4' 'pacman<3.5')
provides=('clyde' 'lualpm')

_gitroot='git://github.com/Kiwi/clyde.git'
_gitbranch=${BRANCH:-'master'}

build() {
  REPO_DIR="${srcdir}/clyde"

  if [ -d "$REPO_DIR" ] ; then
    warning 'Repository directory already exists!'
    msg2 "Pulling from $_gitroot..."
    cd "$REPO_DIR"
    git pull origin
  else
    msg2 "Cloning $_gitroot repository..."
    git clone "$_gitroot" "$REPO_DIR"
    cd "$REPO_DIR"
  fi

  msg2 "Checking out the $_gitbranch branch..."
  git checkout "$_gitbranch"

  if [ "$?" -ne 0 ] ; then
    error "Failed to checkout the $_gitbranch branch... aborting."
    return 1
  fi

  msg 'Building clyde...'
  make || return 1
  msg 'Packaging clyde...'
  make DESTDIR="$pkgdir" install || return 1
  install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}