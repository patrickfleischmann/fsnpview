#!/bin/bash
# This script installs the dependencies for the fsnpview project on Debian-based systems.
set -e
sudo apt-get update && sudo apt-get install -y build-essential qt5-qmake qtbase5-dev libeigen3-dev
