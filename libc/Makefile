DEFAULT_HOST!=../default-host.sh
HOST?=$(DEFAULT_HOST)
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

CFLAGS?=-O2 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
INCLUDEDIR?=$(PREFIX)/include
LIBDIR?=$(EXEC_PREFIX)/lib

CFLAGS:=$(CFLAGS) -ffreestanding -Wall -Wextra
CPPFLAGS:=$(CPPFLAGS) -D__is_libc -Iinclude -Imath
LIBK_CFLAGS:=$(CFLAGS)
LIBK_CPPFLAGS:=$(CPPFLAGS) -D__is_libk

ARCHDIR=arch/$(HOSTARCH)

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(ARCH_CPPFLAGS)
LIBK_CFLAGS:=$(LIBK_CFLAGS) $(KERNEL_ARCH_CFLAGS)
LIBK_CPPFLAGS:=$(LIBK_CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)

FREEOBJS=\
$(ARCH_FREEOBJS) \
stdio/printf.o \
stdio/putchar.o \
stdio/puts.o \
stdio/sprintf.o \
string/more.o \
stdlib/abort.o \
string/memcmp.o \
string/memcpy.o \
string/memmove.o \
string/memset.o \
string/strlen.o \
string/strcmp.o \
string/strchr.o \
string/strcat.o \
string/strstr.o \
string/strcpy.o \
string/strncmp.o \
string/strncpy.o \

HOSTEDOBJS=\
$(ARCH_HOSTEDOBJS) \
unistd/write.o \
unistd/read.o \
unistd/tcsetraw.o \
unistd/fb.o \
unistd/file.o \
unistd/getpid.o \
unistd/fork.o \
unistd/exec.o \
unistd/wait.o \
unistd/_exit.o \
stdlib/exit.o \
stdlib/malloc.o \
unistd/brk.o \
unistd/signal.o \
unistd/time.o \
unistd/pthread.o \
unistd/timeutil.o \
stdlib/misc.o \
math/__cos.o \
math/__math_oflow.o \
math/__math_uflow.o \
math/__math_invalid.o \
math/__math_divzero.o \
math/__math_xflow.o \
math/__expo2.o \
math/__rem_pio2_large.o \
math/__rem_pio2.o \
math/__sin.o \
math/__tan.o \
math/acos.o \
math/asin.o \
math/atan.o \
math/atan2.o \
math/cbrt.o \
math/copysign.o \
math/cos.o \
math/cosh.o \
math/exp_data.o \
math/exp.o \
math/exp2.o \
math/expm1.o \
math/fabs.o \
math/fmax.o \
math/fmin.o \
math/fmod.o \
math/frexp.o \
math/hypot.o \
math/ldexp.o \
math/log_data.o \
math/log.o \
math/log10.o \
math/log1p.o \
math/log2_data.o \
math/log2.o \
math/modf.o \
math/pow_data.o \
math/pow.o \
math/scalbn.o \
math/sin.o \
math/sinh.o \
math/sqrt.o \
math/round_x87.o \
math/tan.o \
math/tanh.o \
stdlib/strtod.o \
stdio/file.o \
stdio/sscanf.o \
stdlib/util.o \

OBJS=\
$(FREEOBJS) \
$(HOSTEDOBJS) \

LIBK_OBJS=$(FREEOBJS:.o=.libk.o)

BINARIES=libc.a libk.a

.PHONY: all clean install install-headers install-libs
.SUFFIXES: .o .libk.o .c .S

all: $(BINARIES)

libc.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

libk.a: $(LIBK_OBJS)
	$(AR) rcs $@ $(LIBK_OBJS)

.c.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

.S.o:
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

.c.libk.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)

.S.libk.o:
	$(CC) -MD -c $< -o $@ $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)

clean:
	rm -f $(BINARIES) *.a
	rm -f $(OBJS) $(LIBK_OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) $(LIBK_OBJS:.o=.d) *.d */*.d */*/*.d

install: install-headers install-libs

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	(cd include && tar -cf - .) | (cd $(DESTDIR)$(INCLUDEDIR) && tar -xpf -)

install-libs: $(BINARIES)
	mkdir -p $(DESTDIR)$(LIBDIR)
	cp $(BINARIES) $(DESTDIR)$(LIBDIR)

-include $(OBJS:.o=.d)
-include $(LIBK_OBJS:.o=.d)