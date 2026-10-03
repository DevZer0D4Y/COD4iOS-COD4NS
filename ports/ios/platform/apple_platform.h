#pragma once

// Release an Apple event only after all users have stopped accessing it.
// The old Windows backend keeps its events until process exit; the iOS
// backend also needs explicit teardown when unloading an engine session.
void Sys_DestroyAppleEvent(void **event);
