#!/sbin/sh

if [ -d /sys/bus/i2c/drivers/AW87319_PA/2-0058 ]
then
cat /system/vendor/etc/mixer_paths_AW87319.xml > /system/vendor/etc/mixer_paths_mtp.xml
fi
