#pragma once

#include "kisak_zone_bridge.h"

// Hooks for the zone being loaded on this thread (null outside KisakZone_Load).
// Vendored OAT files that hand data to the engine read them from here.
const KisakZoneHooks *KisakZone_CurrentHooks();
