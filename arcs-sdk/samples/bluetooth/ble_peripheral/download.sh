#!/bin/bash
sudo pkill picocom
sleep 0.5
set -e
cskburn -C arcs -s /dev/ttyACM0 -b 3000000  0x0 ./build/arcs.bin
rm -fv log.txt
sudo picocom -b 921600 /dev/ttyACM0  --imap lfcrlf --logfile log.txt
