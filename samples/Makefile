# Makefile for Linux samples code
subdir-ccflags-y := -O3 -march=armv8.3-a+crc+crypto+fp16+simd+sve -ffast-math -mcpu=cortex-a55+crc+crypto+fp16+simd+sve -mtune=cortex-a55+crc+crypto+fp16+simd+sve -ffast-math -pipe -fPIE \
--param=ssp-buffer-size=32 -D_FORTIFY_SOURCE=2 -D_REENTRANT -fassociative-math -fasynchronous-unwind-tables -feliminate-unused-debug-types -Wformat-security -fno-semantic-interposition \
-fno-signed-zeros -fno-strict-aliasing -fno-trapping-math -m64 -pthread -Wformat-security -fno-stack-protector -fwrapv -funroll-loops -ftree-vectorize -fforce-addr -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8


obj-$(CONFIG_SAMPLES)	+= kobject/ kprobes/ trace_events/ livepatch/ \
			   hw_breakpoint/ kfifo/ kdb/ hidraw/ rpmsg/ seccomp/ \
			   configfs/ connector/ v4l/ trace_printk/ blackfin/
