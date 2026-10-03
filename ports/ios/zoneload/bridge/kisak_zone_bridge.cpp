#include "kisak_zone_hooks_internal.h"

#include "Game/IGame.h"
#include "Game/IW3/ContentLoaderIW3.h"
#include "Game/IW3/IW3.h"
#include "Loading/ILoadingStream.h"
#include "Zone/Stream/ZoneInputStream.h"
#include "Zone/XBlock.h"
#include "Zone/Zone.h"

#include <cstdio>

#if UINTPTR_MAX > 0xFFFFFFFFu
// Must match the engine cbrush_t stride (static_assert in src/xanim/xanim.h).
static_assert(sizeof(IW3::cbrush_t) == 96, "IW3::cbrush_t stride differs from the engine cbrush_t");
#endif
#include <exception>
#include <memory>
#include <optional>
#include <vector>

namespace
{
    thread_local const KisakZoneHooks* t_hooks = nullptr;

    class ScopedHooks
    {
    public:
        explicit ScopedHooks(const KisakZoneHooks* hooks)
            : m_previous(t_hooks)
        {
            t_hooks = hooks;
        }

        ~ScopedHooks()
        {
            t_hooks = m_previous;
        }

    private:
        const KisakZoneHooks* m_previous;
    };

    class HookStream final : public ILoadingStream
    {
    public:
        explicit HookStream(const KisakZoneHooks& hooks)
            : m_hooks(hooks)
        {
        }

        size_t Load(void* buffer, const size_t length) override
        {
            const size_t read = m_hooks.read(m_hooks.context, buffer, length);
            m_position += static_cast<int64_t>(read);
            return read;
        }

        int64_t Pos() override
        {
            return m_position;
        }

    private:
        const KisakZoneHooks& m_hooks;
        int64_t m_position = 0;
    };

    struct BlockSetup
    {
        const char* name;
        XBlockType type;
    };

    // Same block kinds as OAT's ZoneLoaderFactoryIW3, in XFILE_BLOCK_* order.
    const BlockSetup kBlocks[9] = {
        {"XFILE_BLOCK_TEMP",             XBlockType::BLOCK_TYPE_TEMP   },
        {"XFILE_BLOCK_RUNTIME",          XBlockType::BLOCK_TYPE_RUNTIME},
        {"XFILE_BLOCK_LARGE_RUNTIME",    XBlockType::BLOCK_TYPE_RUNTIME},
        {"XFILE_BLOCK_PHYSICAL_RUNTIME", XBlockType::BLOCK_TYPE_RUNTIME},
        {"XFILE_BLOCK_VIRTUAL",          XBlockType::BLOCK_TYPE_NORMAL },
        {"XFILE_BLOCK_LARGE",            XBlockType::BLOCK_TYPE_NORMAL },
        {"XFILE_BLOCK_PHYSICAL",         XBlockType::BLOCK_TYPE_NORMAL },
        {"XFILE_BLOCK_VERTEX",           XBlockType::BLOCK_TYPE_NORMAL },
        {"XFILE_BLOCK_INDEX",            XBlockType::BLOCK_TYPE_NORMAL },
    };

    // IW3 PC zones: 32-bit pointers whose top 4 bits select the block.
    constexpr unsigned kZonePointerBits = 32u;
    constexpr unsigned kOffsetBlockBits = 4u;
} // namespace

const KisakZoneHooks* KisakZone_CurrentHooks()
{
    return t_hooks;
}

// Defined in ZoneCommon/Marking/BaseAssetMarker.cpp.
void KisakZone_ResetConvertedScriptStrings();

void* KisakZone_Load(const char* zoneName, const KisakZoneBlocks& blocks, const KisakZoneHooks& hooks, char* errorOut, const size_t errorSize)
{
    try
    {
        auto zone = std::make_unique<Zone>(zoneName ? zoneName : "", 0, GameId::IW3, GamePlatform::PC);

        std::vector<std::unique_ptr<XBlock>> ownedBlocks;
        std::vector<XBlock*> blockList;
        for (unsigned index = 0; index < 9; ++index)
        {
            auto block = std::make_unique<XBlock>(kBlocks[index].name, index, kBlocks[index].type);
            block->UseExternalBuffer(blocks.data[index], blocks.size[index]);
            blockList.push_back(block.get());
            ownedBlocks.push_back(std::move(block));
        }

        HookStream stream(hooks);
        ScopedHooks scope(&hooks);
        const auto input =
            ZoneInputStream::Create(kZonePointerBits, kOffsetBlockBits, blockList, IW3::XFILE_BLOCK_VIRTUAL, stream, zone->Memory(), std::nullopt);
        IW3::ContentLoader loader(*zone, *input);
        KisakZone_ResetConvertedScriptStrings();
        loader.Load();
        KisakZone_ResetConvertedScriptStrings();
        return zone.release();
    }
    catch (const std::exception& e)
    {
        if (errorOut && errorSize)
            snprintf(errorOut, errorSize, "%s", e.what());
    }
    catch (...)
    {
        if (errorOut && errorSize)
            snprintf(errorOut, errorSize, "unknown zone loading error");
    }
    return nullptr;
}

void KisakZone_Free(void* zone)
{
    delete static_cast<Zone*>(zone);
}
