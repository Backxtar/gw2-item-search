#include "ItemSearchApp.h"
#include "Constants.h"
#include "Lang.h"
#include "imgui/imgui.h"
#include <cstring>

namespace LegendaryImpactItemSearch
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

        m_Api->InputBinds_RegisterWithString(Constants::KeybindToggleId, ::OnInputBind, "(null)");
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
                std::string accountName;
                std::vector<FoundItem> items;
                std::string error;

                const bool ok = m_ApiService.FetchAll(cfg.apiKey, langStr, accountName, items, error);

                {
                    std::unique_lock sl(m_SharedState.statusLock);
                    m_SharedState.accountName = accountName;
                    m_SharedState.fetchError  = ok ? "" : error;
                }

                if (ok)
                {
                    std::unique_lock il(m_SharedState.itemsLock);
                    m_SharedState.items = std::move(items);
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

    void ItemSearchApp::RenderSearchWindow()
    {
        bool requestRefresh = false;
        m_Window.Render(m_SharedState, requestRefresh);
        if (requestRefresh) RequestRefresh();
    }

    void ItemSearchApp::RenderOptions()
    {
        m_Window.RenderOptions(m_ConfigStore);
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
