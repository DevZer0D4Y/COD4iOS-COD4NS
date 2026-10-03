#!/usr/bin/env python3
"""Exercise the production immediate-save writer with an injected disk failure."""
from pathlib import Path
import subprocess, tempfile
r = Path(__file__).resolve().parents[4]
s = (r / 'src/script/scr_readwrite.cpp').read_text()
production = s[s.index('void __cdecl SaveMemory_SaveWriteImmediate('):s.index('void __cdecl Scr_SaveSourceImmediate(')]
stubs = r'''
#include <cassert>
#include <cstdint>
#define __cdecl
struct SaveImmediate { void *f; };
unsigned writes = 0;
bool shortWrite = false;
uint32_t FS_Write(const char *, uint32_t len, int handle) {
    assert(handle == 7); ++writes; return shortWrite ? len - 1 : len;
}
'''
tests = r'''
int main() {
    const char data[] = "source";
    SaveImmediate save{(void *)(intptr_t)7};
    SaveMemory_SaveWriteImmediate(data, sizeof(data), &save);
    assert(save.f && writes == 1);
    SaveMemory_SaveWriteImmediate(nullptr, 0, &save);
    assert(save.f && writes == 1);
    shortWrite = true;
    SaveMemory_SaveWriteImmediate(data, sizeof(data), &save);
    assert(!save.f && writes == 2);
    shortWrite = false;
    SaveMemory_SaveWriteImmediate(data, sizeof(data), &save);
    assert(!save.f && writes == 2); // Failure cannot be cleared by a later write.
    save.f = (void *)(intptr_t)7;
    SaveMemory_SaveWriteImmediate(nullptr, 3, &save);
    assert(!save.f && writes == 2);
    SaveMemory_SaveWriteImmediate(data, sizeof(data), nullptr);
}
'''
with tempfile.TemporaryDirectory() as d:
    p = Path(d)
    (p / 'test.cpp').write_text(stubs + production + tests)
    subprocess.run(['clang++', '-std=c++17', str(p / 'test.cpp'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('Short metadata writes latch failure and stop subsequent writes')
