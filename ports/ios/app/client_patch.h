#pragma once
#import <Foundation/Foundation.h>
// Fetches only the public CoD4x compatibility patch, never retail game data.
void KisakPrepareCoD4xPatch(NSString *documents, void (^completion)(NSError *));
BOOL KisakCoD4xPatchValid(NSData *data);
