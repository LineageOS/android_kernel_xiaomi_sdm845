#
# Makefile for the linux kernel.
#
subdir-ccflags-y := -O3 -fassociative-math -fasynchronous-unwind-tables -feliminate-unused-debug-types -fexceptions -fno-semantic-interposition -fno-signed-zeros -D_FORTIFY_SOURCE=2 \
-fno-strict-aliasing \
-fno-trapping-math -pipe -ffast-math \
-funroll-loops -fforce-addr -fomit-frame-pointer -mtune=cortex-a75.cortex-a55 -mcpu=cortex-a75.cortex-a55+crc+crypto+fp16+simd+sve \
-fno-stack-protector -pthread -Wall -fwrapv --param=ssp-buffer-size=32 -D_REENTRANT \
-floop-parallelize-all -floop-interchange -ftree-loop-distribution -floop-strip-mine -floop-block -floop-optimize -floop-nest-optimize -fprefetch-loop-arrays -ftree-loop-vectorize \
-ftree-vectorize \
-fPIE




ccflags-y := -fno-function-sections -fno-data-sections

obj-y                          := main.o version.o mounts.o
obj-y                          += noinitramfs.o
obj-$(CONFIG_BLK_DEV_INITRD)   += initramfs.o
obj-$(CONFIG_GENERIC_CALIBRATE_DELAY) += calibrate.o

ifneq ($(CONFIG_ARCH_INIT_TASK),y)
obj-y                          += init_task.o
endif

mounts-y			:= do_mounts.o
mounts-$(CONFIG_BLK_DEV_RAM)	+= do_mounts_rd.o
mounts-$(CONFIG_BLK_DEV_INITRD)	+= do_mounts_initrd.o
mounts-$(CONFIG_BLK_DEV_MD)	+= do_mounts_md.o
mounts-$(CONFIG_BLK_DEV_DM)	+= do_mounts_dm.o

# dependencies on generated files need to be listed explicitly
$(obj)/version.o: include/generated/compile.h

# compile.h changes depending on hostname, generation number, etc,
# so we regenerate it always.
# mkcompile_h will make sure to only update the
# actual file if its content has changed.

       chk_compile.h = :
 quiet_chk_compile.h = echo '  CHK     $@'
silent_chk_compile.h = :
include/generated/compile.h: FORCE
	@$($(quiet)chk_compile.h)
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkcompile_h $@ \
	"$(UTS_MACHINE)" "$(CONFIG_SMP)" "$(CONFIG_PREEMPT)" "$(CC) $(KBUILD_CFLAGS)"
