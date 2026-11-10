#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
# install.sh: Autonomous installer for kdeconnect-extended on KDE Plasma 6 / CachyOS Linux

set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE="user"

for arg in "$@"; do
    case "$arg" in
        --system)
            MODE="system"
            ;;
        --user)
            MODE="user"
            ;;
        --help|-h)
            echo "Uso: ./install.sh [--user | --system]"
            echo "  --user   (Por defecto) Compila e instala los plugins extendidos en ~/.local/lib/qt6/plugins/kdeconnect/"
            echo "  --system Compila e instala el paquete completo a nivel de sistema (/usr) mediante makepkg"
            exit 0
            ;;
    esac
done

echo "==> Instalando kdeconnect-extended (Modo: ${MODE})..."

if [ "$MODE" = "system" ]; then
    if ! command -v makepkg >/dev/null 2>&1; then
        echo "❌ makepkg no encontrado. Este modo requiere Arch Linux / CachyOS."
        exit 1
    fi
    cd "$SCRIPT_DIR"
    makepkg -si --noconfirm
    echo "🎉 Instalación del sistema completada."
    exit 0
fi

# Modo --user (Rápido, sin sudo, sobrescribe los plugins existentes en ~/.local)
MISSING_DEPS=()
for dep in cmake ninja pkg-config; do
    if ! command -v "$dep" >/dev/null 2>&1; then
        MISSING_DEPS+=("$dep")
    fi
done

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo "⚠️  Faltan herramientas de compilación: ${MISSING_DEPS[*]}"
    echo "--> Instálalas con: sudo pacman -S --needed cmake ninja base-devel"
    exit 1
fi

BUILD_DIR="${SCRIPT_DIR}/build"
mkdir -p "$BUILD_DIR"

echo "  -> Configurando CMake..."
cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF

echo "  -> Compilando plugins..."
cmake --build "$BUILD_DIR" --target kdeconnect_notifications kdeconnect_clipboard kdeconnect_plasmadeck

INSTALL_PLUGIN_DIR="${HOME}/.local/lib/qt6/plugins/kdeconnect"
mkdir -p "$INSTALL_PLUGIN_DIR"

echo "  -> Instalando bibliotecas en ${INSTALL_PLUGIN_DIR}..."
install -m 755 "${BUILD_DIR}/bin/plugins/kdeconnect_notifications.so" "${INSTALL_PLUGIN_DIR}/"
install -m 755 "${BUILD_DIR}/bin/plugins/kdeconnect_clipboard.so" "${INSTALL_PLUGIN_DIR}/"
if [ -f "${BUILD_DIR}/bin/plugins/kdeconnect_plasmadeck.so" ]; then
    install -m 755 "${BUILD_DIR}/bin/plugins/kdeconnect_plasmadeck.so" "${INSTALL_PLUGIN_DIR}/"
fi

echo "  -> Reiniciando servicio kdeconnectd..."
systemctl --user restart app-org.kde.kdeconnect.daemon@autostart.service 2>/dev/null || \
systemctl --user restart kdeconnectd 2>/dev/null || \
(killall kdeconnectd 2>/dev/null && kdeconnectd &) || true

echo ""
echo "🎉 ¡kdeconnect-extended instalado y activo en el espacio de usuario!"
