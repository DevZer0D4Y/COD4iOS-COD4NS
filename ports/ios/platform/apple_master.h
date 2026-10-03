#pragma once
#include <cstdint>
struct KisakMasterAddress { uint8_t ip[4]; uint8_t port[2]; };
void KisakMaster_Begin();
// 0 pending, -1 failed, positive number of IPv4 server addresses.
int KisakMaster_Poll(KisakMasterAddress *addresses, int capacity);
