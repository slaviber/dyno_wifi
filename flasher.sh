#!/bin/bash
set -e
#idf.py -p /dev/ttyUSB0 -b 921600 flash
idf.py -b 921600 flash
stty -F /dev/ttyUSB0 icanon
#stty -F /dev/ttyUSB0 115200
stty -F /dev/ttyUSB0 921600
#cat /dev/ttyUSB0 > megasad.txt
cat /dev/ttyUSB0

