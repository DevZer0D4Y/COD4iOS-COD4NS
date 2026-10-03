// Test-only boundaries for game services outside the variable database.
// Unexpected use fails immediately; none of these implementations ship.
#include <script/scr_variable_services.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

[[noreturn]] static void unavailable(const char *name)
{
    std::cerr << "Variable test unexpectedly reached " << name << '\n';
    std::abort();
}
void Scr_Error(const char *error) { throw std::runtime_error(error ? error : "script error"); }
void Scr_TerminalError(const char *error) { Scr_Error(error); }
void Scr_CancelNotifyList(uint32_t) { unavailable("notify scheduler"); }
void VM_CancelNotify(uint32_t, uint32_t) { unavailable("notify scheduler"); }
void VM_TerminateStack(uint32_t, uint32_t, VariableStackBuffer *) { unavailable("VM stack termination"); }
char SetEntityFieldValue(uint32_t, int, int, VariableValue *) { unavailable("entity field write"); }
VariableValue GetEntityFieldValue(uint32_t, int, int) { unavailable("entity field read"); }
XAnim_s *Scr_GetAnims(uint32_t) { unavailable("animation database"); }
char *XAnimGetAnimDebugName(const XAnim_s *, uint32_t) { unavailable("animation database"); }
