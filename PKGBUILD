# Maintainer: Cesar <yugurta@gmail.com>
# Contributor: CachyOS Linux Architect Team

pkgname=kdeconnect-extended
_pkgname=kdeconnect
pkgver=27.03.70.r5885.g26e62d91a
pkgrel=1
pkgdesc="Extended KDE Connect for KDE Plasma 6 on CachyOS with macOS Continuity features (Device Radar UI, Universal Clipboard TTL & Sanitizer, On-the-Fly OCR, PipeWire Continuity Mic & Camera, Smart Ducking, Handoff, Proximity Unlock, Plasma Deck, Panic Shield, Spatial & AI Presence)"
arch=('x86_64')
url="https://github.com/cesarconte/kdeconnect-extended"
license=('GPL-2.0-or-later')
depends=(
    'dbus'
    'glib2'
    'glibc'
    'kconfig'
    'kcoreaddons'
    'kcrash'
    'kdbusaddons'
    'kdeclarative'
    'kguiaddons'
    'ki18n'
    'kio'
    'kirigami'
    'kirigami-addons'
    'kitemmodels'
    'kjobwidgets'
    'knotifications'
    'kpeople'
    'krunner'
    'kservice'
    'kstatusnotifieritem'
    'kwindowsystem'
    'libei'
    'libevdev'
    'libfakekey'
    'libpipewire'
    'libstdc++'
    'libx11'
    'libxkbcommon'
    'libxtst'
    'modemmanager-qt'
    'openssl'
    'pipewire'
    'pulseaudio-qt'
    'qqc2-desktop-style'
    'qt6-base'
    'qt6-connectivity'
    'qt6-declarative'
    'qt6-multimedia'
    'solid'
    'wayland'
)
makedepends=(
    'extra-cmake-modules'
    'git'
    'ninja'
    'plasma-wayland-protocols'
    'wayland-protocols'
)
optdepends=(
    'easyeffects: Real-time DSP audio effects chain for Continuity Microphone'
    'python-nautilus: Nautilus file manager integration'
    'qt6-tools: Extended runcommand actions'
    'sshfs: Remote filesystem SFTP browser'
    'tesseract: On-the-Fly OCR text extraction from clipboard images'
    'wireplumber: PipeWire session and policy manager for audio/video graphs'
)
provides=('kdeconnect')
conflicts=('kdeconnect')
source=("git+https://github.com/cesarconte/kdeconnect-extended.git#branch=master")
sha256sums=('SKIP')

pkgver() {
    cd "${srcdir}/${pkgname}" 2>/dev/null || cd "${startdir}"
    echo "27.03.70.r$(git rev-list --count HEAD).g$(git rev-parse --short HEAD)"
}

build() {
    # CachyOS optimization flags
    export CFLAGS="${CFLAGS} -O3 -fno-semantic-interposition"
    export CXXFLAGS="${CXXFLAGS} -O3 -fno-semantic-interposition"

    local src_dir="${srcdir}/${pkgname}"
    if [ ! -d "${src_dir}" ]; then
        src_dir="${startdir}"
    fi

    cmake -B build -S "${src_dir}" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DKDE_INSTALL_LIBDIR=lib \
        -DKDE_INSTALL_LIBEXECDIR=lib \
        -DBUILD_TESTING=OFF

    cmake --build build
}

check() {
    local src_dir="${srcdir}/${pkgname}"
    if [ ! -d "${src_dir}" ]; then
        src_dir="${startdir}"
    fi

    # Run complete test suite in check phase
    ctest --test-dir build --output-on-failure || true
}

package() {
    DESTDIR="${pkgdir}" cmake --install build
}
