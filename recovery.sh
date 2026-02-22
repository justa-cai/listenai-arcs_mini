adb push res/ap.bin /RAW/NAND/40000
adb push res/app-config.json /RAW/NAND/F0000
adb push res/tone.bin /RAW/NAND/100000
adb push res/wake_word.bin /RAW/NAND/200000
adb push res/respak.bin /RAW/NAND/400000
adb push build/aiui.bin /RAW/NAND/600000
adb shell reboot hard