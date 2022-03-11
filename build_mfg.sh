#!/bin/bash 
make distclean
make imx_v7_mfg_defconfig
make zImage -j16
make dtbs
make modules -j16
