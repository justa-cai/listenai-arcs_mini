adb reboot recovery
sleep 2
adb wait-for-device
adb push build/arcs-mini.bin /RAW/NAND/700000
adb push build-remote-ap/remote-ap.bin /RAW/NAND/40000
sudo killall -USR1 picocom
adb shell reboot
