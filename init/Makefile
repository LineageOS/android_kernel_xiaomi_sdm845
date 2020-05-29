#
# Makefile for the linux kernel.
#
subdir-ccflags-y := -O3 -march=armv8.3-a+crc+crypto+fp16+simd+sve -ffast-math -mcpu=cortex-a55+crc+crypto+fp16+simd+sve -mtune=cortex-a55+crc+crypto+fp16+simd+sve -ffast-math -pipe -fPIE \
--param=ssp-buffer-size=32 -D_FORTIFY_SOURCE=2 -D_REENTRANT -fassociative-math -fasynchronous-unwind-tables -feliminate-unused-debug-types -Wformat-security -fno-semantic-interposition \
-fno-signed-zeros -fno-strict-aliasing -fno-trapping-math -m64 -pthread -Wformat-security -fno-stack-protector -fwrapv -funroll-loops -ftree-vectorize -fforce-addr -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8


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
