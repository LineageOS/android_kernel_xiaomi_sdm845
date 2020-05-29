###
# scripts contains sources for various helper programs used throughout
# the kernel for the build process.
# ---------------------------------------------------------------------------
# kallsyms:      Find all symbols in vmlinux
# pnmttologo:    Convert pnm files to logo files
# conmakehash:   Create chartable
# conmakehash:	 Create arrays for initializing the kernel console tables
# docproc:       Used in Documentation/DocBook
# check-lc_ctype: Used in Documentation/DocBook
subdir-ccflags-y := -O3 -fassociative-math -fasynchronous-unwind-tables -feliminate-unused-debug-types -fexceptions -fno-semantic-interposition -fno-signed-zeros -D_FORTIFY_SOURCE=2 \
-fno-strict-aliasing \
-fno-trapping-math -pipe -ffast-math \
-funroll-loops -fforce-addr -fomit-frame-pointer -mtune=cortex-a75.cortex-a55 -mcpu=cortex-a75.cortex-a55+crc+crypto+fp16+simd+sve \
-fno-stack-protector -pthread -Wall -fwrapv --param=ssp-buffer-size=32 -D_REENTRANT \
-floop-parallelize-all -floop-interchange -ftree-loop-distribution -floop-strip-mine -floop-block -floop-optimize -floop-nest-optimize -fprefetch-loop-arrays -ftree-loop-vectorize \
-ftree-vectorize \
-fPIE




HOST_EXTRACFLAGS += -I$(srctree)/tools/include

CRYPTO_LIBS = $(shell pkg-config --libs libcrypto 2> /dev/null || echo -lcrypto)
CRYPTO_CFLAGS = $(shell pkg-config --cflags libcrypto 2> /dev/null)

hostprogs-$(CONFIG_KALLSYMS)     += kallsyms
hostprogs-$(CONFIG_LOGO)         += pnmtologo
hostprogs-$(CONFIG_VT)           += conmakehash
hostprogs-$(BUILD_C_RECORDMCOUNT) += recordmcount
hostprogs-$(CONFIG_BUILDTIME_EXTABLE_SORT) += sortextable
hostprogs-$(CONFIG_ASN1)	 += asn1_compiler
hostprogs-$(CONFIG_MODULE_SIG)	 += sign-file
hostprogs-$(CONFIG_SYSTEM_TRUSTED_KEYRING) += extract-cert
hostprogs-$(CONFIG_SYSTEM_EXTRA_CERTIFICATE) += insert-sys-cert

HOSTCFLAGS_sortextable.o = -I$(srctree)/tools/include
HOSTCFLAGS_asn1_compiler.o = -I$(srctree)/include
HOSTCFLAGS_sign-file.o = $(CRYPTO_CFLAGS)
HOSTLOADLIBES_sign-file = $(CRYPTO_LIBS)
HOSTCFLAGS_extract-cert.o = $(CRYPTO_CFLAGS)
HOSTLOADLIBES_extract-cert = $(CRYPTO_LIBS)

always		:= $(hostprogs-y) $(hostprogs-m)

# The following hostprogs-y programs are only build on demand
hostprogs-y += unifdef docproc check-lc_ctype

# These targets are used internally to avoid "is up to date" messages
PHONY += build_unifdef build_docproc build_check-lc_ctype
build_unifdef: $(obj)/unifdef
	@:
build_docproc: $(obj)/docproc
	@:
build_check-lc_ctype: $(obj)/check-lc_ctype
	@:

subdir-$(CONFIG_MODVERSIONS) += genksyms
subdir-y                     += mod
subdir-$(CONFIG_SECURITY_SELINUX) += selinux
subdir-$(CONFIG_DTC)         += dtc
subdir-$(CONFIG_GDB_SCRIPTS) += gdb

# Let clean descend into subdirs
subdir-	+= basic kconfig package gcc-plugins
