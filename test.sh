i686-elf-gcc -c kernel/kernel/ctor_test.c -o kernel/kernel/ctor_test.o \
  -std=gnu11 -O2 -Wall -Wextra -ffreestanding -fno-pic -fno-stack-protector -m32 \
  -I kernel/include

i686-elf-gcc -T kernel/arch/i386/linker.ld -o myos.kernel \
  -ffreestanding -O2 -nostdlib -m32 \
  kernel/arch/i386/crti.o "$CRTBEGIN" \
  kernel/arch/i386/boot.o \
  kernel/kernel/kernel.o \
  kernel/arch/i386/tty.o \
  kernel/kernel/ctor_test.o \
  -L sysroot/usr/lib -lk \
  "$CRTEND" kernel/arch/i386/crtn.o \
  -lgcc

cp myos.kernel sysroot/boot/myos.kernel 
