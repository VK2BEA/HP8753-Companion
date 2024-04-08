HP8753 Companion
================

Michael G. Katzmann (NV3Z/VK2BEA/G4NYV)
------------------------------------------------------------------

This is a program to augment functionality of the HP 8753 Vector Nework Analyzer:  
1. Save calibration and setup / Restore calibration & setup
2. Transfer trace data from HP8753 and display on Linux computer
       - also use the mouse to examine the trace (show source and response settings at corresponding mouse position)
3. Save traces as image files (PNG (bitmap) or PDF (vector))
4. Acquire HPGL plot of screen
5. Save trace data in CSV or Touchstone S2P format
6. Print high resolution traces (or HPGL plot) to Linux CUPS printer
7. Import .XKT calibration kit definitions and send them to the HP8753 (optionally saving as a user kit)

Please see the YouTube description here:  
[![HP8753 Companion video](https://github.com/VK2BEA/HP8753-Companion/assets/3782222/f3309627-889d-4e28-8884-ea4f6471a7ff)](https://www.youtube.com/watch?v=ORWQE22tbRo)
[![HP8753 HPGL K+L](https://github.com/VK2BEA/HP8753-Companion/assets/3782222/14e20995-aede-409e-8829-9d42484e72bd)](https://www.youtube.com/watch?v=ORWQE22tbRo)

To install on the Linux Fedora distribution:
-------------------------------------------
`sudo dnf -y copr enable vk2bea/GPIB`  
`sudo dnf -y copr enable vk2bea/HP8753`  
`sudo dnf -y install hp8753 linux-gpib-firmware`  

To build & install using Linux autotools, install the following required packages & tools:
----------------------------------------------------------------------
* `automake`, `autoconf` and `libtool`  
* To build on Raspberri Pi / Debian: 	`libgs-dev libglib2.0-dev libgtk-3-dev libsqlite3-dev yelp-tools`  
* To run on Raspberry Pi / Debian :	`libglib-2, libgtk-3, libgs, libsqlite3, libgpib, fonts-noto-color-emoji`

Install the GPIB driver: 
See the `GPIB-Linux.driver/installGPIBdriver.on.RPI` file for a script that may work for you to download and install the Linux GPIB driver, otherwise, visit https://linux-gpib.sourceforge.io/ for installation instructions.

The National Instruments GPIB driver *may* also be used, but this has not been tested. The Linux GPIB API is compatable with the NI library.... quote: *"The API of the C library is intended to be compatible with National Instrument's GPIB library."*

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

Troubleshooting:
----------------------------------------------------------------------
If problems are encountered, first confirm that correct GPIB communication is occuring. 
<br/>__Warning__ *You must setup the GPIB controller and the GPIB address of the 8753 on the GPIB tab of the GUI. (press the ▶ icon on the notebook widget until the GPIB tab is shown, or right click on any tab and select GPIB from the list)*

Use the `ibtest` and `ibterm` tools distributed with the `linux-gpib` distribution.

The HP8753 Companion logs some information to the journal, the verbosity of which can be set with the `--debug` command line switch.

To enable debugging output to the journal, start the program with the `--debug 7` switch, <em>(Debug levels 0-7)</em>.

If started without the switch, the default logging verbosity is 3.

To view the output (in follow mode) use:

        journalctl -t hp8753 -f
