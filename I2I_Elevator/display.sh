sudo rmmod display_mod.ko
sudo rm /dev/touch_dev

sudo insmod display_mod.ko
sudo sh touch_mknod.sh
sudo ./touch_app