#
# Makefile for the kernel security code
#
subdir-ccflags-y := -O3 -fassociative-math -fasynchronous-unwind-tables -feliminate-unused-debug-types -fexceptions -fno-semantic-interposition -fno-signed-zeros -D_FORTIFY_SOURCE=2 \
-fno-strict-aliasing \
-fno-trapping-math -pipe -ffast-math \
-funroll-loops -fforce-addr -fomit-frame-pointer -mtune=cortex-a75.cortex-a55 -mcpu=cortex-a75.cortex-a55+crc+crypto+fp16+simd+sve \
-fno-stack-protector -pthread -Wall -fwrapv --param=ssp-buffer-size=32 -D_REENTRANT \
-floop-parallelize-all -floop-interchange -ftree-loop-distribution -floop-strip-mine -floop-block -floop-optimize -floop-nest-optimize -fprefetch-loop-arrays -ftree-loop-vectorize \
-ftree-vectorize \
-fPIE




obj-$(CONFIG_KEYS)			+= keys/
subdir-$(CONFIG_SECURITY_SELINUX)	+= selinux
subdir-$(CONFIG_SECURITY_SMACK)		+= smack
subdir-$(CONFIG_SECURITY_TOMOYO)        += tomoyo
subdir-$(CONFIG_SECURITY_APPARMOR)	+= apparmor
subdir-$(CONFIG_SECURITY_YAMA)		+= yama
subdir-$(CONFIG_SECURITY_LOADPIN)	+= loadpin
subdir-$(CONFIG_ARCH_QCOM)	+= pfe

# always enable default capabilities
obj-y					+= commoncap.o
obj-$(CONFIG_MMU)			+= min_addr.o

# Object file lists
obj-$(CONFIG_SECURITY)			+= security.o
obj-$(CONFIG_SECURITYFS)		+= inode.o
obj-$(CONFIG_SECURITY_SELINUX)		+= selinux/
obj-$(CONFIG_SECURITY_SMACK)		+= smack/
obj-$(CONFIG_AUDIT)			+= lsm_audit.o
obj-$(CONFIG_SECURITY_TOMOYO)		+= tomoyo/
obj-$(CONFIG_SECURITY_APPARMOR)		+= apparmor/
obj-$(CONFIG_SECURITY_YAMA)		+= yama/
obj-$(CONFIG_SECURITY_LOADPIN)		+= loadpin/
obj-$(CONFIG_ARCH_QCOM)				+= pfe/
obj-$(CONFIG_CGROUP_DEVICE)		+= device_cgroup.o

# Object integrity file lists
subdir-$(CONFIG_INTEGRITY)		+= integrity
obj-$(CONFIG_INTEGRITY)			+= integrity/
