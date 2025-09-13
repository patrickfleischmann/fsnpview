#!/bin/bash

# Run parser GUI plot and network cascade tests.
set -e
./parser_touchstone_tests
QT_QPA_PLATFORM=offscreen ./gui_plot_tests
./networkcascade_tests
