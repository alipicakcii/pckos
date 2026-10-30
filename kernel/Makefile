# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
DEFAULT_HOST=i686-elf
HOST?=DEFAULT_HOST
HOSTARCH=i386

CFLAGS?=
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/boot
INCLUDEDIR?=$(PREFIX)/include

# -fno-elide-constructors bu bayrak RVO ve NRVO'yu devre dışı bırakır
#  c++ 20 icin -std=c++20
CFLAGS:=$(CFLAGS) -ffreestanding -Wall -Wextra -fno-exceptions -fno-threadsafe-statics  -fconcepts -fno-elide-constructors 
#__is_VBOX virtualbox için derleyiciye bildirim
#__is_DEBUG çekirdeğin geliştirme aşamasında kullanılan bazı fonsksiyonları
CPPFLAGS:=$(CPPFLAGS)   -D__is_kernel -D__is_DEBUG   -Iinclude 
LDFLAGS:=$(LDFLAGS)
LIBS:=$(LIBS) -nostdlib  -lgcc

ARCHDIR=arch/$(HOSTARCH)
OBJCOPY?=i686-elf-objcopy
USER_APP_CFLAGS=-m32 -ffreestanding -Wall -Wextra -fno-exceptions -fno-rtti \
	-fno-use-cxa-atexit -fno-pic -fno-pie -fno-stack-protector -fno-builtin \
	-fno-unwind-tables -fno-asynchronous-unwind-tables -mno-sse -mno-mmx \
	-msoft-float -Iinclude
USER_APP_OBJECTS=\
user/gui/graphics.user.o \
user/apps/file_manager.user.o \
user/apps/terminal.user.o \
user/apps/task_manager.user.o \
user/apps/desktop.user.o
USER_PROCESS_OBJECTS=\
user/apps/terminal_process.user.o \
user/apps/file_manager_process.user.o \
user/apps/task_manager_process.user.o
USER_PROCESS_OBJECTS+=user/apps/calculator_process.user.o user/apps/notepad_process.user.o
USER_PROCESS_IMAGES=\
user/apps/terminal_process.bin \
user/apps/file_manager_process.bin \
user/apps/task_manager_process.bin
USER_PROCESS_IMAGES+=user/apps/calculator_process.bin user/apps/notepad_process.bin

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)

KERNEL_OBJS=\
$(KERNEL_ARCH_OBJS) \
arch/i386/user_mode.o \
arch/i386/user_demo.o \
arch/i386/user_desktop.o \
library/cppabi.o \
library/kernel_log.o \
library/pck_rtti.o \
user_mode.o \
thread.o \
syscall.o \
interrupts/descriptortables.o \
interrupts/isr.o \
device/input/keymap.o \
device/input/ps2_keyboard.o \
device/input/ps2_mouse.o \
device/ata_pio.o \
device/rtc.o \
device/serial_device.o \
device/video/frame_buffer.o \
fs/pckfs.o \
panic.o \
mem/vmm.o \
mem/addr.o \
mem/alloc.o \
kernel.o \
library/kprint.o \
library/convert.o \
library/mem.o \
mem/new.o \

OBJS=\
$(ARCHDIR)/crti.o \
$(ARCHDIR)/crtbegin.o \
$(KERNEL_OBJS) \
$(ARCHDIR)/crtend.o \
$(ARCHDIR)/crtn.o \

LINK_LIST=\
$(LDFLAGS) \
$(ARCHDIR)/crti.o \
$(ARCHDIR)/crtbegin.o \
$(KERNEL_OBJS) \
$(LIBS) \
$(ARCHDIR)/crtend.o \
$(ARCHDIR)/crtn.o \

.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .cpp .asm

all: pckos.bin

pckos.bin: $(OBJS) $(ARCHDIR)/linker.ld $(USER_PROCESS_IMAGES)
	$(CC) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST)
	grub-file --is-x86-multiboot pckos.bin

$(ARCHDIR)/crtbegin.o $(ARCHDIR)/crtend.o:
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

.cpp.o:
	$(CC) -MD -c $< -o $@  $(CFLAGS) $(CPPFLAGS)

# Pixel loops and validated graphics syscalls run frequently during window movement.
device/video/frame_buffer.o: device/video/frame_buffer.cpp
	$(CC) -MD -O2 -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

syscall.o: syscall.cpp
	$(CC) -MD -O2 -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

.asm.o:
	nasm -felf32 $< -o $@

user/gui/graphics.user.o: user/gui/graphics.cpp include/user/gui.hh include/user/syscall.hh
	$(CC) $(USER_APP_CFLAGS) -c $< -o $@

user/apps/file_manager.user.o: user/apps/file_manager.cpp include/user/apps/file_manager.hh include/user/apps/app_thread.hh include/user/gui.hh include/user/syscall.hh
	$(CC) $(USER_APP_CFLAGS) -c $< -o $@

user/apps/terminal.user.o: user/apps/terminal.cpp include/user/apps/terminal.hh include/user/apps/app_thread.hh include/user/gui.hh include/user/syscall.hh
	$(CC) $(USER_APP_CFLAGS) -c $< -o $@

user/apps/task_manager.user.o: user/apps/task_manager.cpp include/user/apps/task_manager.hh include/user/apps/app_thread.hh include/user/gui.hh include/user/syscall.hh
	$(CC) $(USER_APP_CFLAGS) -c $< -o $@

