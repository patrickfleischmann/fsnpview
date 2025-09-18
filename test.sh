#!/bin/bash

# Build and run parser, GUI plot and network cascade tests.
set -e

./build.sh

./parser_touchstone_tests
./tdrcalculator_tests
QT_QPA_PLATFORM=offscreen ./gui_plot_tests
./networkcascade_tests

