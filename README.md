HP8753 Companion
================

Michael G. Katzmann (NV3Z/VK2BEA/G4NYV)  michaelk@IEEE.org
------------------------------------------------------------------

This is a program to augment functionality of the HP 8753 Vector Nework Analyzer:

1. Retrieve and save calibration and setup
2. Retrieve trace(s) and display on GPIB connected computer
       - Use mouse to examine the trace (see source and response settings at mouse position)
3. Save traces as an image file (PNG) or PDF
4. Print traces to connected printer

Install the following packages & tools:
---------------------------------------
* Automake
* Autoconf
* Libtool

On Fedora, install the gpib driver and user utilities and firmware by:
`sudo dnf copr enable vk2bea/GPIB`
`sudo dnf install dkms-linux-gpib linux-gpib linux-gpib-devel`
`sudo dnf install linux-gpib-firmware`

* To run on Raspberry Pi :		`libglib-2, libgtk-3, libgs, libsqlite3, libgpib, fonts-noto-color-emoji`
* To run on Fedora: 			`glib2, gtk3, libgs, sqlite-libs, libgpib, google-noto-emoji-color-fonts`
* To build on Debian (RPi) : 	`libgs-dev, libglib2.0-dev, libgtk-3-dev, libsqlite3-dev, https://linux-gpib.sourceforge.io/, yelp-tools`
* To build on Fedora/Redhat:	`gtk3-devel, glib2-devel, sqlite-devel, libgs-devel, libgipb-devel, yelp-tools`

To install:

        $ ./autogen.sh
        $ cd build/
        $ ../configure
        $ make
        $ sudo make check
        $ sudo make install
To run:
        
        $ /usr/local/bin/hp8753

To uninstall:
        
        $ sudo make uninstall
