// HTTP download backend for the multiplayer client (see apple_download.mm). Plain C so the
// engine's decompiled sources can call it without pulling in Objective-C headers.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Starts one transfer to `localName`; returns 0 if the URL could not be used.
int KisakDownload_Begin(const char *localName, const char *remoteName, int isMotd);
// dlStatus_t: 0 continue, 1 done, 2 failed.
int KisakDownload_Poll(void);
void KisakDownload_Cancel(void);
int KisakDownload_InProgress(void);
int KisakDownload_IsMotd(void);
void KisakDownload_Progress(long long *received, long long *expected);

#ifdef __cplusplus
}
#endif
