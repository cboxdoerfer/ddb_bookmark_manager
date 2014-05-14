pkgname=deadbeef-plugin-bookmark-manager-git
pkgver=20140514
pkgrel=1
pkgdesc="Bookmark Manager Plugin for the DeaDBeeF audio player (development version)"
url="https://github.com/cboxdoerfer/ddb_bookmark_manager"
arch=('i686' 'x86_64')
license='GPL2'
depends=('deadbeef')
makedepends=('git')
conflicts=('deadbeef-plugin-bookmark-manager')

_gitname=ddb_bookmark_manager
_gitroot=https://github.com/cboxdoerfer/${_gitname}

build() {
  cd $srcdir
  msg "Connecting to GIT server..."
  rm -rf $srcdir/$_gitname-build

  if [ -d $_gitname ]; then
    cd $_gitname
    git pull origin master
  else
    git clone $_gitroot
    cd $_gitname
  fi

  msg "GIT checkout done or server timeout"
  msg "Starting make..."

  cd $srcdir
  cp -r $_gitname $_gitname-build

  cd $_gitname-build

  touch AUTHORS
  touch ChangeLog

  make
}

package() {
  install -D -v -c $srcdir/$_gitname-build/ddb_misc_bookmark_manager.so $pkgdir/usr/lib/deadbeef/ddb_misc_bookmark_manager.so
}
