# Makefile for the Linux sound card driver
#
subdir-ccflags-y := -O3 -march=armv8.3-a+crc+crypto+fp16+simd+sve -ffast-math -mcpu=cortex-a55+crc+crypto+fp16+simd+sve -mtune=cortex-a55+crc+crypto+fp16+simd+sve -ffast-math -pipe -fPIE \
--param=ssp-buffer-size=32 -D_FORTIFY_SOURCE=2 -D_REENTRANT -fassociative-math -fasynchronous-unwind-tables -feliminate-unused-debug-types -Wformat-security -fno-semantic-interposition \
-fno-signed-zeros -fno-strict-aliasing -fno-trapping-math -m64 -pthread -Wformat-security -fno-stack-protector -fwrapv -funroll-loops -ftree-vectorize -fforce-addr -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8


obj-$(CONFIG_SOUND) += soundcore.o
obj-$(CONFIG_SOUND_PRIME) += oss/
obj-$(CONFIG_DMASOUND) += oss/
obj-$(CONFIG_SND) += core/ i2c/ drivers/ isa/ pci/ ppc/ arm/ sh/ synth/ usb/ \
	firewire/ sparc/ spi/ parisc/ pcmcia/ mips/ soc/ atmel/ hda/
obj-$(CONFIG_SND_AOA) += aoa/

# This one must be compilable even if sound is configured out
obj-$(CONFIG_AC97_BUS) += ac97_bus.o

ifeq ($(CONFIG_SND),y)
  obj-y += last.o
endif

soundcore-objs  := sound_core.o
