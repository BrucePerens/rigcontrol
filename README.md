# K6BP RigControl

This is the K6BP RigControl software, an Amateur Radio transceiver
controller using an ESP-32 Audio Kit card commonly sold on AliExpress for
around $14.
[Search for sales on AliExpress](https://www.aliexpress.com/premium/ESP32%25252dAudio%25252dKit.html).

The software is currently in "Hello World" stage: it can configure its
WiFi, set the time from SNTP, start a web server, and access the public
Internet.
Some debugging and maintenance commands are implemented on the USB serial
console. No ham radio control is implemented yet.

Without any additional hardware, K6BP RigControl
can connect to any transceiver that implements *CAT* or *CI-V* using 5V or 3.3V
logic levels or a network connection.
RS-232 connections require an inexpensive [MAX-232-based level shifter + DB9 PCB](https://www.aliexpress.com/wholesale?SearchText=MAX232+DB9).
RigControl connects to the internet via WiFi, uses
dynamic DNS to provide a public web address, and provides a web-based interface
to a phone, tablet, or computer. The audio inputs and outputs are best connected
to line-level inputs and outputs on the transceiver, and would require padding
with resistors if connected to a speaker output or microphone input. There is no
USB master. Inexpensive expansion boards support connecting antenna switches, rotators,
and other accessories via relays or serial ports.

### Hardware Required

ESP32 Audio Development Kit sold on AliExpress.com .

![ESP32 Audio Development Kit](website/esp32-audio-kit2.png)

The design of this appears to originate with AI-Thinker [(site)](https://docs.ai-thinker.com/en/esp32-audio-kit)
derived from a design by Espressif called the LyraT
[(site)](https://www.espressif.com/en/products/devkits/esp32-lyrat).
The boards available on AliExpress.com are either the AI-Thinker product or
follow it closely. The LyraT board has a different pinout.


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

Download the ESPTouch App from the app store:
[Android](https://play.google.com/store/apps/details?id=com.khoazero123.iot_esptouch_demo), [iOS](https://apps.apple.com/us/app/espressif-esptouch/id1071176700)

Install the esp-idf, using the instructions from
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html
### Build and Flash

On Linux, using *bash,* run the esp-idf environment script. Assuming that
`esp-idf` is installed in the parent directory:
```
source ../esp-idf/export.sh
```
On Windows, see the installation instructions on how to run a PowerShell or
command prompt environment.

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

Run the ESPTouch app to set the WiFi parameters of the board.

Open your web browser to the IP address printed by ESPTouch or the serial
monitor, in the form http://*address*/

If your home router provides dynamic DHCP addresses for devices on your LAN,
the device name will be *rigcontrol* until you set it. So you can try
opening *http://rigcontrol.lan/* if your router provides a *.lan* domain.
On some browsers, just typing *rigcontrol.lan/* will work.
Multicast DNS will be impmented, which will make the web interface available
on the local network as something
like *rigcontrol._https._tcp.local* .

### Accessories ###
It's often possible to reduce the price of these accessories
through a club buy, since they come in quantities at a lower price than single
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

*CAT* and other radio interfaces that use TTL levels won't need this, but for
level-shifting the serial port to meet the RS-232 specification,
our current prospect is the
[MAX-232-based level shifter + DB9 PCB](https://www.aliexpress.com/wholesale?SearchText=MAX232+DB9).
For directly converting the levels of the GPIO pins, the [TXS0108E 8-pin bi-directional logic converter](https://www.aliexpress.com/wholesale?SearchText=TXS0108E) is an inexpensive way
to convert 8 pins to 5V logic levels, and we
don't really need the bi-directional feature. But the PCF8575 is probably a
better approach for external I/Os.

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

### Why Is My RigControl Board Accessing Outside Web Sites? ###
RigControl must access an outside site to learn its own public IP address,
and it chooses randomly from a list of such sites. It must access an outside
site to set its hostname in dynamic DNS, so that you can access it remotely.
It calls github.com, k6bp.com, or hams.com to check if its software
is up-to-date amd to access security bulletins, etc.
Hams.com or k6bp.com may provide you with remote access.

### Issues
Hardware flow control is not connected on the PCB between the CPU and the
serial-to-USB chip. This saves some precious GPIO pins, but causes the
console to drop characters sometimes.
This also may be the reason that flashing the device sometimes fails.
The console is for debugging and maintenance, not everyday use of the
device, so
this should not generally present a problem to the user.

Changing the console
baud rate is fraught with difficulty. Setting the baud rate of the embedded
software and tools must be done in a specific order, or the device becomes
unflashable with the tools using a simple command-line like "idf.py flash".
It is possible to construct a command-line that works.
New hardware comes with the baud rate set to 115200, and reconfiguring
the tool baud rate makes new hardware unflashable with the usual command-line
as well.
