DEFAULT_HOST := $(shell ../default-host.sh)
HOST?=$(DEFAULT_HOST)
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

CC := $(HOST)-gcc
AR := $(HOST)-ar
AS := $(HOST)-as
LD := $(HOST)-ld

SYSROOT ?= $(abspath ../sysroot)
CPPFLAGS := $(CPPFLAGS) -isystem $(SYSROOT)/usr/include

CFLAGS?=-O2 -g
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/boot
INCLUDEDIR?=$(PREFIX)/include

CFLAGS  := $(CFLAGS) --sysroot=$(SYSROOT)
CPPFLAGS:=$(CPPFLAGS) -D__is_kernel -Iinclude
LDFLAGS := $(LDFLAGS) --sysroot=$(SYSROOT)
LIBS:=$(LIBS) -nostdlib -lk -lgcc

ARCHDIR=arch/$(HOSTARCH)

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)

# For tests
#kernel/early.o kernel/ctor_test.o
KERNEL_C_OBJS := kernel/kernel.o $(ARCHDIR)/shell/shell.o
KERNEL_OBJS := $(strip $(KERNEL_ARCH_OBJS) $(KERNEL_C_OBJS))

OBJS := $(ARCHDIR)/crti.o $(ARCHDIR)/crtbegin.o $(KERNEL_OBJS) $(ARCHDIR)/crtend.o $(ARCHDIR)/crtn.o
LINK_LIST := $(LDFLAGS) $(ARCHDIR)/crti.o $(ARCHDIR)/crtbegin.o $(KERNEL_OBJS) $(LIBS) $(ARCHDIR)/crtend.o $(ARCHDIR)/crtn.o

.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .c .S

all: myos.kernel

myos.kernel: $(OBJS) $(ARCHDIR)/linker.ld
	$(CC) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST)
	i686-elf-grub-file --is-x86-multiboot myos.kernel

$(ARCHDIR)/crtbegin.o $(ARCHDIR)/crtend.o:
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

.c.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

.S.o:
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

clean:
	rm -f myos.kernel
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d

install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	(cd include && tar -cf - .) | (cd $(DESTDIR)$(INCLUDEDIR) && tar -xpf -)

install-kernel: myos.kernel
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp myos.kernel $(DESTDIR)$(BOOTDIR)

-include $(OBJS:.o=.d)