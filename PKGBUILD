pkgname=simple-make
pkgver=1.0.0.r0
pkgrel=1
pkgdesc="Grab a random GitHub project, build it, and install it — all in one command"
arch=('x86_64' 'aarch64')
url="https://github.com/ArtemyStudio/simple-make"
license=('MIT')
depends=('curl' 'git' 'gcc-libs')
makedepends=('cmake' 'git')
optdepends=('make: building make-based projects'
            'meson: building meson-based projects'
            'ninja: building ninja/meson-based projects'
            'cargo: building rust projects'
            'go: building go projects'
            'python-pip: building python projects'
            'maven: building java (maven) projects'
            'gradle: building java (gradle) projects'
            'unzip: extracting .zip sources'
            'p7zip: extracting .7z sources'
            'unrar: extracting .rar sources'
            'zstd: extracting .tar.zst sources')
source=("git+https://github.com/ArtemyStudio/simple-make.git")
sha256sums=('SKIP')

pkgver() {
  cd "$pkgname"
  printf "1.0.0.r%s" "$(git rev-list --count HEAD)"
}

build() {
  cd "$pkgname"
  cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
  cmake --build build -j"$(nproc)"
}

package() {
  cd "$pkgname"
  install -Dm755 build/simple-make "$pkgdir/usr/bin/simple-make"
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
