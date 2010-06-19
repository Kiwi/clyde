# -*- Mode: shell-script; sh-basic-offset: 2 -*-
# Contributor: Daniel J Griffiths <ghost1227@archlinux.us>
# Contributor: Justin Davis <jrcd83@gmail.com>
pkgname=clyde-fork-git
pkgver=20100618
pkgrel=1
pkgdesc="Next-generation libalpm/makepkg wrapper."
arch=('i686' 'x86_64')
url="http://clyde.archuser.com"
license=('custom')
makedepends=('git')
depends=('lua-lzlib' 'lua-yajl-git' 'luasocket' 'luafilesystem'
         'pacman>=3.4' 'pacman<3.5' )
provides=('clyde' 'lualpm' 'lualpm-git')
conflicts=('clyde' 'lualpm' 'lualpm-git')

_gitroot='git://github.com/juster/clyde.git'
_gitbranch=${BRANCH:-'pacman3.4'}

build() {
  DIST_DIR="${srcdir}/clyde"
  msg 'Creating CPANPLUS::Dist::Arch developer package...'

  if [ -d "$DIST_DIR" ] ; then
    warning 'Repository directory already exists!'
    msg2 'Attempting to pull from repo...'
    cd "$DIST_DIR"
    git pull origin "$_gitbranch"
  else
    msg2 "Cloning $_gitroot repository..."
    git clone "$_gitroot" "$DIST_DIR"
    cd "$DIST_DIR"
  fi

  msg2 "Checking out the $_gitbranch branch..."
  git checkout "$_gitbranch"
  if [ "$?" -ne 0 ] ; then
    error "Failed to checkout the $_gitbranch branch... aborting."
    return 1
  fi

  msg 'Building clyde...'
  make lualpm clyde || return 1
  make DESTDIR=${pkgdir} install_lualpm install_clyde || return 1
  install -Dm644 LICENSE ${pkgdir}/usr/share/licenses/${pkgname}/LICENSE
}