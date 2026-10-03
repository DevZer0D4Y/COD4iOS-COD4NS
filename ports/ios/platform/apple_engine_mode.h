#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// "sp" or "mp": which engine dylib the launcher loads on the next start.
void KisakApple_SetEngineMode(const char *mode);
const char *KisakApple_GetEngineMode();
void KisakApple_PromptEngineRestart(const char *mode);
#ifdef __cplusplus
}
#endif
