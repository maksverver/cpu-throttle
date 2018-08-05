pkgname=cpu-throttle
pkgver=1.0
pkgrel=1
arch=('i686' 'x86_64')
depends=('cpupower')

build() {
	make -C "${startdir}"
}

package() {
	mkdir -p "${pkgdir}/usr/bin"
	install "${startdir}/cpu-throttle" -o root -g wheel -m 4750 "${pkgdir}/usr/bin"
}
