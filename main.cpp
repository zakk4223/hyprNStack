#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/helpers/Workspace.hpp>
#include "globals.hpp"
#include "nstackLayout.hpp"
#include <unistd.h>
#include <thread>
// Methods
inline std::unique_ptr<CHyprNstackLayout> g_pNstackLayout;

static void deleteWorkspaceData(CWorkspace *ws) {
	if (g_pNstackLayout)
		g_pNstackLayout->removeWorkspaceData(ws->m_iID);
}
// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}


APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:orientation", SConfigValue{.strValue = "left"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:new_on_top", SConfigValue{.intValue = 0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:new_is_master", SConfigValue{.intValue = 1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:no_gaps_when_only", SConfigValue{.intValue = 0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:special_scale_factor", SConfigValue{.floatValue = 0.8f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:inherit_fullscreen", SConfigValue{.intValue = 1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:stacks", SConfigValue{.intValue = 2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:center_single_master", SConfigValue{.intValue = 0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:mfact", SConfigValue{.floatValue = 0.5f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:single_mfact", SConfigValue{.floatValue = 0.5f});
    g_pNstackLayout = std::make_unique<CHyprNstackLayout>();
		HyprlandAPI::registerCallbackDynamic(PHANDLE, "moveWorkspace", [&](void *self, SCallbackInfo &, std::any data) {
			std::vector<void *> moveData = std::any_cast<std::vector<void *>>(data);
			CWorkspace *ws = static_cast<CWorkspace *>(moveData.front());
			deleteWorkspaceData(ws);
		});

	
		HyprlandAPI::registerCallbackDynamic(PHANDLE, "destroyWorkspace", [&](void *self, SCallbackInfo &, std::any data) {
			CWorkspace *ws = std::any_cast<CWorkspace *>(data);
			deleteWorkspaceData(ws);
		});
	
    HyprlandAPI::addLayout(PHANDLE, "nstack", g_pNstackLayout.get());

    HyprlandAPI::reloadConfig();

    return {"N_Stack layout", "Plugin for column layout", "Zakk", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::invokeHyprctlCommand("seterror", "disable");
}
