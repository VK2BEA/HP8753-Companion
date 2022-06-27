HP8753 Companion
================

Michael G. Katzmann (NV3Z/VK2BEA/G4NYV)
------------------------------------------------------------------

This is a program to augment functionality of the HP 8753 Vector Nework Analyzer:  
1. Save calibration and setup / Restore calibration & setup
2. Transfer trace data from HP8753 and display on Linux computer
       - also use the mouse to examine the trace (show source and response settings at corresponding mouse position)
3. Print and save traces as an image files (PNG or PDF)
4. Save trace data in CSV or Touchstone S2P format
5. Print traces to connected printer

Please see the YouTube description here:

[![HP8753 Companion video](https://img.youtube.com/vi/ORWQE22tbRo/0.jpg)](https://www.youtube.com/watch?v=ORWQE22tbRo)

To install on the Linux Fedora distribution:
-------------------------------------------
`sudo dnf -y copr enable vk2bea/GPIB`  
`sudo dnf -y copr enable vk2bea/HP8753`  
`sudo dnf -y install hp8753 linux-gpib-firmware`  

To build & install using Linux autotools, install the following required packages & tools:
----------------------------------------------------------------------
* `automake`, `autoconf` and `libtool`  
* To build on Raspberri Pi / Debian: 	`libgs-dev libglib2.0-dev libgtk-3-dev libsqlite3-dev`  
* To run on Raspberry Pi / Debian :	`libglib-2, libgtk-3, libgs, libsqlite3, libgpib, fonts-noto-color-emoji`

Install the Linux GPIB driver: 
See the `GPIB-Linux.driver/installGPIBdriver.on.RPI` file for a script that may work for you to download and install the Linux GPIB driver, otherwise, visit https://linux-gpib.sourceforge.io/ for installation instructions.

Once the prerequisites (as listed above) are installed, install the 'HP8753 Companion' with these commands:

        $ ./autogen.sh
        $ cd build/
        $ ../configure
        $ make all
        $ sudo make install
To run:
        
        $ /usr/local/bin/hp8753

To uninstall:
        
        $ sudo make uninstall
