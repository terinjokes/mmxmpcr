# MMXMPCR

Author: Michael Minn
[\<mmxmpcr@michaelminn.com\>](mailto:mmxmpcr@michaelminn.com)

February 28, 2003

------------------------------------------------------------------------

*MMXMPCR, a kernel module and command line program for playing the XM
Radio PCR satellite radio receiver*

------------------------------------------------------------------------

## 1. Introduction

MMXMPCR is a simple program for playing the XM PCR USB satellite radio
receiver under Linux. The XM PCR is an inexpensive USB device that
permits listening to XM satellite radio through a PC. The PC only
provides the means for viewing and selecting channels. Audio output from
the device is analog via a 1/8\" phone jack that can be plugged into a
computer sound card or a home stereo system. Digital audio output is not
available from the device.

The XM PCR is recoginized as a USB serial device by the ftdi_sio kernel
module that is provided in most Linux distros and was built to support
the FTDI RS-232 USB serial port convertor. The communications protocol
has been partially deciphered and posted on the [XM Fan
Website.](http://www.xmfan.com) I also did some work using USB Snoopy
under Windoze. Although the protocol seems relatively simple, there are
some messages of unknown purpose that must be sent on faith. No attempt
has been made to find a way of defeating the subscription protection
that is built into the device. Since the future of XM Radio is tenuous
and dependent on income from a subscription base, I would encourage you
to support this fabulous resource rather than expending effort on trying
to get it for free.

MMXMPCR can operate either from the command line or a graphical user
interface. There are only two command line options: 1) power up the unit
and select a channel, or 2) select a channel on an already powered-up
unit. The user GUI program is a simple MOTIF list box that displays what
is currently playing on the various XM radio channels and permits the
user to change channels.

MMXMPCR is provided to the open source community to promote further
adoption of this device. There are other existing packages out there.
MMXMPCR is free software; you can redistribute it and/or modify it under
the terms of version 2 the GNU General Public License as published by
the Free Software Foundation. It is provided to the open source
community without a warranty of any kind.

I LOVE XM Radio and can\'t imagine how I lived my life without it. Get
it today!

------------------------------------------------------------------------

## 2. Version History & Download

