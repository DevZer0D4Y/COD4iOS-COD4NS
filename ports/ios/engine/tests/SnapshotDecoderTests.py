#!/usr/bin/env python3
"""Exercise production snapshot readers with malformed wire data and boundary values."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[4]
source = (repo / 'src/qcommon/msg_mp.cpp').read_text()

def function(name):
    import re
    match = re.search(r'^.*\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    if not match:
        raise RuntimeError(name)
    start = match.start()
    brace = source.index('{', match.start())
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'

stubs = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <initializer_list>
#define __cdecl
#define iassert(x) assert(x)
#define vassert(x,...) assert(x)
using byte = uint8_t;
using uint = unsigned;
struct msg_t { uint8_t *data, *splitData; int cursize, splitSize, readcount, bit, overflowed; };
int MSG_GetByte(msg_t *, int);
unsigned GetMinBitCountForNum(unsigned n) { return 32 - __builtin_clz(n); }
void MSG_TraceDump(const char *) {}
const char *va(const char *,...) { return "invalid"; }
void MyAssertHandler(const char *, int, int, const char *, ...) { throw std::runtime_error("assertion"); }
'''
tests = r'''
int main() {
    uint8_t bytes[8]{};
    msg_t m{bytes,nullptr,1,0,0,0,0};
    bytes[0]=61; assert(MSG_ReadLastChangedField(&m,61)==61 && !m.overflowed);
    for (int invalid: {62,63}) {
        bytes[0]=invalid; m={bytes,nullptr,1,0,0,0,0};
        assert(MSG_ReadLastChangedField(&m,61)==0 && m.overflowed);
    }
    m={bytes,nullptr,0,0,0,0,0};
    assert(MSG_ReadLastChangedField(&m,61)==0 && m.overflowed);
    for (int index=0;index<32;++index) {
        // A zero selector bit encodes one toggled bit using a five-bit index.
        bytes[0]=uint8_t(index<<1); m={bytes,nullptr,1,0,0,0,0};
        const int flags=MSG_Read24BitFlag(&m,0x456789);
        if(index<24) assert(flags==(0x456789^(1<<index)) && !m.overflowed);
        else assert(flags==0x456789 && m.overflowed);
    }
    bytes[0]=1; bytes[1]=0x12; bytes[2]=0x34; bytes[3]=0x56;
    m={bytes,nullptr,4,0,0,0,0};
    assert(MSG_Read24BitFlag(&m,0)==0x563412 && !m.overflowed);
    for(int length=1;length<4;++length) {
        m={bytes,nullptr,length,0,0,0,0};
        MSG_Read24BitFlag(&m,0); assert(m.overflowed);
    }
    puts("Snapshot wire bounds: valid fields preserved; invalid counts/flag indices and truncated packets rejected");
}
'''
names = ['MSG_ReadBits','MSG_GetByte','MSG_ReadBit','MSG_ReadByte','MSG_Read24BitFlag','MSG_ReadLastChangedField']
with tempfile.TemporaryDirectory(prefix='kisak-snapshot-test-') as tmp:
    cpp = Path(tmp) / 'snapshot.cpp'
    binary = Path(tmp) / 'snapshot-tests'
    cpp.write_text(stubs + '\n'.join(function(n) for n in names) + tests)
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
