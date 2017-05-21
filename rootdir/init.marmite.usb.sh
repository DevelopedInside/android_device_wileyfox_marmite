#!/system/bin/sh

# Allow USB enumeration with default PID/VID  TQY
#
boot_cust_mode=`cat /sys/class/android_usb/android0/usb_mode`
usb_current_config=`getprop sys.usb.config`
case "$boot_cust_mode" in
    "0") # ynn modify for user mode
        if [ "$usb_current_config" != "mtp,adb" ]; then
               echo "boot_cust_mode is 0"
               setprop sys.usb.config mtp,adb
               setprop sys.usb.configfs 0
               fi
        ;;
    *)
        if [ "$usb_current_config" != "diag,serial_smd,rmnet_bam,adb" ]; then
        echo "boot_cust_mode is 1 or null"
               setprop sys.usb.config diag,serial_smd,rmnet_bam,adb
               setprop sys.usb.configfs 0
        fi
        ;;
esac

#debug mode contol
debug_cust_mode=`cat /sys/class/android_usb/android0/mydebug_mode`
case "$debug_cust_mode" in
     "1") #debug enable
        echo "debug mode enable"
        echo 7 > /proc/sys/kernel/printk
        ;;
     *)
        echo "debug mode disable"
        echo 0 > /sys/devices/soc/78b0000.serial/console
        echo 0 > /sys/module/msm_poweroff/parameters/download_mode
        echo RELATED > /sys/bus/msm_subsys/devices/subsys0/restart_level
        echo RELATED > /sys/bus/msm_subsys/devices/subsys1/restart_level
        echo RELATED > /sys/bus/msm_subsys/devices/subsys2/restart_level
        echo RELATED > /sys/bus/msm_subsys/devices/subsys3/restart_level
        echo 6 > /proc/sys/kernel/printk
        ;;
esac

