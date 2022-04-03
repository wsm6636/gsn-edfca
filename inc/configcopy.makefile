ARCH           = arm                 
CFLAGS         = -O2 -Wall -Werror -g -Wdeclaration-after-statement
LDFLAGS        = 
LDLIBS         = -L${LIBLITMUS} -llitmus 
CPPFLAGS       = -D_XOPEN_SOURCE=600 -D_GNU_SOURCE  -DARCH=arm -I${LIBLITMUS}/include -I${LIBLITMUS}/arch/arm/include -I${LIBLITMUS}/arch/arm/include/uapi -I${LIBLITMUS}/arch/arm/include/generated/uapi
CC             = /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-gcc --sysroot=/opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/cortexa9hf-neon-poky-linux-gnueabi -mfloat-abi=hard
LD             = /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-ld
AR             = /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-ar
CXX	       = /opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-g++
