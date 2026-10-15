#pragma once
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <csignal>
#include <cstdint>
#include <cerrno>
#include <initializer_list>
#include <dlfcn.h>
#if defined(__APPLE__) && defined(__aarch64__)
#include <sys/ucontext.h>
#endif

namespace kisak::savefile::diagnostics {
inline int eventFD=-1;
inline volatile sig_atomic_t crashFD=-1;
inline void Hex(char *&out,uintptr_t value) {
    *out++='0';*out++='x';
    for(int shift=int(sizeof(value)*8)-4;shift>=0;shift-=4)*out++="0123456789abcdef"[(value>>shift)&15];
}
inline void Literal(char *&out,const char *s){while(*s)*out++=*s++;}
inline void FatalSignal(int signal,siginfo_t *info,void *context) {
    // Only async-signal-safe operations. Preserve the fatal signal for the OS
    // crash reporter; do not try to save engine state from a signal handler.
    char line[192];char *out=line;Literal(out,"signal=");Hex(out,signal);
    Literal(out," fault=");Hex(out,reinterpret_cast<uintptr_t>(info?info->si_addr:nullptr));
#if defined(__APPLE__) && defined(__aarch64__)
    auto *state=static_cast<ucontext_t *>(context);
    if(state && state->uc_mcontext){
        Literal(out," pc=");Hex(out,state->uc_mcontext->__ss.__pc);
        Literal(out," lr=");Hex(out,state->uc_mcontext->__ss.__lr);
    }
#else
    (void)context;
#endif
    *out++='\n';if(crashFD>=0)write(crashFD,line,size_t(out-line));
    struct sigaction reset{};reset.sa_handler=SIG_DFL;sigemptyset(&reset.sa_mask);
    sigaction(signal,&reset,nullptr);kill(getpid(),signal);
}
inline void Begin(const char *root,const char *mode) {
    if(eventFD>=0 || !root)return;
    const std::string directory=std::string(root)+"/diagnostics";
    if(mkdir(directory.c_str(),0700) && errno!=EEXIST)return;
    eventFD=open((directory+"/campaign-events.txt").c_str(),O_WRONLY|O_CREAT|O_APPEND,0600);
    crashFD=open((directory+"/native-crash.txt").c_str(),O_WRONLY|O_CREAT|O_APPEND,0600);
    Dl_info image{};dladdr(reinterpret_cast<const void *>(&Begin),&image);
    char line[160];const int n=snprintf(line,sizeof(line),"\nsession build=9 mode=%s time=%lld imageBase=%p\n",mode,(long long)time(nullptr),image.dli_fbase);
    if(eventFD>=0)write(eventFD,line,n);
    if(crashFD>=0){
        write(crashFD,line,n);
        for(int signal:{SIGABRT,SIGSEGV,SIGBUS,SIGILL,SIGFPE}){
            struct sigaction previous{};if(sigaction(signal,nullptr,&previous))continue;
            // Respect a handler installed by the runtime or a debugger.
            if(previous.sa_handler!=SIG_DFL)continue;
            struct sigaction action{};action.sa_sigaction=FatalSignal;
            action.sa_flags=SA_SIGINFO;sigemptyset(&action.sa_mask);sigaction(signal,&action,nullptr);
        }
    }
}
inline void Stage(const char *stage,const char *map="",const char *save="") {
    if(eventFD<0)return;
    char line[320];const int n=snprintf(line,sizeof(line),"time=%lld stage=%.80s map=%.64s save=%.64s\n",
        (long long)time(nullptr),stage,map?map:"",save?save:"");
    if(n>0)write(eventFD,line,size_t(n)<sizeof(line)?size_t(n):sizeof(line)-1);
}
}
