#pragma once
struct msg_t;
bool CL_UsesCoD4x();
void CL_CoD4xStart();
void CL_CoD4xRefreshServers();
void CL_CoD4xRecordServerError(const char *reason);
void CL_CoD4xReset(bool preserveDownload = false);
bool CL_CoD4xPacket(msg_t *msg);
void CL_CoD4xFrame(int localClientNum);
int CL_CoD4xConfigSequence();
void CL_CoD4xSetConfigSequence(int sequence);
void CL_CoD4xClearGameState();
void CL_CoD4xParseClient(msg_t *msg, bool gamestate);
const char *CL_CoD4xClientName(int index);
const char *CL_CoD4xGetConfigString(unsigned index);
void CL_CoD4xSetConfigString(unsigned index, const char *value);
bool CL_CoD4xDownloadCommand(const char *command);
void CL_CoD4xBeginDownload(const char *name);
bool CL_CoD4xVerifyDownload(const char *osPath);

void CL_CoD4xSendPureChecksums(int localClientNum);

void CL_CoD4xStoreServerCommand(int sequence, const char *text);
char *CL_CoD4xServerCommand(int sequence);
