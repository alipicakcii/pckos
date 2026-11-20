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
USER_APP_CFLAGS=-MMD -MP -m32 -ffreestanding -Wall -Wextra -fno-exceptions -fno-rtti \
	-fno-use-cxa-atexit -fno-threadsafe-statics -fPIC -fno-stack-protector -fno-builtin \
	-fno-unwind-tables -fno-asynchronous-unwind-tables -mno-sse -mno-mmx \
	-msoft-float -Iinclude
USER_EXEC_LDFLAGS=-m32 -no-pie -nostdlib -Wl,-T,user/apps/linker.ld \
	-Wl,--build-id=none -Wl,--no-dynamic-linker -Wl,--hash-style=sysv \
	-Wl,-z,max-page-size=0x1000 -Wl,--no-as-needed -Luser/lib
USER_LIBRARY_OBJECTS=\
user/syscall/call.user.o user/syscall/process.user.o \
user/syscall/thread.user.o user/syscall/system.user.o \
user/syscall/graphics.user.o user/syscall/memory.user.o \
user/syscall/io.user.o user/syscall/filesystem.user.o \
user/syscall/input.user.o user/syscall/audio.user.o \
user/syscall/serial_dev.user.o user/memory/new.user.o \
user/lib/memory.user.o user/io/io.user.o user/gui/graphics.user.o \
user/gui/window.user.o user/gui/widgets.user.o user/lib/rnd.user.o \
user/gui/image.user.o
USER_DESKTOP_OBJECTS=\
user/apps/file_manager.user.o user/apps/terminal.user.o \
user/apps/terminal_commands.user.o user/apps/task_manager.user.o \
user/apps/desktop.user.o
USER_PROCESS_OBJECTS=\
user/apps/terminal_process.user.o \
user/apps/file_manager_process.user.o \
user/apps/task_manager_process.user.o \
user/apps/calculator_process.user.o \
user/apps/notepad_process.user.o \
user/apps/demo_process.user.o \
user/apps/hello_word.user.o \
user/apps/system_info_process.user.o \
user/apps/clock_process.user.o \
user/apps/unit_converter_process.user.o \
user/apps/memory_hunter_process.user.o
USER_APP_OBJECTS=$(USER_LIBRARY_OBJECTS) $(USER_DESKTOP_OBJECTS) $(USER_PROCESS_OBJECTS)
USER_PROCESS_IMAGES=\
user/apps/terminal_process.elf \
user/apps/file_manager_process.elf \
user/apps/task_manager_process.elf \
user/apps/calculator_process.elf \
user/apps/notepad_process.elf \
user/apps/demo_process.elf \
user/apps/hello_word.elf \
user/apps/system_info_process.elf \
user/apps/clock_process.elf \
user/apps/unit_converter_process.elf \
user/apps/memory_hunter_process.elf

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)

KERNEL_OBJS=\
$(KERNEL_ARCH_OBJS) \
arch/i386/user_mode.o \
arch/i386/user_desktop.o \
library/cppabi.o \
library/rnd.o \
library/kernel_log.o \
library/pck_rtti.o \
task/user_mode.o \
task/thread.o \
loader/elf.o \
loader/dynamic_linker.o \
syscalls/syscall.o \
syscalls/common.o \
syscalls/io.o \
syscalls/filesystem.o \
syscalls/thread.o \
syscalls/cpu_rnd.o \
syscalls/process.o \
syscalls/system.o \
syscalls/memory.o \
syscalls/input.o \
syscalls/graphics.o \
syscalls/audio.o \
syscalls/serial_dev.o \
interrupts/descriptortables.o \
interrupts/isr.o \
device/input/keymap.o \
device/input/ps2_keyboard.o \
device/input/ps2_mouse.o \
device/ata_pio.o \
device/serial_device.o \
device/rtc.o \
device/video/frame_buffer.o \
device/pc_speaker.o \
fs/pckfs.o \
boot/screen.o \
core/panic.o \
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

# Çapraz derleyici -shared seçeneğini ld'ye aktarmadığı için doğrudan iletiyoruz.
device/video/frame_buffer.o: device/video/frame_buffer.cpp
	$(CC) -MD -O2 -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

syscalls/%.o: syscalls/%.cpp
	$(CC) -MD -O2 -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

