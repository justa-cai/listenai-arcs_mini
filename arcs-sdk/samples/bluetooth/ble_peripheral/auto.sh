rm -f build/*
set -e
./build.sh -DBOARD=arcs_mini
./download.sh
