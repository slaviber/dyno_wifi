How to build
--------------

- Unzip the archive.
- Setup the idf.py environment (eg. by running source {idf_dir}/esp-idf-v5.0.2/export.sh) in the terminal.
- Change the working directory to the dyno_wifi one.
- Run idf.py build
- Connect the hardware (eg. on port /dev/ttyUSB0)
- Run idf.py -p /dev/ttyUSB0 flash

Dependencies
------------

The ESP IDF v5.0.2 toolchain


Prerequisites
-------------

The workstation has to have physical (USB port) access to the hardware.

