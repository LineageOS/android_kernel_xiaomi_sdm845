#
# Makefile for the linux ipc.
#
subdir-ccflags-y := -O3 -fassociative-math -fasynchronous-unwind-tables -feliminate-unused-debug-types -fexceptions -fno-semantic-interposition -fno-signed-zeros -D_FORTIFY_SOURCE=2 \
-fno-strict-aliasing \
-fno-trapping-math -pipe -ffast-math \
-funroll-loops -fforce-addr -fomit-frame-pointer -mtune=cortex-a75.cortex-a55 -mcpu=cortex-a75.cortex-a55+crc+crypto+fp16+simd+sve \
-fno-stack-protector -pthread -Wall -fwrapv --param=ssp-buffer-size=32 -D_REENTRANT \
-floop-parallelize-all -floop-interchange -ftree-loop-distribution -floop-strip-mine -floop-block -floop-optimize -floop-nest-optimize -fprefetch-loop-arrays -ftree-loop-vectorize \
-ftree-vectorize \
-fPIE




obj-$(CONFIG_SYSVIPC_COMPAT) += compat.o
obj-$(CONFIG_SYSVIPC) += util.o msgutil.o msg.o sem.o shm.o syscall.o
obj-$(CONFIG_SYSVIPC_SYSCTL) += ipc_sysctl.o
obj_mq-$(CONFIG_COMPAT) += compat_mq.o
obj-$(CONFIG_POSIX_MQUEUE) += mqueue.o msgutil.o $(obj_mq-y)
obj-$(CONFIG_IPC_NS) += namespace.o
obj-$(CONFIG_POSIX_MQUEUE_SYSCTL) += mq_sysctl.o
