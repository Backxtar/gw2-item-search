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
        void RequestRefreshAll(); // fetch every configured account in sequence
        // Switch to another configured account: publishes its cached name,
        // then lets the worker load its item cache (or fetch if none exists).
        void RequestAccountSwitch(int index);

    private:
        void WorkerLoop();
        // One account: permission check + fetch + per-account cache write.
        // publishItems: also push items/name into the shared (displayed) state.
        bool FetchAccount(const std::string& apiKey, const std::string& langStr,
                          bool publishItems, std::string& outError);

        HMODULE          m_Self       = nullptr;
        AddonAPI_t*      m_Api        = nullptr;
        NexusLinkData_t* m_NexusLink  = nullptr; // shared Nexus data (fonts, UI scaling)
        Mumble::Data*    m_MumbleLink = nullptr; // game state (UI size, competitive, ...)

        AppState         m_SharedState;
        HttpClient       m_HttpClient;
        ConfigStore      m_ConfigStore;
        GW2ApiService    m_ApiService;
        ItemSearchWindow m_Window;

        bool m_Running             = false;
        bool m_RefreshRequested    = false;
        bool m_RefreshAllRequested = false; // fetch all accounts, not just the active one
        bool m_SwitchRequested     = false; // account changed: load its cache first

        std::thread             m_Worker;
        std::mutex              m_WorkerMutex;
        std::condition_variable m_WorkerWake;
    };
}
