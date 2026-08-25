#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

cd "$ROOT_DIR"

build_cpp() {
    local name="$1"
    shift
    echo "[BUILD] $name"
    g++ -std=c++17 -O2 "$@" -o "$name"
}

build_cpp complete_scanner complete_scanner.cpp -pthread
build_cpp device_names device_names.cpp
build_cpp port_service_scanner port_service_scanner.cpp
build_cpp os_fingerprint os_fingerprint.cpp
build_cpp scanner_menu scanner_menu.cpp

if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libpcap; then
    build_cpp monitor_visits monitor_visits.cpp -lpcap
else
    echo "[WARN] libpcap not found; skipping monitor_visits build"
fi

if [[ -d "$ROOT_DIR/qt_gui" ]]; then
    echo "[BUILD] Qt GUI"
    cd "$ROOT_DIR/qt_gui"
    qmake >/dev/null
    make -j"$(nproc)" >/dev/null
    echo "[OK] GUI built at $ROOT_DIR/qt_gui/complete_scanner_gui"
fi

echo "[OK] Build complete. Run: sudo ./scanner_menu or ./launch.sh"
