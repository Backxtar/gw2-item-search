#pragma once
#include "ConfigStore.h"
#include "GW2ApiService.h"
#include "HttpClient.h"
#include "ItemSearchWindow.h"
#include "SharedState.h"
#include "nexus/Nexus.h"
#include <condition_variable>
#include <mutex>
#include <thread>

void AddonRenderSearchWindow();
void AddonRenderOptions();
void OnInputBind(const char* identifier, bool isRelease);

namespace ItemSearch
{
    class ItemSearchApp
    {
    public:
        explicit ItemSearchApp(HMODULE self);
        ~ItemSearchApp();

        void Load(AddonAPI_t* api);
        void Unload();

        void RenderSearchWindow();
        void RenderOptions();
        void OnInputBind(const char* identifier, bool isRelease);
        void RequestRefresh();

    private:
        void WorkerLoop();

        HMODULE      m_Self = nullptr;
        AddonAPI_t*  m_Api  = nullptr;

        AppState         m_SharedState;
        HttpClient       m_HttpClient;
        ConfigStore      m_ConfigStore;
        GW2ApiService    m_ApiService;
        ItemSearchWindow m_Window;

        bool m_Running          = false;
        bool m_RefreshRequested = false;

        std::thread             m_Worker;
        std::mutex              m_WorkerMutex;
        std::condition_variable m_WorkerWake;
    };
}
