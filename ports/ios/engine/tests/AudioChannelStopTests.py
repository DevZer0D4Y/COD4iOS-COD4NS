#!/usr/bin/env python3
"""Exercise production OpenAL buffer cleanup with playing/paused sources."""
from pathlib import Path
import tempfile,subprocess
repo=Path(__file__).resolve().parents[4]
s=(repo/'src/sound/snd_driver_openal.cpp').read_text()
f=s[s.index('void SND_ReleaseChannelBuffer(int index)'):s.index('// Defined near SND_SetRoomtype')]
stubs=r'''
#include <cassert>
#include <cstdio>
using ALuint=unsigned;constexpr int AL_BUFFER=0;
struct {ALuint source[3]{10,11,12},channelBuffer[3]{20,21,0};}alGlob;
int playing[3]{1,2,1},stopCount=0,detachCount=0,deleteCount=0;bool attached[3]{true,true,false};
void alSourceStop(ALuint source){assert(source>=10&&source<=12);playing[source-10]=0;++stopCount;}
void alSourcei(ALuint source,int param,int value){assert(param==AL_BUFFER && value==0);assert(playing[source-10]==0);attached[source-10]=false;++detachCount;}
void alDeleteBuffers(int count,ALuint *buffer){assert(count==1 && *buffer>=20 && *buffer<=21);assert(!attached[*buffer-20]);++deleteCount;}
'''
tests=r'''
int main(){
 SND_ReleaseChannelBuffer(0);assert(!playing[0]&&!attached[0]&&!alGlob.channelBuffer[0]);
 SND_ReleaseChannelBuffer(1);assert(!playing[1]&&!attached[1]&&!alGlob.channelBuffer[1]);
 assert(stopCount==2&&detachCount==2&&deleteCount==2);
 SND_ReleaseChannelBuffer(0);SND_ReleaseChannelBuffer(2);
 assert(!playing[2]&&stopCount==4&&detachCount==2&&deleteCount==2);
 puts("Production OpenAL cleanup: playing/paused/looping sources stop before detach/delete; repeated cleanup is safe");}
'''
with tempfile.TemporaryDirectory(prefix='cod4ios-audio-stop-') as d:
 p=Path(d);(p/'test.cpp').write_text(stubs+f+tests)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
