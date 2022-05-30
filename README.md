# K6BP RigControl

* There's a discussion group
  [here](https://github.com/BrucePerens/rigcontrol/discussions/). Please speak up,
  *there are no stupid questions.* I'll be making my progress reports and answering
  questions there.
* Support this and other Open Source and Open Hardware projects for Amateur Radio by
[Joining HamOpen.org](https://HamOpen.org/)

This is the K6BP RigControl software, an Amateur Radio transceiver
controller using an ESP-32 Audio Kit card commonly sold on AliExpress for
around $14.
[Search for sales on AliExpress](https://www.aliexpress.com/premium/ESP32%25252dAudio%25252dKit.html).

RigControl connects to the internet via WiFi, sets itself up with a public web address,
and provides a web-based interface to control an amateur radio transceiver.

Without any additional hardware, K6BP RigControl
can connect to any transceiver that implements *CAT* or *CI-V* using 5V or 3.3V
logic levels. Simple resistor pads may be necessary to adjust the input and output
audio levels, but construction of them should be within the competence of any ham.

### Project Status

The software is currently in "Hello World" stage: No ham radio control
is implemented yet.
Most of the work done so far is concerned with setting up the system with an
address on the public internet.
user.
Of course, it's easiest if the user has forwarded the proper ports on their router, but
the software can set up a public internet address even when operated by a net-naive
user.

The systems facilities required new embedded protocol
handlers for SSNTP, ICMPv6, STUN, PCP, REST, Dynamic DNS, a new embedded event-driven I/O
facility, and a new facility for submitting jobs to run in existing FreeRTOS threads.
It will now require a TURN implementation that is not yet started.
On a Linux system, these facilities would
have been available out-of-the-box, but on the ESP-32 running FreeRTOS and LwIP, new
implementations had to be coded for small size and efficiency.

You can see this code in the
[Generic Main](https://github.com/BrucePerens/rigcontrol/tree/main/components/generic_main)
component. This is usable in other Open Source ESP32 applications *only,* and those must
be under a license that is compatible with AGPL3, and the entire application must comply
with the source-code distribution terms of the AGPL3.
Use in proprietary software would require a commercial license from the author.

### Hardware Required

ESP32 Audio Development Kit sold on AliExpress.com .

![ESP32 Audio Development Kit](website/esp32-audio-kit2.png)

The design of this appears to originate with AI-Thinker [(site)](https://docs.ai-thinker.com/en/esp32-audio-kit)
derived from a design by Espressif called the LyraT
[(site)](https://www.espressif.com/en/products/devkits/esp32-lyrat).
The boards available on AliExpress.com are either the AI-Thinker product or
follow it closely. The LyraT board is also available, but has several differences
from the ESP32-Audio-Kit: a different pinout, different audio ICs that may require
driver changes, and power and speaker connectors that are half the size of those on the
ESP32-Audio-Kit.

* [Board schematic.](website/esp32-audio-kit_v2.2_sch.pdf)
* [Module specification.](website/esp32-a1s_v2.3_specification.pdf)
* [ESP32 audio design guide.](website/esp32_audio_design_guidelines__en.pdf)
* [Audio chip datasheet](website/ES8388_DS.pdf)

The board provides two audio inputs and two
audio outputs. Microphone or line inputs are switched through a multiplexer.
Line and speaker outputs are in common, with enable lines for the speaker
amplifiers.
The audio outputs are ususally fed by ADCs, but can be switched to the
line inputs and microphone preamplifiers, for debugging.
There is a USB serial interface
for debugging, and additional serial and digital outputs.
Logic inputs are driven to 3.3 volts, but are 5V tolerant when neither of the
internal pull-up and pull-down resistors are configured. This allows direct control
of logic-level interfaces such as CAT and CI-V, since the 3.3V output will be
sensed as a logic high by 5V logic.
CW and RTTY control is possible but not yet
implemented in software.

### Software Required

Your system must have a native C compiler which can be run using the command `cc`,
and must have the zlib compression library installed so that `cc -lz` finds it.
These are used to build a native generator for the compressed filesystem.
On Debian, the zlib compression library is in the `zlib1g-dev`
package. Other Linux systems should be similar. On Windows, follow the instructions
[here](https://github.com/horta/zlib.install). Source code is [here](http://zlib.net).

Install *git* if you haven't yet done so, using the instructions
[here](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git).

Download the ESPTouch App from the app store:
[Android](https://play.google.com/store/apps/details?id=com.khoazero123.iot_esptouch_demo), [iOS](https://apps.apple.com/us/app/espressif-esptouch/id1071176700)

Install the esp-idf, using the instructions from
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html

You might need to learn some command-line basics for your system if you don't have them.
Open a command-line processor. Use the command line to change the current directory to
the folder where you'd like to keep your work on this project.
Use *git* to create a folder called *rigcontrol* in there, containing a copy of the source code:
``` shell
  git clone https://github.com/BrucePerens/rigcontrol.git
```
Change to the newly created *rigcontrol* folder. That's where you'll work on the software.

### Build and Flash

On Linux, using *bash*, run the `esp-idf/export.sh` environment script to set variables
in your command-line environment. Assuming that
`esp-idf` is installed in the parent directory:
```shell
source ../esp-idf/export.sh
```
The `source` command is important here. That runs the script in the current command-line shell, so it
sets its variables. Just running the script without that doesn't do anything, quietly, and then
when you go to run `idf.py`, the shell won't be able to find it.

On Windows, see the installation instructions on how to run a PowerShell or
command prompt environment.

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

The `flash` command includes the `build` command, but feel free to run that separately.

Run the ESPTouch app to set the WiFi parameters of the board, *or* use the console
commands `param ssid `*value* and `param wifi_password`*value*.

Open your web browser to the IP address printed by ESPTouch or the serial
monitor, in the form http://*address*/

If your home router provides dynamic DHCP addresses for devices on your LAN,
the device name will be *rigcontrol* until you set it. So you can try
opening *http://rigcontrol.lan/* if your router provides a *.lan* domain.
On some browsers, just typing *rigcontrol.lan/* will work.
Multicast DNS will be impmented, which will make the web interface available
on the local network as something
like *rigcontrol._https._tcp.local* .

Every time you'd like to catch up to changes in software, use this command:
```shell
git pull
```
to update your copy of the source code. Then, go through the build and monitor
process above, except use `app-flash` instead of `flash`. That is faster than
`flash` because it only flashes the
application, which would have changed; rather than the boot loader, partition table,
etc.


### Accessories ###
It's often possible to reduce the price of these accessories
through a club buy, since they come in quantities at a lower price and shipping
fee than single
units. Start with AliExpress, but explore Alibaba and Taobao for even larger
wholesale quantities and better prices. 
Larger buyers build a relationship with a
[sourcing agent](https://leelinesourcing.com/sourcing-agent/) in Shenzen,
who deals with translation, aggregates shipping, and has access to
manufacturers that don't trouble to operate an English-language storefront.

The ESP Audio Kit has precious few free GPIO pins, just enough for a serial
port and PTT control.
Planned expansion is
via an I2C bus, a two-wire bus.
Detailed technical information on I2C is
[here](https://www.nxp.com/docs/en/user-guide/UM10204.pdf).

Current prospects for expansion are the
[SC16IS752 dual UART](https://www.aliexpress.com/wholesale?SearchText=SC16IS752)
and the [PCF8575 16-pin I/O expander](https://www.aliexpress.com/wholesale?SearchText=PCF8575). The PCF8575 can work with 3.3V or 5V logic levels, depending on the power supplied.
These boards both
have programmable addresses in the range of 0x20-0x30, via solder-bridging
3 address pins.
Theoretically a combination of 16 expansion boards could be supported, in
practice this is much more than anyone should need.

For directly converting the levels of the GPIO pins, the [TXS0108E 8-pin bi-directional logic converter](https://www.aliexpress.com/wholesale?SearchText=TXS0108E) is an inexpensive way
to convert 8 pins to 5V logic levels, and we
don't really need the bi-directional feature. But the PCF8575 is probably a
better approach for external I/Os.

RS-485 to 5V TTL serial interface boards for with built-in lightning protection are
available for less than a dollar. The software can impement the MODBUS protocol.
This makes expansion over a bus thousands of feet in length possible.

The main I/O header is a 2x8 pin double-row header socket with 0.1 inch pitch.
The JTAG header is single-row, and two of the pins on that connector can be
used for I/O if you aren't debugging the software with JTAG.
The connectors for the DC input (and the speaker outputs, which we don't use)
are JST male connectors, the correct ones look like the one on the lower
right in
[this photo](https://en.wikipedia.org/wiki/JST_connector#/media/File:JST_RCY.JPG).
They are available
[on AliExpress](https://www.aliexpress.com/wholesale?SearchText=JST), but make
sure the ones you buy match the photo above.
They are also [on Amazon](https://www.amazon.com/gp/product/B071XN7C43/) in
male-female pairs.
You don't need these connectors if you are powering the device with an
old micro-USB power supply. The better ones come pre-assembled with 20
gauge black and red silicone-insulated wire pigtails. You can get vinyl
insulation for less, but be careful soldering: it melts.

### Why Not Raspberry PI, or Some Other Single-Board Computer? ###
This is meant to be a functional and fun project that most Radio Amateurs can do on
their own, and in a short time. Thus, the bill of materials (BOM) is both simple and
cheap, connections are simple and can be hand-soldered without any
skill in surface-mount soldering.
It is possible to connect many radios with *no* additional hardware other than connectors,
wire, and an old micro-USB power supply.

All of the other platforms I have looked at require additions to the BOM such as
SD cards, and daughter cards to add audio or networking.
That said, a platform that runs Linux or BSD is definitely superior.
It can run tasks that the ESP-32 *won't* do: for
example the ESP-32 doesn't have a USB master, to connect to modern rigs with USB,
and its hardware floating-point performance is too poor to support CODEC2. And Linux
can support many more, and more powerful, apps, and run them simultaneously.

Prices change, and new hardware arrives. Appropriate SD cards below the $4 level are
available overseas, although there is much fraud. So, when you find a combination that
does it all with a simple and cheap BOM, I'm interested.

### Issues You May Experience
If flashing fails and console characters drop, switch to the shortest USB cable you have
on hand, preferrably one with ferrite beads attached for RFI reduction. I found a foot-long one which improved performance over the 6-foot I had previously been using.

When plugging in the board doesn't attach the USB serial device to the operating system, I have been able to fix that by power-clearing the USB hub that I have the board plugged in to. Power clearing the board wasn't sufficient, the problem was in the hub.

### RigControl Access to Outside Web Sites ###
The point of RigControl is to make control of your radio accessable from the
public internet, appropriately password-protected. So, it does all sorts of things
with your firewall and outside sites:
* It will attempt to create a pinhole in your firewall using NAT-PMP or PCP so that it is directly accessible on the public internet.
* If not, it will use TURN to get around your firewall, as VoIP phones often do.
* It will use STUN to check its public IP.
* It will use an outside site to check that it can actually be accessed from the public internet.
* It will set its hostname in dynamic DNS at an outside site, so that you can access it
remotely.
* It calls an outside site to check for firmware updates and security bulletins.

### TO DO
* UPnP. It's overcomplicated compared to NAT-PMP and PCP, but some routers may have UPnP and not the other two.
* Access-point mode. Should just be activating existing APIs.
* mDNS. Should just be activating existing APIs.
* Web configuration for the parameters that currently work from the command line.
* All of the ham radio functions.

### Warning
Before you release production binaries of this application, rebuild your toolset to use a 64-bit time_t, so that the software doesn't stop working in 2038. Instructions
are at https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html#bit-time-t
