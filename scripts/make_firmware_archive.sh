VERSION=`git log -n 1 --format=format:%ci`
rm -f -r firmware
mkdir firmware
cp -f bootloader/bootloader.bin partition_table/partition-table.bin k6bp_rigcontrol.bin firmware
cat > firmware/manifest.json <<-END
	{
	  "name": "K6BP Rigcontrol",
	  "version": "${VERSION}"
	  "builds": [
	    {
	      "chipFamily": "ESP32",
	      "parts": [
	        { "path": "firmware/bootloader.bin", "offset": 4096 },
	        { "path": "firmware/partition-table.bin", "offset": 32768 },
	        { "path": "firmware/k6bp_rigcontrol.bin", "offset": 65536 }
	      ]
	    }
	  ]
	}
END
tar clzf firmware.tar.gz firmware
