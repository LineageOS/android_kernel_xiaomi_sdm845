#
# Makefile for the linux ipc.
#
subdir-ccflags-y := -O3 -march=armv8.3-a+crc+crypto+fp16+simd+sve -ffast-math -mcpu=cortex-a55+crc+crypto+fp16+simd+sve -mtune=cortex-a55+crc+crypto+fp16+simd+sve -ffast-math -pipe -fPIE \
--param=ssp-buffer-size=32 -D_FORTIFY_SOURCE=2 -D_REENTRANT -fassociative-math -fasynchronous-unwind-tables -feliminate-unused-debug-types -Wformat-security -fno-semantic-interposition \
-fno-signed-zeros -fno-strict-aliasing -fno-trapping-math -m64 -pthread -Wformat-security -fno-stack-protector -fwrapv -funroll-loops -ftree-vectorize -fforce-addr -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8


obj-$(CONFIG_SYSVIPC_COMPAT) += compat.o
obj-$(CONFIG_SYSVIPC) += util.o msgutil.o msg.o sem.o shm.o syscall.o
obj-$(CONFIG_SYSVIPC_SYSCTL) += ipc_sysctl.o
obj_mq-$(CONFIG_COMPAT) += compat_mq.o
obj-$(CONFIG_POSIX_MQUEUE) += mqueue.o msgutil.o $(obj_mq-y)
obj-$(CONFIG_IPC_NS) += namespace.o
obj-$(CONFIG_POSIX_MQUEUE_SYSCTL) += mq_sysctl.o
