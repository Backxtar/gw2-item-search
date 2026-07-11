#include "ItemSearchApp.h"
#include "Constants.h"
#include "nexus/Nexus.h"
#include <memory>

using namespace ItemSearch;

namespace
{
    AddonDefinition_t g_AddonDef = {};
    HMODULE g_Self = nullptr;
    std::unique_ptr<ItemSearchApp> g_App;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) g_Self = hModule;
    return TRUE;
}

void AddonLoad(AddonAPI_t* api)
{
    g_App = std::make_unique<ItemSearchApp>(g_Self);
    g_App->Load(api);
}

void AddonUnload()
{
    if (!g_App) return;
    g_App->Unload();
    g_App.reset();
}

void AddonRenderSearchWindow()
{
    if (!g_App) return;
    g_App->RenderSearchWindow();
}

void AddonRenderOptions()
{
    if (!g_App) return;
    g_App->RenderOptions();
}

void OnInputBind(const char* identifier, bool isRelease)
{
    if (!g_App) return;
    g_App->OnInputBind(identifier, isRelease);
}

void OnFontReceived(const char* identifier, void* font)
{
    if (!g_App) return;
    g_App->OnFontReceived(identifier, font);
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
    g_AddonDef.Signature   = -29183; 
    g_AddonDef.APIVersion  = NEXUS_API_VERSION;
    g_AddonDef.Name        = Constants::AddonName;
    g_AddonDef.Version     = { 1, 0, 6, 0 };
    g_AddonDef.Author      = "Backxtar.3879";
    g_AddonDef.Description = "Search items across your entire Guild Wars 2 account with a native GW2 UI look and feel.";
    g_AddonDef.Load        = AddonLoad;
    g_AddonDef.Unload      = AddonUnload;
    g_AddonDef.Flags       = AF_None;

    g_AddonDef.Provider    = UP_GitHub;
    g_AddonDef.UpdateLink  = "https://github.com/Backxtar/gw2-item-search";

    return &g_AddonDef;
}
