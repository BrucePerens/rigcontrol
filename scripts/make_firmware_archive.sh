PROJECT="K6BP Rigcontrol"
VERSION=`git log -n 1 --format=format:%ci`
DATE=`git log -n 1 --format=format:%cI`
rm -f -r firmware
mkdir firmware
cp -f bootloader/bootloader.bin partition_table/partition-table.bin k6bp_rigcontrol.bin firmware
cat > firmware/manifest.json <<-END
	{
	  "name": "${PROJECT}",
	  "version": "${VERSION}",
	  "builds": [
	    {
	      "chipFamily": "ESP32",
	      "parts": [
	        { "path": "bootloader.bin?date=${DATE}", "offset": 4096 },
	        { "path": "partition-table.bin?date=${DATE}", "offset": 32768 },
	        { "path": "k6bp_rigcontrol.bin?date=${DATE}", "offset": 65536 }
	      ]
	    }
	  ]
	}
END
cat > firmware/index.html <<-END
<!DOCTYPE html>
<HTML>
<HEAD>
  <META http-equiv="cache-control" content="no-cache, no-store, must-revalidate">
  <META http-equiv="Content-Type" content=
  "text/html; charset=utf-8">
  <TITLE>${PROJECT} Web Firmware Installer</TITLE>
  <SCRIPT type="module" src=
  "https://unpkg.com/esp-web-tools@8.0.2/dist/web/install-button.js?module"></SCRIPT>
</HEAD>
<BODY>
  <H1>${PROJECT} Firmware Installer</H1>
  <P>To install the ${PROJECT} firmware, connect the board to a computer via USB,
  and run a web browser on that same computer.</P>
  <P>You must use a <A href=
  "https://www.google.com/chrome/downloads/">Chrome</A> or <A href=
  "https://www.microsoft.com/en-us/edge">Edge</A> web browser.<BR>
  You must use the a computer with the Windows, MacOS, or Linux operating system.<BR>
  And you must use the <B>https</B> version of this page,
  plain http won't work.
  </p>
  <P>Connect your board via USB. Use the <B>top</B> USB
  connector on the board. That will provide <I>both</I>
  data and power.</P>
  <P>When the installer asks what serial port you wish to use,
  select the one with a <B>CP201 USB to UART Bridge Controller</B>,
  that is the USB chip on the board.</P>
  <P>If the installer asks if you want to erase the device, say
  <B>yes</B> if you are installing this program for the
  first time, or if there is a different program currently
  installed on the device. This will erase any saved settings.</P>
  <P>Push the button below to start the installer.</P>
  <p>When installation is done, use the menu option to configure the board's WiFi
  parameters.</p>
  <esp-web-install-button manifest="./manifest.json?date=${DATE}">
  </esp-web-install-button>
  <H2>Troubleshooting</H2>
  <p>
  <UL>
    <LI>If the program says
    <i>Serial port is not ready. Close any other application using it and try again:</i>
    Unplug and reconnect its USB interface.
    If that doesn't work, remove and restore
    power to the USB hub the card is connected to.</LI>
    <LI>If the browser opens an alert box that says
    <i>Failed to open serial port:</i>
     The first time you run this, you may have to grant yourself
    permission to open serial ports on your computer. You'll only
    need to do this once per user and computer:<BR>
      <P>On <B>Linux</B> use the command</P>
      <BLOCKQUOTE>
        <SPAN style="font-family: monospace">sudo adduser</SPAN>
        <I>your-user-name</I> <SPAN style=
        "font-family: monospace">dialout</SPAN>
      </BLOCKQUOTE>Then log out, and in again.
      <P>On <B>MacOS</B>, use these commands:</P>
      <BLOCKQUOTE>
        <SPAN style="font-family: monospace">sudo dseditgroup -o
        edit -a</SPAN> <I>your-user-name</I> <SPAN style=
        "font-family: monospace">-t user wheel</SPAN><BR>
        <SPAN style="font-family: monospace">sudo dseditgroup -o
        edit -a</SPAN> <I>your-user-name</I> <SPAN style=
        "font-family: monospace">-t user admin</SPAN>
      </BLOCKQUOTE>Then log out, and in again.
      <P>If you are running <B>Windows</B>, the system may ask for
      permission to access the serial device. Give it.</P>
      <P>Restricting permission to access serial ports was
      important when they were used for dial-out telephone modems.
      It's still there today.</P>
    </LI>
  </UL>
  </p>
  <p>
    <h2>Attribution</h2>
    <ul>
      <li><a href="https://github.com/BrucePerens/rigcontrol">K6BP Rigcontrol</a></li>
      <li><a href="https://esphome.github.io/esp-web-tools/">ESP Web Tools</a></li>
      <li><a href="https://www.improv-wifi.com/">Improv WiFi Protocol</a></li>
    </ul>
  </p>

</BODY>
</HTML>
END
tar clzf firmware.tar.gz firmware
