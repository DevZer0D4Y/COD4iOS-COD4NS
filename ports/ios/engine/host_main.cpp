// macOS arm64 host executable for the single-player engine. Same ABI as the
// iPhone build, so engine bring-up can be run and debugged directly.
//
// Usage: KISAK_INSTALL_PATH="/path/to/Call of Duty 4" kisakcod_sp_host [+set ...]

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

int KisakApple_RunEngine(const char *commandLine);

int main(int argc, char **argv)
{
    const char *installPath = getenv("KISAK_INSTALL_PATH");
    if (!installPath || !*installPath)
    {
        fprintf(stderr, "Set KISAK_INSTALL_PATH to the Call of Duty 4 folder (the one containing main/ and zone/).\n");
        return 2;
    }
    // localization.txt and the engine's relative paths resolve from the game folder.
    if (chdir(installPath) != 0)
    {
        perror("chdir KISAK_INSTALL_PATH");
        return 2;
    }

    std::string commandLine;
    for (int i = 1; i < argc; ++i)
    {
        if (!commandLine.empty())
            commandLine += ' ';
        commandLine += argv[i];
    }
    return KisakApple_RunEngine(commandLine.c_str());
}
