# argonONE
Standalone daemon for the [Argon40 One Pi 4 case](https://www.argon40.com/argon-one-raspberry-pi-4-case.html)

This release is messy and carries the 'It works for me!' label. Use
at your own risk.

Note that this daemon will do two things:
* Tturn on and speed up the fan when the temperature goes higher
* Watch the power button and do a clean shutdown on a double click

Holding the power button for 3 seconds or longer will do a hard (i.e. unclean) shutdown and can cause filesystem corruption. This is done in hardware and the daemon is unable to respond to it.

## Background 
The ArgonONE case comes with a Python script to respond to powerbutton presses
and to set the fan speed depending on the CPU temperature. The Python dependency
is not always easy to fix, so I recreated the functionailty in a C program.

These sources are compiled with 'musl' on a Raspbian distro. The result is
a ~25kb static executable that has no dependencies on any libraries. It should
run on basically any Pi4 distro.

## Installation on RetroPie
Download RetroPie here: https://retropie.org.uk/download/#Pre-made_images_for_the_Raspberry_Pi

Once you are up and running and in a SSH shell, do the following:
```
sudo bash -login
raspi-config nonint do_i2c 0
cd /usr/local/sbin/
wget https://github.com/RenHoekNL/argonONE/raw/master/argonONE
chmod a+x argonONE
sed -i 's~^exit 0~/usr/local/sbin/argonONE \&\nexit 0~' /etc/rc.local
ln -s /usr/local/sbin/argonONE /lib/systemd/system-shutdown/argonONE.poweroff
sync
reboot
```
Once booted, double-clicking the power button will do a shutdown and shutdown via the menu will turn the boards power off.

## Installation on Lakka
*The following instructions are tailored to the [Lakka](http://www.lakka.tv/get/linux/rpi4/) distro:*

The daemon has the following requirements:

`ls -l /dev/vcio /dev/i2c-1 /sys/class/gpio/export /sbin/shutdown`

You can get the i2c-1 device by editing the config.txt file:
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

Now to install the daemon, make sure it starts at boot and enable the poweroff
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

cat << 'EOF' > /storage/.config/system.d/argonONE.service
[Unit]
Description=ArgonONE poweroff

[Service]
Type=oneshot
RemainAfterExit=true
ExecStop=/storage/.config/argonONE.poweroff

[Install]
WantedBy=multi-user.target
EOF
systemctl enable argonONE
systemctl start argonONE
```

## Installation on Raspbian
- Enable the I2C device. See the instructions above. Instead of `/flash`, use `/boot`
```
cd /usr/local/sbin
wget https://github.com/RenHoekNL/argonONE/raw/master/argonONE
chmod a+x argonONE
ln -s /usr/local/sbin/argonONE /lib/systemd/system-shutdown/argonONE.poweroff
```
- Edit `/etc/rc.local` and put in `/usr/local/sbin/argonONE &`
