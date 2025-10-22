#!/bin/bash

# Build and run parser, GUI plot and network cascade tests.
set -euo pipefail

ensure_fsnpview_binary() {
    if [[ -x ./fsnpview ]]; then
        return
    fi

    echo "fsnpview binary not found; building GUI application via qmake6..."
    qmake6 fsnpview.pro

    local jobs=1
    if command -v nproc >/dev/null 2>&1; then
        jobs=$(nproc)
    fi

    make -j"${jobs}"

    if [[ ! -x ./fsnpview ]]; then
        echo "Failed to build fsnpview binary" >&2
        exit 1
    fi
}

run_regression_test() {

pip install matplotlib numpy scikit-rf


    local missing_modules
    missing_modules=$(python3 - <<'PY'
import importlib.util
modules = ["matplotlib", "numpy", "skrf"]
missing = [name for name in modules if importlib.util.find_spec(name) is None]
if missing:
    print(" ".join(missing))
PY
)

    if [[ -n "${missing_modules}" ]]; then
        echo "Skipping lumped network regression test: missing Python modules: ${missing_modules}"
        echo "Install them with 'python3 -m pip install --user ${missing_modules}' to enable this test."
        return
    fi

    ensure_fsnpview_binary
    python3 tests/test_lumped_networks_cli_vs_skrf.py
}

./build.sh

qmake6
make -j"$(nproc)"

./parser_touchstone_tests
./tdrcalculator_tests
QT_QPA_PLATFORM=offscreen ./gui_plot_tests
./networkcascade_tests
./cascadeio_tests
./network_plot_style_tests
QT_QPA_PLATFORM=offscreen ./parameter_style_dialog_tests
QT_QPA_PLATFORM=offscreen ./plotmanager_selection_tests
QT_QPA_PLATFORM=offscreen ./plotmanager_mathplot_tests
QT_QPA_PLATFORM=offscreen ./plotmanager_tdr_marker_tests
QT_QPA_PLATFORM=offscreen ./cascade_wheel_tests
QT_QPA_PLATFORM=offscreen ./plotsettingsdialog_tests

run_regression_test
