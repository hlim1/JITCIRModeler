#!/usr/bin/env bash

set -e

# Update the following to appropriate paths.
PIN=""
TOOL=""
PY=""

# Check required files
if [ ! -x "$PIN" ]; then
    echo "ERROR: PIN not found or not executable: $PIN"
    exit 1
fi

if [ ! -f "$TOOL" ]; then
    echo "ERROR: TOOL not found: $TOOL"
    exit 1
fi

if [ ! -f "$PY" ]; then
    echo "ERROR: IRModeler.py not found: $PY"
    exit 1
fi

echo "Running Pin:"
echo "$PIN -t $TOOL -- $@"
echo ""

"$PIN" -t "$TOOL" -- "$@"

echo ""
echo "Running IRModeler.py"
python3 "$PY"
