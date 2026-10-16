#include "../../save/SaveDiagnostics.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sys/wait.h>
#include <sys/resource.h>
int main(){
    char folder[]="/tmp/cod4-diagnostics-XXXXXX";assert(mkdtemp(folder));
    pid_t child=fork();assert(child>=0);
    if(!child){
        struct rlimit limit{0,0};setrlimit(RLIMIT_CORE,&limit);
        kisak::savefile::diagnostics::Begin(folder,"sp");
        kisak::savefile::diagnostics::Stage("checkpoint serialization begin","blackout","blackout-inside");
        raise(SIGABRT);_exit(3);
    }
    int status=0;assert(waitpid(child,&status,0)==child && WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
    std::ifstream crash(std::string(folder)+"/diagnostics/native-crash.txt");
    std::string text{std::istreambuf_iterator<char>(crash),{}};assert(text.find("signal=0x")!=std::string::npos);
    assert(text.find("session build="+std::to_string(KISAK_IOS_RELEASE_BUILD))!=std::string::npos);
#if defined(__APPLE__) && defined(__aarch64__)
    assert(text.find(" sp=0x")!=std::string::npos && text.find(" fp=0x")!=std::string::npos);
    for(int reg=0;reg<29;++reg)assert(text.find(" x"+std::to_string(reg)+"=0x")!=std::string::npos);
#endif
    kisak::savefile::diagnostics::Begin(folder,"sp");
    std::ifstream events(std::string(folder)+"/diagnostics/campaign-events.txt");
    text={std::istreambuf_iterator<char>(events),{}};assert(text.find("map=blackout")!=std::string::npos);
    std::filesystem::remove_all(folder);
}
