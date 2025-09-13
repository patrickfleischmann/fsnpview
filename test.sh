#!/bin/bash
# Run parser and GUI plot tests.
set -e
./parser_touchstone_tests
QT_QPA_PLATFORM=offscreen ./gui_plot_tests
