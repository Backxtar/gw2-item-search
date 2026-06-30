#include "ItemSearchApp.h"
#include "Constants.h"
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

        g_State = &m_SharedState;

        m_ConfigStore.Load();

        // Pre-populate items from disk cache so search works immediately
        {
            std::vector<FoundItem> cached;
            m_ConfigStore.LoadItemCache(cached);
            if (!cached.empty())
            {
                std::unique_lock il(m_SharedState.itemsLock);
                m_SharedState.items = std::move(cached);
                m_SharedState.itemsVersion.fetch_add(1, std::memory_order_release);
            }
        }

        m_Window.SetApi(m_Api);

        m_Api->Textures_LoadFromResource(Constants::IconId,      IDB_PNG1,  m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::IconHoverId, IDB_PNG52, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::CoinGoldId,   IDB_PNG2, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::CoinSilverId, IDB_PNG3, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::CoinCopperId, IDB_PNG4, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::FoodIconId,    IDB_PNG5, m_Self, nullptr);
        m_Api->Textures_LoadFromResource(Constants::UtilityIconId, IDB_PNG6, m_Self, nullptr);

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
        m_Api->GUI_RegisterCloseOnEscape(Constants::CloseOnEscapeId,
            reinterpret_cast<bool*>(&m_SharedState.showWindow));

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

        m_ConfigStore.Save();

        m_Api->GUI_DeregisterCloseOnEscape(Constants::CloseOnEscapeId);
        m_Api->GUI_Deregister(::AddonRenderSearchWindow);
        m_Api->GUI_Deregister(::AddonRenderOptions);
        m_Api->QuickAccess_Remove(Constants::QuickAccessId);
        m_Api->InputBinds_Deregister(Constants::KeybindToggleId);

        g_State = nullptr;
        m_Api   = nullptr;
    }

    void ItemSearchApp::WorkerLoop()
    {
        std::unique_lock<std::mutex> lk(m_WorkerMutex);

        while (m_Running)
        {
            m_WorkerWake.wait(lk, [this] { return !m_Running || m_RefreshRequested; });
            if (!m_Running) break;

            m_RefreshRequested = false;
            lk.unlock();

            const PluginConfig cfg = GetConfig(m_SharedState);
            if (cfg.apiKey[0] != '\0')
            {
                m_SharedState.fetching.store(true, std::memory_order_relaxed);
                {
                    std::unique_lock sl(m_SharedState.statusLock);
                    m_SharedState.fetchError.clear();
                }

                const std::string langStr = (cfg.language == 0) ? "de" : "en";

                // Permission check via /tokeninfo
                std::vector<std::string> missingPerms;
                std::string permError;
                bool permOk = false;
                try { permOk = m_ApiService.CheckPermissions(cfg.apiKey, missingPerms, permError); }
                catch (const std::exception& e) { permError = e.what(); }
                catch (...) { permError = "Unknown error checking API key."; }

                if (!permOk || !missingPerms.empty())
                {
                    std::string msg;
                    if (!permOk)
                    {
                        msg = permError;
                    }
                    else
                    {
                        msg = "Missing API permissions: ";
                        for (size_t i = 0; i < missingPerms.size(); ++i)
                        {
                            if (i > 0) msg += ", ";
                            msg += missingPerms[i];
                        }
                    }
                    {
                        std::unique_lock sl(m_SharedState.statusLock);
                        m_SharedState.fetchError = msg;
                    }
                    m_SharedState.fetching.store(false, std::memory_order_relaxed);
                }
                else
                {
                std::string accountName;
                std::vector<FoundItem> items;
                std::string error;

                bool ok = false;
                try { ok = m_ApiService.FetchAll(cfg.apiKey, langStr, accountName, items, error); }
                catch (const std::exception& e) { error = e.what(); }
                catch (...) { error = "Unknown error during fetch."; }

                {
                    std::unique_lock sl(m_SharedState.statusLock);
                    if (!accountName.empty()) m_SharedState.accountName = accountName;
                    m_SharedState.fetchError = ok ? "" : error;
                }

                if (ok)
                {
                    m_ConfigStore.SaveItemCache(items);
                    {
                        std::unique_lock il(m_SharedState.itemsLock);
                        m_SharedState.items = std::move(items);
                        m_SharedState.itemsVersion.fetch_add(1, std::memory_order_release);
                    }
                    if (!accountName.empty())
                    {
                        m_ConfigStore.CachedAccountName() = accountName;
                        m_ConfigStore.Save();
                    }
                }

                m_SharedState.fetching.store(false, std::memory_order_relaxed);
                } // end permission-ok branch
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

    void ItemSearchApp::RenderSearchWindow()
    {
        if (!m_Api) return;
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_Api->ImguiContext));
        bool requestRefresh = false;
        m_Window.Render(m_SharedState, requestRefresh);
        if (requestRefresh) RequestRefresh();
    }

    void ItemSearchApp::RenderOptions()
    {
        bool requestRefresh = false;
        m_Window.RenderOptions(m_ConfigStore, m_SharedState, requestRefresh);
        if (requestRefresh) RequestRefresh();
    }

    void ItemSearchApp::OnInputBind(const char* identifier, bool isRelease)
    {
        if (isRelease || !identifier) return;
        if (std::strcmp(identifier, Constants::KeybindToggleId) == 0)
        { 
            const bool current = m_SharedState.showWindow.load(std::memory_order_relaxed);
            m_SharedState.showWindow.store(!current, std::memory_order_relaxed);
            m_ConfigStore.Save();
        }
    }
}
