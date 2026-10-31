#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
"""Ayrı test imajında kullanıcı new/delete ve süreç belleğini QEMU ile denetler."""
from pathlib import Path
import os
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tools/tests/heap-output"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    with (OUT / "build.log").open("w") as build_log:
        def run(args, cwd=ROOT):
            subprocess.run(args, cwd=cwd, check=True, stdout=build_log, stderr=build_log)

        run(["./build.sh"])
        flags = ["-m32", "-ffreestanding", "-Wall", "-Wextra", "-fno-exceptions",
                 "-fno-rtti", "-fno-use-cxa-atexit", "-fno-pic", "-fno-pie",
                 "-fno-stack-protector", "-fno-builtin", "-fno-unwind-tables",
                 "-fno-asynchronous-unwind-tables", "-mno-sse", "-mno-mmx",
                 "-msoft-float", "-Ikernel/include"]
        source = ROOT / "tools/tests/user_heap_test.cpp"
        apps = OUT / "source/apps"
        apps.mkdir(parents=True, exist_ok=True)
        for role, name in enumerate(("heap_parent", "heap_child", "heap_oom")):
            obj, elf = OUT / f"{name}.o", OUT / f"{name}.elf"
            run(["i686-elf-g++", *flags, f"-DHEAP_TEST_ROLE={role}", "-c", str(source), "-o", str(obj)])
            run(["i686-elf-g++", "-m32", "-no-pie", "-nostdlib",
                 "-Wl,-T,kernel/user/apps/linker.ld", "-Wl,--build-id=none",
                 "-o", str(elf), str(obj), "kernel/user/lib/new.user.o",
                 "kernel/user/lib/memory.user.o"])
            run(["i686-elf-objcopy", "-O", "binary", str(elf), str(apps / f"{name}.bin")])
        shutil.copyfile(ROOT / "kernel/user/apps/new_demo_process.bin", apps / "new_demo.bin")
        run(["python3", "tools/mkpckfs.py", "--source", str(OUT / "source"),
             "--output", str(OUT / "pckfs.img")])

        # Asıl masaüstü ve ISO dosyaları yerine ayrı bir test çekirdeği bağlanır.
        embed = OUT / "embed.asm"
        embed.write_text('BITS 32\nsection .rodata\nglobal pckos_user_desktop_start\n'
                         'global pckos_user_desktop_end\npckos_user_desktop_start:\n'
                         f' incbin "{apps / "heap_parent.bin"}"\npckos_user_desktop_end:\n')
        run(["nasm", "-felf32", str(embed), "-o", str(OUT / "embed.o")])
        fragment = OUT / "test.mk"
        fragment.write_text('.PHONY: heap-test-kernel\nheap-test-kernel:\n'
            '\t$(CC) -T $(ARCHDIR)/linker.ld -o ../tools/tests/heap-output/pckos.bin '
            '$(CFLAGS) $(subst arch/i386/user_desktop.o,../tools/tests/heap-output/embed.o,$(LINK_LIST))\n')
        run(["make", "-C", "kernel", "-f", "Makefile", "-f", str(fragment),
             "heap-test-kernel", "CC=i686-elf-g++"])
        boot = OUT / "iso/boot"
        (boot / "grub").mkdir(parents=True, exist_ok=True)
        shutil.copyfile(OUT / "pckos.bin", boot / "pckos.bin")
        (boot / "grub/grub.cfg").write_text('set timeout=0\nset default=0\nmenuentry "heap test" {\n'
            'set gfxpayload=1024x768x32\nmultiboot /boot/pckos.bin\nboot\n}\n')
        run(["grub-mkrescue", "-o", str(OUT / "test.iso"), str(OUT / "iso")])

    serial = OUT / "serial.log"
    serial.write_text("")
    with (OUT / "qemu.log").open("w") as qemu_log:
        process = subprocess.Popen([
            "qemu-system-i386", "-m", "128M", "-accel", "tcg", "-cpu", "qemu32",
            "-cdrom", str(OUT / "test.iso"), "-boot", "d",
            "-drive", f"file={OUT / 'pckfs.img'},format=raw,if=ide,index=0,snapshot=on",
            "-display", "none", "-monitor", "none", "-serial", f"file:{serial}", "-no-reboot",
        ], stdout=qemu_log, stderr=qemu_log, env={**os.environ, "TMPDIR": str(OUT)})
        try:
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline:
                output = serial.read_text(errors="replace")
                if "HEAP_FAIL" in output or "KERNEL PANIC" in output:
                    raise RuntimeError(output)
                if "HEAP_ALL_PASS" in output:
                    assert output.count("HEAP_CHILD_PASS") == 9, output
                    assert "new: kullanici bellegi ayrilamadi" in output, output
                    assert "[new-demo] ptr = 0x40100000" in output, output
                    assert "[new-demo] delete tamamlandi" in output, output
                    print("PASS: memory/string helpers, new/delete, hizalama, sayfalar, thread'ler, süreç yalıtımı ve OOM")
                    return
                if process.poll() is not None:
                    raise RuntimeError("QEMU erken kapandı: " + output)
                time.sleep(0.1)
            raise TimeoutError(serial.read_text(errors="replace"))
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


if __name__ == "__main__":
    main()
