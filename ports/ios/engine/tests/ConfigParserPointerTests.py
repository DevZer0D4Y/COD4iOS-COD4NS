#!/usr/bin/env python3
"""Test the production config parser with stub asset registries, without a full engine."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[4]
source = (repo / 'src/universal/q_shared.cpp').read_text()
start = source.index('bool __cdecl ParseConfigStringToStructCustomSize(')
end = source.index('\ndouble __cdecl GetLeanFraction', start)
parser = source[start:end]
header = (repo / 'src/universal/q_shared.h').read_text()
start = header.index('enum csParseFieldType_t')
end = header.index('\n};', start) + 3
enum = header[start:end].replace('__int32', 'int32_t')
stubs = r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <cstdio>
#define __cdecl
#define KISAK_MP
constexpr bool alwaysfails = false;
constexpr int ERR_DROP = 1, IMAGE_TRACK_MISC = 0;
struct FxEffectDef { int marker; } fx;
struct XModel { int marker; } model;
struct Material { int marker; } material;
struct snd_alias_list_t { int marker; } sound;
struct Dvar { struct { int integer; } current; } dedicated{}, *com_dedicated=&dedicated;
struct cspField_t { const char *szName; int iOffset; int iFieldType; };
const char *Info_ValueForKey(char *, char *) { return "asset"; }
void I_strncpyz(char *out, const char *in, int size) { std::snprintf(out,size,"%s",in); }
const FxEffectDef *FX_Register(const char *) { return &fx; }
XModel *R_RegisterModel(const char *) { return &model; }
Material *Material_RegisterHandle(const char *, int) { return &material; }
snd_alias_list_t *Com_FindSoundAlias(const char *) { return &sound; }
const char *va(const char *, ...) { return "error"; }
void MyAssertHandler(const char *,int,int,const char *,...) { std::abort(); }
void Com_Error(int,const char *,...) { std::abort(); }
'''
tests = r'''
struct Assets { const FxEffectDef *fx; XModel *model; Material *material; snd_alias_list_t *sound; uint64_t guard; };
int main() {
    const cspField_t fields[] = {
        {"fx",offsetof(Assets,fx),CSPFT_FX}, {"model",offsetof(Assets,model),CSPFT_XMODEL},
        {"material",offsetof(Assets,material),CSPFT_MATERIAL}, {"sound",offsetof(Assets,sound),CSPFT_SOUND}
    };
    Assets assets;
    // Dirty high bytes make a partial-width store fail even on a low-address test process.
    std::memset(&assets,0xa5,sizeof(assets)); assets.guard=0x123456789abcdef0ULL;
    char input[]="fixture";
    assert(ParseConfigStringToStructCustomSize(reinterpret_cast<uint8_t*>(&assets),fields,4,input,CSPFT_NUM_BASE_FIELD_TYPES,nullptr,nullptr));
    assert(assets.fx==&fx && assets.model==&model && assets.material==&material && assets.sound==&sound);
    assert(assets.guard==0x123456789abcdef0ULL);
    puts("Production config parser preserves native asset pointers and adjacent memory");
}
'''
with tempfile.TemporaryDirectory(prefix='kisak-config-parser-') as tmp:
    cpp = Path(tmp) / 'parser.cpp'
    exe = Path(tmp) / 'parser-test'
    cpp.write_text(stubs + enum + '\n' + parser + tests)
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-g',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