**[Version
2004.02.28](http://www.michaelminn.com/linux/mmxmpcr/mmxmpcr-2004.02.28.tgz)**:
Use 25 09 to get channel data rather than 25 08, which was returning
unpredictable channel number data. Added command line options. Bug fix
for segfault in XmListReplaceItemsPos() with negative position index.

**[Version
2004.01.11](http://www.michaelminn.com/linux/mmxmpcr/mmxmpcr-2004.01.11.tgz)**:
Having problems selecting and displaying certain channels (namely 76 and
171). Changed channel selection command from \"10 01\" to \"10 02\" and
that fixed the selection problem. Did further exploration of the channel
info response codes to figure out the display problem.

**[Version
2003.12.11](http://www.michaelminn.com/linux/mmxmpcr/mmxmpcr-2003.12.06.tgz)**:
Add mouse wheel page scrolling. Filter out bogus channel info messages.
Refreshes only visible channels so refresh is faster. Added shortcut
keys - required significant modification of widget implementation. Fixed
scrolling and selection display problems from premature 12.06 release.

**[Version
2003.07.08](http://www.michaelminn.com/linux/mmxmpcr/mmxmpcr-2003.07.08.tgz)**:
Upgrade to use the ftdi_sio module - proprietary mmxmpcr module no
longer needed; Cleared up junk display due to overlapping channel info
request messages.

**[Version
2003.06.14](http://www.michaelminn.com/linux/mmxmpcr/mmxmpcr-2003.06.14.tgz)**:
Initial release that compiles under Red Hat 8.0

------------------------------------------------------------------------

## 3. Installation

MMXMPCR consists of a single C++ source file that compiles into a single
executable file (mmxmpcr). Download a tarball from the version list
above and AS SUPERUSER decompress the tarball and make install:

        tar -zxvf mmxmpcr*.tgz
        cd mmxmpcr
        make install

------------------------------------------------------------------------

## 4. Uninstall

MMXMPCR can be uninstalled by typing \"make uninstall\" in the source
code directory you created above. You can also go into /usr/local/bin
and manually delete the mmxmpcr executable.

------------------------------------------------------------------------

## 5. Operation

It is recommended that you go through the setup and subscription process
for the XM PCR under Windoze before attempting to use it under Linux.
The signal strength meter is important for troubleshooting setup and no
meter is provided with MMXMPCR.

There are three options for starting from the command line:

-   \"mmxmpcr -p n\" powers up the unit and selects a channel. No
    graphical interface starts.
-   \"mmxmpcr -c n\" selects a channel on an already powered up unit.
    The unit must already have gone through the initialization protocol.
    No graphical interface starts.
-   \"mmxmpcr\" with no command line options powers up the unit and
    starts the MOTIF graphical interface.

The initialization protocol takes a few seconds and if the GUI is
chosen, a list of channels should pop up. The list refreshes
sequentially and not very quickly. You can select a channel by clicking
on the appropriate list entry. Nothing will play until you select a
channel.

Channels are organized in groups of 10, which works out nicely for a
single page of display. You will notice large gaps in the list reserved
for unused channels. Selecting an unused channel will give unpredictable
behavior - usually nothing happens.

The MMXMPCR program can be terminated by clicking the close icon in the
title bar. The XM PCR will continue playing the last selected channel if
the MMXMPCR program is terminated. Sound can be stopped by by unplugging
the device, powering down the computer, or restarting MMXMPCR (which
resets the device).

MMXMPCR can also be operated with the following shortcut keys:

        PgUp: display up one page
        PgDn: display down one page
        0 - 9: select channel (i.e. if top channel on the page is 40, key 5 selects channel 45)
        q: quit

Fast, simple, effective. No pretty graphics, but this is about sounds,
not pictures.

------------------------------------------------------------------------

------------------------------------------------------------------------

## 6. Protocol

Courtesy of Bushing and Dobbz via the [XM Fan XMPCR Hardware &
Development Message
Board](http://www.xmfan.com/viewtopic.php?t=6870&sid=18085bac1e7ad8d0750ec3a324bf0b1a).
Some additional \"discoveries\" of mine follow their comments\...

    Requests from PC to PCR:

    All requests take the form:

    5A A5 xx xx  ED ED

    where xx xx is the length of the data field in big-endian (MSB first) format

    For example:

    5A A5 00 05 00 10 10 01 ED ED -- power unit on


    Known commands:
    00 10 10 10 01     -- power on. see response 80
    01 00              -- power off. see response 81
    10 02              -- select channel
    13 00              -- ? see response 93
    25 08 xx 00        -- get channel info. see response A5
    25 09 xx 00        -- get channel info. see response A5
    31                 -- get radio ID. see response B1
    42 01              -- ? see response C2
    43                 -- get signal strength info. see response C3
    50 00 00 00 00 00  -- ?
    70 05              -- get radio info? see response E3


    Responses from PCR to PC:

    All responses take the form:

    5A A5 xx xx  yy yy

    where xx xx is the length of the data field in big-endian format
          yy yy is the last two bytes of the data field, repeated (why?)

    Many response codes seem to be the request codes with the MSB set high.

    Known responses:

    80: HELLO
    80 ?? ?? ?? aa bb cc dd ee ?? ?? ?? ?? ff gg hh ii jj ?? 
       aa         "SDEC Ver." in BCD
       bb/cc/ddee    SDEC revision date, in BCD
             (note: My PCR returns 25 10 29 20 02, but diag.exe
             refers to this as "SDEC Ver. 25 (10/02/2002)) ???
       ff
            gg/hh/iijj    XMSTK version info
        XM ID in ASCII, 8 characters long
       Sent on poweron. See request 00 01.

    81: GOODBYE
    81 ?? ?? ??
        Sent on power down.  See request 01.

    93: ??
    93 ?? ?? ??
        See request 13 00.

    A5: CHINFO (see below for further notes on this response)
    A5 ?? ?? aa bb ?? XX XX XX XX XX XX XX XX XX XX
    XX XX XX XX XX XX ?? ?? YY YY YY YY YY YY YY YY
    YY YY YY YY YY YY YY YY ?? ZZ ZZ ZZ ZZ ZZ ZZ ZZ
    ZZ ZZ ZZ ZZ ZZ ZZ ZZ ZZ ZZ WW WW WW WW WW WW WW
    WW WW WW WW WW WW WW WW WW ?? ?? ?? ??

       aa   Channel number.  This is one two low when 25 09 is used?
       bb    Usually the same as the channel number -- but not always.
       XX   Station name.  (16 chars)
       YY    Genre name.    (16 chars)   all are padded with trailing
            ZZ   Artist name.   (16 chars)   whitespace (0x20)
       WW   Song title.    (16 chars)
        See request 25 08, 25 09.

    B1: GETID
    B1 ?? ?? ?? XX XX XX XX XX XX XX XX
       XX   XM ID, in ASCII. (8 chars)
       See request 31.

    C1: ??
    C1 ??

    C2: ??
    C2 ?? ??
       See request 42 01.

    C3: Signal strength?
    C3 (lots and lots of numbers. :)
       See request 43.

    D0: ??
    D0 ?? ?? ??
       See request 50.

    E0: Some sort of seperator or ACK?
    E0

    E3: XM INFO
    E3 ?? ?? ?? XX XX XX XX XX YY YY YY YY YY ZZ ZZ ZZ ZZ ZZ
       XX CBM Version
       YY XMSTK Version
       ZZ SDEC Version   (see 80 for more details)
       See request 70 05. 


    Issue 5AA5000425090000EDED, and it will return a channel. Then you ack 
    with 5AA500042509 xx 00EDED for whatever channel it just sent. It will 
    then send you the next channel in the list. Repeat until it finishes. 

    ------------------- Additional Information -------------------------

    Select (play) a channel (xx is the channel number)

    5A A5 00 06 10 02 xx 00 00 01 ED ED


    Channels can also be selected with the following command,
    although some channels will not select, so the command
    above should be used instead of this

    5A A5 00 06 10 01 xx 00 00 01 ED ED


    Request channel info (A5 CHINFO above) commands are 25 08 and 25 09. 
    With 25 08 there is some meaning to "aa" and "bb". aa is always the 
    channel number, but sometimes bb is not the same as aa, even if the 
    channel exists.  If the channel doesn't exist, they are always
    different, although exactly what this means is uncertain. Also, the
    returned data is data for the last valid channel info request.

    With 25 09, aa, bb, and the returned data are always only for 
    valid channels. Request for an invalid channel will yield a response
    with data for the next valid channel.

    When changing channels, the XMPCR also issues a "50" request before and after the
    channel change request. These commands do not appear to be necessary since 
    MMXMPCR works fine without them. This is an example sequence, where xx is the
    current channel and yy is the channel being changed to.

    5a a5 00 06 50 xx 00 00 00 00 ED ED
    5a a5 00 06 10 02 yy 00 00 01 ED ED
    5a a5 00 06 50 yy 01 01 01 01 ED ED
