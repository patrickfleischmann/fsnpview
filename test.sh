#!/bin/bash

# Build and run parser, GUI plot and network cascade tests.
set -e

./build.sh

./parser_touchstone_tests

if [ -x ./gui_plot_tests ]; then
    QT_QPA_PLATFORM=offscreen ./gui_plot_tests
else
    echo "Skipping GUI plot tests because Qt6 is not available."
fi

if [ -x ./networkcascade_tests ]; then
    ./networkcascade_tests
else
    echo "Skipping network cascade tests because Qt6 is not available."
fi

