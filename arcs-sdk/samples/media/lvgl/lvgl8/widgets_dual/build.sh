#!/bin/bash

set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
exec "$SCRIPT_DIR/../widgets/build.sh" -S "$SCRIPT_DIR" "$@"
