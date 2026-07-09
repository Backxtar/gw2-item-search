#pragma once
#include "ConfigStore.h"
#include "GW2ApiService.h"
#include "HttpClient.h"
#include "ItemSearchWindow.h"
#include "SharedState.h"
#include "mumble/Mumble.h"
#include "nexus/Nexus.h"
#include <condition_variable>
#include <mutex>
#include <thread>

void AddonRenderSearchWindow();
void AddonRenderOptions();
void OnInputBind(const char* identifier, bool isRelease);
void OnFontReceived(const char* identifier, void* font);

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
        void OnFontReceived(const char* identifier, void* font);
        void RequestRefresh();

    private:
        void WorkerLoop();

        HMODULE          m_Self       = nullptr;
        AddonAPI_t*      m_Api        = nullptr;
        NexusLinkData_t* m_NexusLink  = nullptr; // shared Nexus data (fonts, UI scaling)
        Mumble::Data*    m_MumbleLink = nullptr; // game state (UI size, competitive, ...)

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
