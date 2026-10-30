#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
"""Headless QEMU GUI regression checks. Run after ./iso.sh and ./share.sh.
Keeps captures/logs in this project; uses a temporary snapshot of the PCKFS image.
"""
from pathlib import Path
import json
import os
import select
import shutil
import subprocess
import sys
import time
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'tools/tests/gui-output'
DESKTOP_COLOR = (32, 43, 35)

class Guest:
    def __init__(self, memory='256M'):
        OUT.mkdir(exist_ok=True)
        self.log = (OUT / 'qemu.log').open('w')
        disk_image = OUT / 'pckfs-smoke.img'
        shutil.copyfile(ROOT / 'pckfs.img', disk_image)
        self.process = subprocess.Popen([
            'qemu-system-i386', '-m', memory, '-accel', 'tcg', '-cpu', 'qemu32',
            '-cdrom', str(ROOT / 'pckos.iso'),
            '-drive', f'file={disk_image},format=raw,if=ide,index=0,snapshot=on',
            '-boot', 'd', '-display', 'none', '-monitor', 'none', '-qmp', 'stdio',
            '-rtc', 'base=localtime',
            '-serial', f'file:{OUT / "serial.log"}', '-no-reboot',
        ], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log, bufsize=0, env={**os.environ, 'TMPDIR': str(OUT)})
        self.pending = b''
        self.receive()
        self.command('qmp_capabilities')
        self.x, self.y = 512, 384

    def receive(self):
        deadline = time.monotonic() + 8
        while b'\n' not in self.pending:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.process.stdout], [], [], remaining)[0]:
                raise TimeoutError('QEMU QMP response timeout')
            chunk = os.read(self.process.stdout.fileno(), 65536)
            if not chunk:
                raise RuntimeError((OUT / 'qemu.log').read_text())
            self.pending += chunk
        line, self.pending = self.pending.split(b'\n', 1)
        return json.loads(line)

    def command(self, name, arguments=None):
        message = {'execute': name}
        if arguments is not None: message['arguments'] = arguments
        self.process.stdin.write((json.dumps(message) + '\n').encode())
        while True:
            result = self.receive()
            if 'error' in result: raise RuntimeError(result)
            if 'return' in result: return result['return']

    def hmp(self, text):
        result = self.command('human-monitor-command', {'command-line': text})
        if result: raise RuntimeError(result)

    def shot(self, name):
        path = OUT / f'{name}.ppm'
        self.command('screendump', {'filename': str(path)})
        with Image.open(path) as source:
            image = source.convert('RGB')
        image.save(OUT / f'{name}.png')
        path.unlink()
        return image

    def move(self, x, y):
        while (self.x, self.y) != (x, y):
            dx = max(-80, min(80, x - self.x))
            dy = max(-80, min(80, y - self.y))
            self.hmp(f'mouse_move {dx} {dy}')
            self.x += dx
            self.y += dy
            time.sleep(0.025)
        time.sleep(0.08)

    def click(self, x, y):
        self.move(x, y)
        self.hmp('mouse_button 1')
        time.sleep(0.07)
        self.hmp('mouse_button 0')
        time.sleep(0.18)

    def key(self, key):
        self.hmp(f'sendkey {key} 1')
        time.sleep(0.035)

    def type(self, text):
        keys = {' ': 'spc', '\n': 'ret', '/': 'slash', '.': 'dot', '-': 'minus'}
        for c in text: self.key(keys.get(c, c))
        time.sleep(0.15)

    def close(self):
        if self.process.poll() is None:
            try: self.command('quit')
            except (BrokenPipeError, RuntimeError): pass
            self.process.wait(timeout=5)
        self.log.close()


def main():
    guest = Guest()
    try:
        for _ in range(40):
            time.sleep(0.2)
            boot = guest.shot('desktop')
            if boot.size == (1024, 768) and boot.getpixel((500, 100)) == DESKTOP_COLOR:
                break
        else: raise AssertionError('Desktop failed to boot')
        # HMP göreli girdisi, PS/2 aygıtı etkin QEMU konsoluna geçerken ilk olayı düşürebilir.
        # Önce işaretçiyi bilinen bir noktaya getir.
        time.sleep(0.5)
        for _ in range(14):
            guest.hmp('mouse_move -100 -100')
            time.sleep(0.06)
        guest.x = guest.y = 0
        guest.click(42, 190)
        guest.type('help\n')
        terminal = guest.shot('terminal-help')
        assert terminal.getpixel((300, 300)) != DESKTOP_COLOR, 'Terminal did not open'
        guest.type('threads\n')
        thread_status = guest.shot('terminal-threads')
        assert ImageChops.difference(thread_status, terminal).getbbox() is not None, 'Thread status command did not update the terminal'
        guest.type('echo ' + 'abcdefghij' * 11 + '\n')
        guest.type('dmesg\n')
        long_output = guest.shot('terminal-long-output')
        # 1024x768 çözünürlükte Terminal sınırları: x=287, y=214, genişlik=560, yükseklik=340.
        # İşaretçi ve saati hesaba katmamak için masaüstünün sağ ve sol şeritlerini denetle.
        assert long_output.crop((848, 215, 1024, 550)).tobytes() == boot.crop((848, 215, 1024, 550)).tobytes(), 'Terminal text escaped right edge'
        assert long_output.crop((85, 215, 287, 550)).tobytes() == boot.crop((85, 215, 287, 550)).tobytes(), 'Terminal text escaped left edge'
        if '--keyboard-only' in sys.argv:
            log = (OUT / 'serial.log').read_text()
            assert '[Ring3] Hata' not in log and 'PANIC' not in log, log
            print('QEMU: keyboard input reached terminal and commands ran without a kernel fault')
            print(f'Captures: {OUT}')
            return
        guest.click(42, 117)
        guest.shot('files')
        guest.key('down')
        guest.key('ret')
        guest.shot('files-directory')
        # İşaretçi hareket ederken masaüstü ve tüm pencereler bozulmadan kalır.
        guest.move(820, 650)
        stable_a = guest.shot('stable-a')
        guest.move(950, 650)
        guest.move(820, 650)
        stable_b = guest.shot('stable-b')
        assert stable_a.crop((0, 0, 1024, 720)).tobytes() == stable_b.crop((0, 0, 1024, 720)).tobytes(), 'Cursor trails or unintended repaint'
        # Dosya penceresini sürüklerken ara kareleri örnekle: birleştirme sırasında sabit Terminal
        # penceresinin yerini masaüstü arka planı almamalıdır.
        guest.move(420, 180)
        guest.hmp('mouse_button 1')
        time.sleep(0.06)
        for index in range(8):
            guest.move(430 + index * 12, 186 + index * 6)
            capture = guest.shot(f'drag-{index}')
            assert capture.getpixel((810, 500)) != DESKTOP_COLOR, 'Window cleared during drag'
        guest.hmp('mouse_button 0')
        time.sleep(0.15)
        guest.shot('dragged')
        log = (OUT / 'serial.log').read_text()
        assert '[Ring3] Hata' not in log and 'PANIC' not in log and 'masaustu sonlandi' not in log, log
        print('QEMU: boot, terminal wrapping/log, file navigation, cursor stability and drag passed')
        print(f'Captures: {OUT}')
    finally:
        guest.close()

if __name__ == '__main__': main()
