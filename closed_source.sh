#!/bin/sh
# do close source operations
#change by zyh 2018-03-28
export PATH=/home/diskc/home/zyh/.local/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi:$PATH
make imx_v7_defconfig
make zImage -j16
#for wm8960 closed source 
cp sound/soc/fsl/Makefile_backup sound/soc/fsl/Makefile
cp sound/soc/fsl/imx-wm8960.o sound/soc/fsl/imx-wm8960.mod
rm sound/soc/fsl/imx-wm8960.c
#for wm8960 closed source add by lixinguo
