CC = i686-elf-gcc
AS = nasm
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra
OBJS = boot.o kernel.o

all: myos.bin

boot.o: boot.s
	$(AS) -felf32 boot.s -o boot.o

kernel.o: kernel.c
	$(CC) -c kernel.c -o kernel.o $(CFLAGS)

myos.bin: $(OBJS) linker.ld
	$(CC) -T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib $(OBJS) -lgcc

run: myos.bin
	qemu-system-i386 -kernel myos.bin -display cocoa

clean:
	rm -f *.o myos.bin