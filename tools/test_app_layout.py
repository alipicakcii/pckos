#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
"""Host regression checks for client layout; no booted kernel is required."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
compiler = shutil.which('g++')
if not compiler:
    raise SystemExit('g++ is required for host layout checks')

syscall = r'''
#pragma once
#include <stdint.h>
#include <cstring>
namespace user {
constexpr uint32_t directory_type = 2;
struct DirectoryEntry { uint32_t inode, type, size; char name[48]; };
inline int32_t root_inode() { return 0; }
inline int32_t get_ticks() { return 12345; }
inline int32_t get_heap_usage() { return 54321; }
inline int32_t open(const char*) { return 3; }
inline int32_t close(int32_t) { return 0; }
inline int32_t read(int32_t, void*, uint32_t) { return 0; }
inline int32_t get_kernel_log(char*, uint32_t) { return 0; }
inline int32_t read_directory(uint32_t, uint32_t index, DirectoryEntry* entry) {
    if (index >= 32) return 0;
    *entry = {};
    entry->inode = index + 1;
    entry->size = UINT32_MAX;
    std::memset(entry->name, 'a', sizeof(entry->name) - 1);
    return 1;
}
}
'''

gui = r'''
#pragma once
#include <stdint.h>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
namespace user::gui {
constexpr uint32_t FONT_WIDTH=8, FONT_HEIGHT=16;
constexpr uint32_t COLOR_BLACK=0, COLOR_FACE=1, COLOR_WHITE=2,
    COLOR_SELECTION=3, COLOR_TEXT=4, COLOR_TITLE=5;
struct Rect { uint32_t left, top, right, bottom; };
inline std::vector<Rect> clips;
inline std::vector<std::string> painted;
inline uint32_t text_width(const char* value) { return std::strlen(value)*FONT_WIDTH; }
inline void push_clip(uint32_t x,uint32_t y,uint32_t w,uint32_t h) {
    Rect next{x,y,x+w,y+h};
    if (!clips.empty()) {
        const auto& p=clips.back();
        next={std::max(p.left,next.left), std::max(p.top,next.top),
            std::min(p.right,next.right),std::min(p.bottom,next.bottom)};
    }
    assert(next.right>=next.left && next.bottom>=next.top);
    clips.push_back(next);
}
inline void pop_clip() { assert(!clips.empty()); clips.pop_back(); }
inline void check(uint32_t x,uint32_t y,uint32_t w,uint32_t h) {
    assert(!clips.empty());
    const auto& p=clips.back();
    assert(x>=p.left && y>=p.top && x+w<=p.right && y+h<=p.bottom);
}
inline void text(uint32_t x,uint32_t y,const char* value,uint32_t) {
    if (*value) check(x,y,text_width(value),FONT_HEIGHT);
    painted.emplace_back(value);
}
inline void fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t) { check(x,y,w,h); }
inline void frame(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t,bool=false) { check(x,y,w,h); }
inline void button(uint32_t x,uint32_t y,uint32_t w,uint32_t h,const char*,bool=false) { check(x,y,w,h); }
}
'''

test = r'''
#include <cassert>
#include <cstring>
#include <string>
#include <user/gui.hh>
#define private public
#include <user/apps/terminal.hh>
#include <user/apps/file_manager.hh>
#undef private
int main() {
    using namespace user::apps;
    Terminal terminal;
    const std::string long_line(300, 'w');
    terminal.print_line(long_line.c_str());
    for (int i=0; i<127; ++i) terminal.key('x');
    for (uint32_t width : {100u,180u,372u,546u,1024u})
        for (uint32_t height : {70u,122u,300u,700u}) {
            terminal.draw(17,29,width,height);
            assert(user::gui::clips.empty());
        }
    terminal.reset();
    for (char c : std::string("ticks")) terminal.key(c);
    terminal.key('\n');
    assert(std::string(terminal.lines_[terminal.line_count_-1])=="PIT tik sayisi: 12345");
    terminal.reset();
    terminal.print_line(long_line.c_str());
    std::string stored;
    for (uint32_t i=2;i<terminal.line_count_;++i) stored += terminal.lines_[i];
    assert(stored==long_line);
    for (int i=0;i<100;++i) terminal.print_line(long_line.c_str());
    terminal.draw(17,29,546,300);
    assert(user::gui::clips.empty());

    FileManager files;
    std::memset(files.path_+1,'a',sizeof(files.path_)-2);
    files.path_[sizeof(files.path_)-1]='\0';
    std::memset(files.preview_,'x',sizeof(files.preview_));
    files.preview_size_=sizeof(files.preview_);
    for (uint32_t width : {180u,300u,419u,420u,636u,1000u})
        for (uint32_t height : {122u,200u,390u,700u}) {
            files.draw(17,29,width,height);
            assert(user::gui::clips.empty());
        }
    files.reset();
    files.key(0xE3);
    assert(files.selected_valid_ && files.selected_==0);
    files.key(0xE3);
    assert(files.selected_==1);
    files.draw(17,29,636,390);
    assert(user::gui::clips.empty());
}
'''

with tempfile.TemporaryDirectory(prefix='.app-layout-test-', dir=root) as working:
    work = Path(working)
    (work/'include/user').mkdir(parents=True)
    (work/'include/user/syscall.hh').write_text(syscall)
    (work/'include/user/gui.hh').write_text(gui)
    (work/'layout.cpp').write_text(test)
    subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
        '-I'+str(work/'include'), '-I'+str(root/'kernel/include'),
        str(root/'kernel/user/apps/terminal.cpp'),
        str(root/'kernel/user/apps/file_manager.cpp'), str(work/'layout.cpp'),
        '-o', str(work/'layout')], check=True)
    subprocess.run([str(work/'layout')], check=True,
        env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
print('Terminal wrapping, cursor bounds, file panes, long paths and history: passed')
