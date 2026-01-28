#!/bin/sh
mkdir -p /sys/kernel/config/device-tree/overlays/rylr998
cat /home/ubuntu/roc-rk3328-cc-kernel-drivers/rylr998_driver/rylr998.dtbo > /sys/kernel/config/device-tree/overlays/rylr998/dtbo
