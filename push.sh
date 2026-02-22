adb wait-for-device
adb push build/aiui.bin /RAW/NAND/600000
adb shell reboot hard