.asm.o:
	nasm -felf32 $< -o $@

user/lib/libpckos.so: $(USER_LIBRARY_OBJECTS)
# Çapraz derleyici -shared seçeneğini ld'ye aktarmadığı için doğrudan iletiyoruz.
	$(CC) -m32 -shared -nostdlib -Wl,--build-id=none \
		-Wl,-shared -Wl,--hash-style=sysv -Wl,-soname,libpckos.so \
		-Wl,-z,max-page-size=0x1000 -o $@ $(USER_LIBRARY_OBJECTS)

user/%.user.o: user/%.cpp
	$(CC) $(USER_APP_CFLAGS) -c $< -o $@

user/lib/memory.user.o: user/lib/memory.cpp include/user/lib/memory.hh
	$(CC) $(USER_APP_CFLAGS) -O2 -c $< -o $@

user/gui/image.user.o: user/gui/image.cpp include/user/gui/image.hh include/user/memory/new.hh include/user/lib/memory.hh user/lib/stb_image.h
	$(CC) $(USER_APP_CFLAGS) -O2 -c $< -o $@

user/apps/desktop.elf: $(USER_DESKTOP_OBJECTS) user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ $(USER_DESKTOP_OBJECTS) -lpckos

user/apps/terminal_process.elf: user/apps/terminal.user.o user/apps/terminal_commands.user.o user/apps/terminal_process.user.o user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ user/apps/terminal.user.o user/apps/terminal_commands.user.o user/apps/terminal_process.user.o -lpckos

user/apps/file_manager_process.elf: user/apps/file_manager.user.o user/apps/file_manager_process.user.o user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ user/apps/file_manager.user.o user/apps/file_manager_process.user.o -lpckos

user/apps/task_manager_process.elf: user/apps/task_manager.user.o user/apps/task_manager_process.user.o user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ user/apps/task_manager.user.o user/apps/task_manager_process.user.o -lpckos

user/apps/calculator_process.elf: user/apps/calculator_process.user.o user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ user/apps/calculator_process.user.o -lpckos

user/apps/notepad_process.elf: user/apps/notepad_process.user.o user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ user/apps/notepad_process.user.o -lpckos

user/apps/demo_process.elf: user/apps/demo_process.user.o user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ user/apps/demo_process.user.o -lpckos

user/apps/hello_word.elf: user/apps/hello_word.user.o user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ user/apps/hello_word.user.o -lpckos

user/apps/system_info_process.elf: user/apps/system_info_process.user.o user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ user/apps/system_info_process.user.o -lpckos

user/apps/clock_process.elf: user/apps/clock_process.user.o user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ user/apps/clock_process.user.o -lpckos

user/apps/unit_converter_process.elf: user/apps/unit_converter_process.user.o user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ user/apps/unit_converter_process.user.o -lpckos

user/apps/memory_hunter_process.elf: user/apps/memory_hunter_process.user.o user/lib/libpckos.so user/apps/linker.ld
	$(CC) $(USER_EXEC_LDFLAGS) -o $@ user/apps/memory_hunter_process.user.o -lpckos

arch/i386/user_desktop.o: arch/i386/user_desktop.asm user/apps/desktop.elf
	nasm -felf32 $< -o $@

-include $(OBJS:.o=.d) $(USER_APP_OBJECTS:.o=.d)

clean:
	rm -f user/apps/demo_process.elf user/apps/hello_word.elf user/apps/system_info_process.elf \
		user/apps/clock_process.elf user/apps/unit_converter_process.elf user/apps/memory_hunter_process.elf
	rm -f pckos.bin
	rm -f $(USER_APP_OBJECTS) $(USER_PROCESS_OBJECTS) user/apps/desktop.elf \
		user/apps/terminal_process.elf user/apps/file_manager_process.elf \
		user/apps/task_manager_process.elf user/apps/calculator_process.elf \
		user/apps/notepad_process.elf $(USER_PROCESS_IMAGES)
	rm -f user/apps/*.bin user/lib/libpckos.so
	find . -type f \( -name '*.o' -o -name '*.d' \) -delete

install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.

install-kernel: pckos.bin
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp pckos.bin $(DESTDIR)$(BOOTDIR)

