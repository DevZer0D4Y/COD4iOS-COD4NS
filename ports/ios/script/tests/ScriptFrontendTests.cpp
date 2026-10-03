#include <universal/q_shared.h>
#include <universal/com_memory.h>
#include <script/scr_parsetree.h>
#include <script/scr_compile_state.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void check(bool ok, const char *message)
{
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}

static void hunk()
{
    const auto pageSize = Z_VirtualPageSize();
    auto *h = Hunk_UserCreate(int(pageSize * 4), "test script hunk", true, true, 7);
    const auto initial = h->pos;
    auto *first = static_cast<unsigned char *>(Hunk_UserAlloc(h, 1, 1));
    *first = 0x45;
    for (unsigned alignment = 2; alignment <= 4096; alignment *= 2) {
        auto *p = static_cast<unsigned char *>(Hunk_UserAlloc(h, 17, alignment));
        check(reinterpret_cast<std::uintptr_t>(p) % alignment == 0, "hunk alignment");
        std::memset(p, 0x99, 17);
    }
    auto *tail = static_cast<unsigned char *>(Hunk_UserAlloc(h, pageSize, 16));
    std::memset(tail, 0x67, pageSize);
    check(*first == 0x45, "page commit clobbered earlier allocation");
    Hunk_UserReset(h);
    check(h->pos == initial && !h->next && h->current == h, "hunk reset state");
    auto *reset = static_cast<unsigned char *>(Hunk_UserAlloc(h, pageSize * 2, 1));
    check(std::all_of(reset, reset + pageSize * 2, [](auto c) { return c == 0; }), "decommit/recommit must produce zeroed pages");
    Hunk_UserSetPos(h, reset + 4);
    check(Hunk_UserAlloc(h, 8, 1) == reset + 4, "fixed hunk rewind");
    Hunk_UserDestroy(h);

    h = Hunk_UserCreate(4096, "growing parser hunk", false, true, 7);
    std::vector<unsigned char *> blocks;
    for (int i = 0; i < 40; ++i) {
        auto *p = static_cast<unsigned char *>(Hunk_UserAlloc(h, 2048, 8));
        std::memset(p, i, 2048);
        blocks.push_back(p);
    }
    check(h->next && h->current != h, "hunk failed to grow");
    for (int i = 0; i < 40; ++i)
        check(std::all_of(blocks[i], blocks[i] + 2048, [i](auto value) { return value == i; }), "hunk chain lost data");
    bool rejected = false;
    try { Hunk_UserAlloc(h, 4096, 4096); } catch (const std::runtime_error &) { rejected = true; }
    check(rejected, "oversized request must fail instead of growing forever");
    Hunk_UserReset(h);
    check(!h->next && h->current == h, "hunk chain reset");
    check(std::string(Hunk_CopyString(h, "maps/killhouse")) == "maps/killhouse", "hunk string copy");
    Hunk_UserDestroy(h);
}

static void releaseTokens()
{
    // The compiler normally consumes token references. This isolated parser
    // harness owns all tokens and releases them after checking the AST.
    for (unsigned i = 1; i < MEMORY_NODE_COUNT; ++i)
        while (scrStringDebugGlob->refCount[i]) SL_RemoveRefToString(i);
}

static unsigned parse(const std::string &source, bool expectFailure = false)
{
    SL_Init();
    Scr_InitAllocNode();
    scrCompilePub.in_ptr = "+";
    scrCompilePub.parseBuf = source.c_str();
    scrCompilePub.far_function_count = 0;
    unsigned functions = 0;
    bool failed = false;
    try {
        sval_u result;
        ScriptParse(&result, 0);
        // Program = (include-list, thread-list); list wrappers contain head/tail.
        check(result.node != nullptr, "missing program AST");
        for (auto *item = result.node[1].node[0].node; item; item = item[1].node) {
            const auto *thread = item[0].node;
            if (thread[0].type == ENUM_thread) {
                check(SL_ConvertToString(thread[1].stringValue) != nullptr, "lost function identifier");
                ++functions;
            }
        }
    } catch (const std::runtime_error &) {
        if (!expectFailure) throw;
        failed = true;
    }
    check(failed == expectFailure, "invalid script accepted");
    Scr_ShutdownAllocNode();
    releaseTokens();
    SL_Shutdown();
    check(scrMemTreeGlob.totalAlloc == 0, "parser leaked token references");
    return functions;
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            std::ifstream input(argv[i], std::ios::binary);
            if (!input) return 2;
            const std::string source((std::istreambuf_iterator<char>(input)), {});
            try { std::cout << argv[i] << ": " << parse(source) << " functions parsed\n"; }
            catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
        }
        return 0;
    }
    hunk();
    check(parse("main() { level.ready = 1; } helper(a,b) { return a+b; }") == 2, "function list");
    check(parse(R"gsc(
        #include maps\_utility;
        main() {
            level endon("mission_ended");
            level thread worker();
            for (i = 0; i < 8; i++) { if (i == 4) continue; level.counter = i; }
            level waittill("checkpoint", checkpoint);
            wait 0.05;
            level notify("mission_ended");
        }
        worker() { while (true) { waittillframeend; } }
    )gsc") == 2, "mission script control-flow syntax");
    std::string large = "main() {\n";
    for (int i = 0; i < 6000; ++i) large += "level.checkpoint = 1;\n";
    large += "}\n";
    check(parse(large) == 1, "scanner refill and AST arena growth");
    std::string deep = "main() { x = ";
    deep.append(350, '('); deep += "1"; deep.append(350, ')'); deep += "; }";
    check(parse(deep) == 1, "parser stack growth");
    parse("main() { if (", true);
    check(parse("main() { return; }") == 1, "parse after error");
    std::cout << "Native hunk, AST pointers, GSC grammar, buffer refill and parse recovery passed\n";
}
