#!/bin/bash
# This script tests the fsnpview application.
set -e

# Run the application with a test file in the background and redirect output
QT_QPA_PLATFORM=offscreen ./fsnpview test/a\ \(1\).s2p > test_output.log 2>&1 &
pid=$!

# Wait for a few seconds for the application to start and process the file
sleep 2

# Check if the process is still running
if ! kill -0 $pid; then
  echo "Test failed: Application exited unexpectedly."
  cat test_output.log
  exit 1
fi

# Check the output for the expected string
if grep -q "Hello, path is" test_output.log; then
  echo "Test passed: Application started successfully and processed the file."
  kill $pid
  rm test_output.log
  exit 0
else
  echo "Test failed: Application did not produce the expected output."
  cat test_output.log
  kill $pid
  rm test_output.log
  exit 1
fi
