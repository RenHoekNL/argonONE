# argonONE
Standalone daemon for Argon40 One Pi 4 case

This release is messy and carries the 'It works for me!' label. Use
at your own risk.

## Background 
These sources are compiled with 'musl' on a Raspbian distro. The result is
a ~25kb static executable that will not have the python dependency that
the original script has.

## Installation
I need the following files:

`ls -l /dev/vcio /dev/i2c-1 /sys/class/gpio/export /sbin/shutdown`

You can get the i2c-1 device on lakka by editing the config.txt file:
```
mount -o remount,rw /flash
cp -a /flash/config.txt /flash/config.ori            # Make a backup
echo 'dtparam=i2c_arm=on'   >  /flash/config.txt     # Enable I2C device
echo 'hdmi_force_hotplug=1' >> /flash/config.txt     # Personal preference, you can skip this one
cat /flash/config.ori       >> /flash/config.txt     # Add the old stuff
sync
mount -o remount,ro /flash
reboot
```
After reboot, check /dev for the new device

Now to install the daemon, make sure it boots at boot and enable the poweroff
```
cd /storage/.config/
wget https://github.com/RenHoekNL/argonONE/raw/master/argonONE
chmod a+x argonONE
ln -s argonONE argonONE.poweroff

cat << 'EOF' > /storage/.config/autostart.sh
(
 /storage/.config/argonONE &
) &
EOF

cat << 'EOF' > /storage/.config/autostart.sh
/storage/.config/argonONE.poweroff
EOF
```