user/apps/desktop.user.o: user/apps/desktop.cpp include/user/apps/file_manager.hh include/user/apps/terminal.hh include/user/apps/task_manager.hh include/user/gui.hh include/user/syscall.hh
	$(CC) $(USER_APP_CFLAGS) -c $< -o $@

user/apps/desktop.elf: $(USER_APP_OBJECTS) user/apps/linker.ld
	$(CC) -m32 -no-pie -nostdlib -Wl,-T,user/apps/linker.ld -Wl,--build-id=none -o $@ $(USER_APP_OBJECTS)

user/apps/desktop.bin: user/apps/desktop.elf
	$(OBJCOPY) -O binary $< $@

user/apps/terminal_process.user.o: user/apps/terminal_process.cpp include/user/apps/terminal.hh include/user/gui.hh include/user/syscall.hh include/user/window_drag.hh
	$(CC) $(USER_APP_CFLAGS) -c $< -o $@

user/apps/file_manager_process.user.o: user/apps/file_manager_process.cpp include/user/apps/file_manager.hh include/user/gui.hh include/user/syscall.hh include/user/window_drag.hh
	$(CC) $(USER_APP_CFLAGS) -c $< -o $@

user/apps/task_manager_process.user.o: user/apps/task_manager_process.cpp include/user/apps/task_manager.hh include/user/gui.hh include/user/syscall.hh include/user/window_drag.hh
	$(CC) $(USER_APP_CFLAGS) -c $< -o $@

user/apps/calculator_process.user.o: user/apps/calculator_process.cpp include/user/gui.hh include/user/syscall.hh include/user/window_drag.hh
	$(CC) $(USER_APP_CFLAGS) -c $< -o $@

user/apps/notepad_process.user.o: user/apps/notepad_process.cpp include/user/gui.hh include/user/syscall.hh include/user/window_drag.hh
	$(CC) $(USER_APP_CFLAGS) -c $< -o $@

user/apps/terminal_process.elf: user/gui/graphics.user.o user/apps/terminal.user.o user/apps/terminal_process.user.o user/apps/linker.ld
	$(CC) -m32 -no-pie -nostdlib -Wl,-T,user/apps/linker.ld -Wl,--build-id=none -o $@ user/gui/graphics.user.o user/apps/terminal.user.o user/apps/terminal_process.user.o

user/apps/file_manager_process.elf: user/gui/graphics.user.o user/apps/file_manager.user.o user/apps/file_manager_process.user.o user/apps/linker.ld
	$(CC) -m32 -no-pie -nostdlib -Wl,-T,user/apps/linker.ld -Wl,--build-id=none -o $@ user/gui/graphics.user.o user/apps/file_manager.user.o user/apps/file_manager_process.user.o

user/apps/task_manager_process.elf: user/gui/graphics.user.o user/apps/task_manager.user.o user/apps/task_manager_process.user.o user/apps/linker.ld
	$(CC) -m32 -no-pie -nostdlib -Wl,-T,user/apps/linker.ld -Wl,--build-id=none -o $@ user/gui/graphics.user.o user/apps/task_manager.user.o user/apps/task_manager_process.user.o

user/apps/calculator_process.elf: user/gui/graphics.user.o user/apps/calculator_process.user.o user/apps/linker.ld
	$(CC) -m32 -no-pie -nostdlib -Wl,-T,user/apps/linker.ld -Wl,--build-id=none -o $@ user/gui/graphics.user.o user/apps/calculator_process.user.o

user/apps/notepad_process.elf: user/gui/graphics.user.o user/apps/notepad_process.user.o user/apps/linker.ld
	$(CC) -m32 -no-pie -nostdlib -Wl,-T,user/apps/linker.ld -Wl,--build-id=none -o $@ user/gui/graphics.user.o user/apps/notepad_process.user.o

user/apps/terminal_process.bin: user/apps/terminal_process.elf
	$(OBJCOPY) -O binary $< $@

user/apps/file_manager_process.bin: user/apps/file_manager_process.elf
	$(OBJCOPY) -O binary $< $@

user/apps/task_manager_process.bin: user/apps/task_manager_process.elf
	$(OBJCOPY) -O binary $< $@

user/apps/calculator_process.bin: user/apps/calculator_process.elf
	$(OBJCOPY) -O binary $< $@

user/apps/notepad_process.bin: user/apps/notepad_process.elf
	$(OBJCOPY) -O binary $< $@

arch/i386/user_desktop.o: arch/i386/user_desktop.asm user/apps/desktop.bin
	nasm -felf32 $< -o $@

clean:
	rm -f pckos.bin
	rm -f $(USER_APP_OBJECTS) $(USER_PROCESS_OBJECTS) user/apps/desktop.elf user/apps/desktop.bin \
		user/apps/terminal_process.elf user/apps/file_manager_process.elf \
		user/apps/task_manager_process.elf user/apps/calculator_process.elf \
		user/apps/notepad_process.elf $(USER_PROCESS_IMAGES)
	find . -type f \( -name '*.o' -o -name '*.d' \) -delete

install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.

install-kernel: pckos.bin
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp pckos.bin $(DESTDIR)$(BOOTDIR)

-include $(OBJS:.o=.d)
