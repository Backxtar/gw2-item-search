#include "ItemSearchApp.h"
#include "Constants.h"
#include "FontConfig.h"
#include "Lang.h"
#include "resource.h"
#include "imgui/imgui.h"
#include <cstring>

namespace ItemSearch
{
    ItemSearchApp::ItemSearchApp(HMODULE self)
        : m_Self(self), m_ApiService(m_HttpClient)
    {}

    ItemSearchApp::~ItemSearchApp()
    {
        Unload();
    }

    void ItemSearchApp::Load(AddonAPI_t* api)
    {
        m_Api = api;

        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_Api->ImguiContext));
        ImGui::SetAllocatorFunctions(
            reinterpret_cast<void* (*)(size_t, void*)>(m_Api->ImguiMalloc),
            reinterpret_cast<void (*)(void*, void*)>(m_Api->ImguiFree));

        // Shared data links (Nexus owns the memory; pointers stay valid until unload)
        m_NexusLink  = static_cast<NexusLinkData_t*>(m_Api->DataLink_Get(DL_NEXUS_LINK));
        m_MumbleLink = static_cast<Mumble::Data*>(m_Api->DataLink_Get(DL_MUMBLE_LINK));

        m_ConfigStore.Load(m_SharedState);

        // Pre-populate items from the active account's disk cache so search
        // works immediately
        {
            std::vector<FoundItem> cached;
            m_ConfigStore.LoadItemCache(GetConfig(m_SharedState).ActiveKey(), cached);
            if (!cached.empty())
            {
                std::unique_lock il(m_SharedState.itemsLock);
                m_SharedState.items = std::move(cached);
                m_SharedState.itemsVersion.fetch_add(1, std::memory_order_release);
            }
        }

        m_Window.SetApi(m_Api);
        m_Window.SetLinks(m_NexusLink, m_MumbleLink);

        m_Api->Textures_LoadFromResource(Constants::IconId,      IDB_PNG1,  m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::IconHoverId, IDB_PNG52, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::CoinGoldId,   IDB_PNG2, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::CoinSilverId, IDB_PNG3, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::CoinCopperId, IDB_PNG4, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::FoodIconId,    IDB_PNG5, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::UtilityIconId, IDB_PNG6, m_Self, nullptr);

        // GW2-style window chrome (title bar, corner, exit button)
        m_Api->Textures_LoadFromResource(Constants::WndTitlebarId,       IDB_PNG53, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::WndTitlebarActiveId, IDB_PNG54, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::WndTopRightId,       IDB_PNG55, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::WndTopRightActiveId, IDB_PNG56, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::WndExitId,           IDB_PNG57, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::WndExitActiveId,     IDB_PNG58, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::WndBackgroundId,     IDB_PNG59, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::TooltipBgId,         IDB_PNG60, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::TextboxId,           IDB_PNG61, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::ItemHoverId,         IDB_PNG63, m_Self, nullptr);

        // Menomonia, embedded as RCDATA. Every requested size gets its own
        // Nexus font identifier ("<prefix>_<px*10>") so each size is a crisp
        // dedicated atlas font; the ImFont* arrives async via ::OnFontReceived.
        // The resource memory stays valid for the module lifetime, so the
        // window can register further sizes from the same data later.
        if (HRSRC rc = FindResourceA(m_Self, MAKEINTRESOURCEA(IDR_TTF1), "RCDATA"))
            if (HGLOBAL hg = LoadResource(m_Self, rc))
            {
                void*       data = LockResource(hg);
                const DWORD size = SizeofResource(m_Self, rc);
                if (data && size > 0)
                {
                    m_SharedState.fontData     = data;
                    m_SharedState.fontDataSize = static_cast<uint32_t>(size);

                    // Pre-register the default sizes (body, heading, window
                    // title) so the first frame renders crisply; the window
                    // registers any further configured size on demand.
                    auto reg = [&](float px)
                    {
                        char id[64];
                        std::snprintf(id, sizeof(id), "%s_%d", Constants::FontIdPrefix,
                                      static_cast<int>(px * 10.0f));
                        if (m_SharedState.fontsById.count(id)) return;
                        m_SharedState.fontsById.emplace(id, nullptr);
                        m_SharedState.addedFontIds.push_back(id);
                        m_Api->Fonts_AddFromMemory(id, px, data, size,
                                                   ::OnFontReceived, FontLoadConfig());
                    };
                    reg(Constants::FontBodySize);
                    reg(Constants::FontHeadingSize);
                    reg(Constants::FontBodySize * Constants::FontTitleScale);
                }
            }

        // Profession (class) icons — keyed "LIIS_PROF_<Profession>"
        struct ProfTex { const char* prof; int res; };
        static const ProfTex kProfTex[] = {
            {"Guardian", IDB_PNG7},  {"Revenant", IDB_PNG8},   {"Warrior", IDB_PNG9},
            {"Engineer", IDB_PNG10}, {"Ranger",   IDB_PNG11},  {"Thief",   IDB_PNG12},
            {"Elementalist", IDB_PNG13}, {"Mesmer", IDB_PNG14}, {"Necromancer", IDB_PNG15},
        };
        for (const auto& pt : kProfTex)
            m_Api->Textures_LoadFromResource((std::string(Constants::ProfIconPrefix) + pt.prof).c_str(),
                                             pt.res, m_Self, nullptr);

        // Elite specialization icons — keyed "LIIS_PROF_<EliteSpecName>"
        static const ProfTex kEliteTex[] = {
            {"Dragonhunter", IDB_PNG16}, {"Firebrand", IDB_PNG17}, {"Willbender", IDB_PNG18},
            {"Herald", IDB_PNG19},       {"Renegade", IDB_PNG20},  {"Vindicator", IDB_PNG21},
            {"Berserker", IDB_PNG22},    {"Spellbreaker", IDB_PNG23}, {"Bladesworn", IDB_PNG24},
            {"Scrapper", IDB_PNG25},     {"Holosmith", IDB_PNG26}, {"Mechanist", IDB_PNG27},
            {"Druid", IDB_PNG28},        {"Soulbeast", IDB_PNG29}, {"Untamed", IDB_PNG30},
            {"Daredevil", IDB_PNG31},    {"Deadeye", IDB_PNG32},   {"Specter", IDB_PNG33},
            {"Tempest", IDB_PNG34},      {"Weaver", IDB_PNG35},    {"Catalyst", IDB_PNG36},
            {"Chronomancer", IDB_PNG37}, {"Mirage", IDB_PNG38},    {"Virtuoso", IDB_PNG39},
            {"Reaper", IDB_PNG40},       {"Scourge", IDB_PNG41},   {"Harbinger", IDB_PNG42},
            // Visions of Eternity elite specs
            {"Amalgam", IDB_PNG43},      {"Antiquary", IDB_PNG44}, {"Conduit", IDB_PNG45},
            {"Evoker", IDB_PNG46},       {"Galeshot", IDB_PNG47},  {"Luminary", IDB_PNG48},
            {"Paragon", IDB_PNG49},      {"Ritualist", IDB_PNG50}, {"Troubadour", IDB_PNG51},
        };
        for (const auto& et : kEliteTex)
            m_Api->Textures_LoadFromResource((std::string(Constants::ProfIconPrefix) + et.prof).c_str(),
                                             et.res, m_Self, nullptr);

        m_Api->InputBinds_RegisterWithString(Constants::KeybindToggleId, ::OnInputBind, "(null)");
        m_Api->QuickAccess_Add(Constants::QuickAccessId,
                               Constants::IconId,
                               Constants::IconHoverId,
                               Constants::KeybindToggleId,
                               Constants::AddonName);
        m_Api->GUI_Register(RT_Render, ::AddonRenderSearchWindow);
        m_Api->GUI_Register(RT_OptionsRender, ::AddonRenderOptions);
        // Nexus matches the escape target by the window's FULL name (exact strcmp, not ###-aware),
        // so we must register every localized title the window can have — the title bar text
        // (and thus the ImGui window Name) changes when the user switches language at runtime.
        for (auto lang : { Lang::Language::German, Lang::Language::English })
        {
            const std::string title = std::string(Lang::Get(lang).windowTitle) + Constants::WindowId;
            m_Api->GUI_RegisterCloseOnEscape(title.c_str(),
                reinterpret_cast<bool*>(&m_SharedState.showWindow));
        }

        {
            std::lock_guard<std::mutex> lk(m_WorkerMutex);
            m_Running          = true;
            m_RefreshRequested = true; // auto-fetch on load if key is set
        }
        m_Worker = std::thread(&ItemSearchApp::WorkerLoop, this);
    }

    void ItemSearchApp::Unload()
    {
        if (!m_Api) return;

        {
            std::lock_guard<std::mutex> lk(m_WorkerMutex);
            m_Running = false;
        }
        m_WorkerWake.notify_one();
        if (m_Worker.joinable()) m_Worker.join();

        m_ConfigStore.Save(m_SharedState);

        for (auto lang : { Lang::Language::German, Lang::Language::English })
        {
            const std::string title = std::string(Lang::Get(lang).windowTitle) + Constants::WindowId;
            m_Api->GUI_DeregisterCloseOnEscape(title.c_str());
        }
        for (const auto& id : m_SharedState.addedFontIds)
            m_Api->Fonts_Release(id.c_str(), ::OnFontReceived);
        m_Api->GUI_Deregister(::AddonRenderSearchWindow);
        m_Api->GUI_Deregister(::AddonRenderOptions);
        m_Api->QuickAccess_Remove(Constants::QuickAccessId);
        m_Api->InputBinds_Deregister(Constants::KeybindToggleId);

        m_NexusLink  = nullptr;
        m_MumbleLink = nullptr;
        m_Api = nullptr;
    }

    // Permission check + full fetch for one account. Saves its item cache and
    // caches the resolved account name; publishes items/name to the shared
    // state only for the account currently shown (publishItems).
    bool ItemSearchApp::FetchAccount(const std::string& apiKey, const std::string& langStr,
                                     bool publishItems, std::string& outError)
    {
        if (apiKey.empty()) return false;

        // Permission check via /tokeninfo
        std::vector<std::string> missingPerms;
        std::string permError;
        bool permOk = false;
        try { permOk = m_ApiService.CheckPermissions(apiKey, missingPerms, permError); }
        catch (const std::exception& e) { permError = e.what(); }
        catch (...) { permError = "Unknown error checking API key."; }

        if (!permOk || !missingPerms.empty())
        {
            if (!permOk)
            {
                outError = permError;
            }
            else
            {
                outError = "Missing API permissions: ";
                for (size_t i = 0; i < missingPerms.size(); ++i)
                {
                    if (i > 0) outError += ", ";
                    outError += missingPerms[i];
                }
            }
            return false;
        }

        std::string accountName;
        std::vector<FoundItem> items;
        std::string error;

        bool ok = false;
        try { ok = m_ApiService.FetchAll(apiKey, langStr, accountName, items, error); }
        catch (const std::exception& e) { error = e.what(); }
        catch (...) { error = "Unknown error during fetch."; }

        if (!ok)
        {
            outError = error;
            return false;
        }

        m_ConfigStore.SaveItemCache(apiKey, items);
        if (publishItems)
        {
            std::unique_lock il(m_SharedState.itemsLock);
            m_SharedState.items = std::move(items);
            m_SharedState.itemsVersion.fetch_add(1, std::memory_order_release);
        }
        if (!accountName.empty())
        {
            if (publishItems)
            {
                std::unique_lock sl(m_SharedState.statusLock);
                m_SharedState.accountName = accountName;
            }
            // Cache the resolved name on the account entry (looked up by
            // key: the list may have been edited meanwhile).
            PluginConfig cur = GetConfig(m_SharedState);
            for (auto& acc : cur.accounts)
                if (acc.apiKey == apiKey) acc.accountName = accountName;
            SetConfig(m_SharedState, cur);
            m_ConfigStore.Save(m_SharedState);
        }
        return true;
    }

    void ItemSearchApp::WorkerLoop()
    {
        std::unique_lock<std::mutex> lk(m_WorkerMutex);

        while (m_Running)
        {
            m_WorkerWake.wait(lk, [this]
                { return !m_Running || m_RefreshRequested || m_RefreshAllRequested || m_SwitchRequested; });
            if (!m_Running) break;

            const bool doSwitch  = m_SwitchRequested;
            const bool doAll     = m_RefreshAllRequested;
            bool doRefresh       = m_RefreshRequested;
            m_SwitchRequested    = false;
            m_RefreshAllRequested= false;
            m_RefreshRequested   = false;
            lk.unlock();

            const PluginConfig cfg = GetConfig(m_SharedState);
            const std::string activeKey = cfg.ActiveKey();

            if (doSwitch)
            {
                // Publish the switched account's cached items immediately;
                // only fall back to a full fetch when there is no cache yet.
                std::vector<FoundItem> cached;
                m_ConfigStore.LoadItemCache(activeKey, cached);
                const bool empty = cached.empty();
                {
                    std::unique_lock il(m_SharedState.itemsLock);
                    m_SharedState.items = std::move(cached);
                    m_SharedState.itemsVersion.fetch_add(1, std::memory_order_release);
                }
                if (empty) doRefresh = true;
            }

            if ((doRefresh || doAll) && !cfg.accounts.empty())
            {
                m_SharedState.fetching.store(true, std::memory_order_relaxed);
                {
                    std::unique_lock sl(m_SharedState.statusLock);
                    m_SharedState.fetchError.clear();
                }

                const std::string langStr = (cfg.language == 0) ? "de" : "en";

                std::string errors;
                if (doAll)
                {
                    for (size_t i = 0; i < cfg.accounts.size(); ++i)
                    {
                        {
                            std::lock_guard<std::mutex> g(m_WorkerMutex);
                            if (!m_Running) break;
                        }
                        const AccountConfig& acc = cfg.accounts[i];
                        std::string err;
                        if (!FetchAccount(acc.apiKey, langStr, acc.apiKey == activeKey, err) &&
                            !err.empty())
                        {
                            if (!errors.empty()) errors += "\n";
                            errors += acc.accountName.empty()
                                      ? ("#" + std::to_string(i + 1) + ": " + err)
                                      : (acc.accountName + ": " + err);
                        }
                    }
                }
                else if (!activeKey.empty())
                {
                    FetchAccount(activeKey, langStr, true, errors);
                }

                {
                    std::unique_lock sl(m_SharedState.statusLock);
                    m_SharedState.fetchError = errors;
                }
                m_SharedState.fetching.store(false, std::memory_order_relaxed);
            }

            lk.lock();
        }
    }

    void ItemSearchApp::RequestRefresh()
    {
        {
            std::lock_guard<std::mutex> lk(m_WorkerMutex);
            m_RefreshRequested = true;
        }
        m_WorkerWake.notify_one();
    }

    void ItemSearchApp::RequestRefreshAll()
    {
        {
            std::lock_guard<std::mutex> lk(m_WorkerMutex);
            m_RefreshAllRequested = true;
        }
        m_WorkerWake.notify_one();
    }

    void ItemSearchApp::RequestAccountSwitch(int index)
    {
        PluginConfig cfg = GetConfig(m_SharedState);
        if (index < 0 || index >= static_cast<int>(cfg.accounts.size())) return;
        if (index == cfg.activeAccount) return;

        cfg.activeAccount = index;
        SetConfig(m_SharedState, cfg);
        {
            std::unique_lock sl(m_SharedState.statusLock);
            m_SharedState.accountName = cfg.accounts[index].accountName;
            m_SharedState.fetchError.clear();
        }
        m_ConfigStore.Save(m_SharedState);

        {
            std::lock_guard<std::mutex> lk(m_WorkerMutex);
            m_SwitchRequested = true;
        }
        m_WorkerWake.notify_one();
    }

    void ItemSearchApp::RenderSearchWindow()
    {
        if (!m_Api) return;
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_Api->ImguiContext));
        bool requestRefresh    = false;
        bool requestRefreshAll = false;
        int  switchAccount     = -1;
        m_Window.Render(m_SharedState, requestRefresh, requestRefreshAll, switchAccount);
        if (switchAccount >= 0)     RequestAccountSwitch(switchAccount);
        if (requestRefreshAll)      RequestRefreshAll();
        else if (requestRefresh)    RequestRefresh();
    }

    void ItemSearchApp::RenderOptions()
    {
        bool requestRefresh = false;
        m_Window.RenderOptions(m_ConfigStore, m_SharedState, requestRefresh);
        if (requestRefresh) RequestRefresh();
    }

    void ItemSearchApp::OnFontReceived(const char* identifier, void* font)
    {
        if (!identifier) return;
        // Called (again) with the ImFont* whenever Nexus (re)builds the atlas.
        // Cache every delivered size; only the currently selected identifiers
        // feed the atomics the renderer reads.
        m_SharedState.fontsById[identifier] = font;
    }

    void ItemSearchApp::OnInputBind(const char* identifier, bool isRelease)
    {
        if (isRelease || !identifier) return;
        if (std::strcmp(identifier, Constants::KeybindToggleId) == 0)
        { 
            const bool current = m_SharedState.showWindow.load(std::memory_order_relaxed);
            m_SharedState.showWindow.store(!current, std::memory_order_relaxed);
            m_ConfigStore.Save(m_SharedState);
        }
    }
}
