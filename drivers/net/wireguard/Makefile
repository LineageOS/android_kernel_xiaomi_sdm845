# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.

ccflags-y := -O3
ccflags-y += -D'pr_fmt(fmt)=KBUILD_MODNAME ": " fmt'
ccflags-$(CONFIG_WIREGUARD_DEBUG) += -DDEBUG
wireguard-y := main.o
wireguard-y += noise.o
wireguard-y += device.o
wireguard-y += peer.o
wireguard-y += timers.o
wireguard-y += queueing.o
wireguard-y += send.o
wireguard-y += receive.o
wireguard-y += socket.o
wireguard-y += peerlookup.o
wireguard-y += allowedips.o
wireguard-y += ratelimiter.o
wireguard-y += cookie.o
wireguard-y += netlink.o

ccflags-y += -Wframe-larger-than=2048
ccflags-$(if $(WIREGUARD_VERSION),y,) += -D'WIREGUARD_VERSION="$(WIREGUARD_VERSION)"'

include $(src)/crypto/Makefile.include
include $(src)/compat/Makefile.include

obj-$(if $(KBUILD_EXTMOD),m,$(CONFIG_WIREGUARD)) := wireguard.o
