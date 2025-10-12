#!/bin/bash

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check if hdepanel is in the same directory
if [ -f "$SCRIPT_DIR/hdepanel" ]; then
    export LD_LIBRARY_PATH="$SCRIPT_DIR:$LD_LIBRARY_PATH"
    exec "$SCRIPT_DIR/hdepanel" "$@"
# Check if hdepanel is in build/ directory
elif [ -f "$SCRIPT_DIR/build/hdepanel" ]; then
    export LD_LIBRARY_PATH="$SCRIPT_DIR/build:$LD_LIBRARY_PATH"
    exec "$SCRIPT_DIR/build/hdepanel" "$@"
else
    echo "Error: hdepanel binary not found in $SCRIPT_DIR or $SCRIPT_DIR/build/"
    exit 1
fi