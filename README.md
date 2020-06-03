# argonONE
Standalone daemon for Argon40 One Pi 4 case

This release is messy and carries the 'It works for me!' label. Use
at your own risk.

These sources are compiled with 'musl' on a Raspbian distro. The result is
a ~25kb static executable that will not have the python dependency that
the original script has.

The only things it needs are the following device files:

- /dev/vcio                 - Reading temperature
- /dev/i2c-1                - Controlling fan and poweroff
- /sys/class/gpio/export    - Keeping track of the power button clicks

And optionally:
- /sbin/shutdown            - Controlled shutdown

You can get the i2c-1 device on lakka by editing the config.txt file:
```
mount -o remount,rw /flash
sed -i.ori 's~^\(force_turbo\)~dtparam=i2c_arm=on\n\1~' /flash/config.txt
sync
mount -o remount,ro /flash
reboot
```
After reboot, check /dev for the new device



/storage/roms/scripts/run_scripts
