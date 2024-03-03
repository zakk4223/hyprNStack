#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
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

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:orientation", Hyprlang::STRING{"left"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:new_on_top", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:new_is_master", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:no_gaps_when_only", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:special_scale_factor", Hyprlang::FLOAT{0.8f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:inherit_fullscreen", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:stacks", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:center_single_master", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:mfact", Hyprlang::FLOAT{0.5f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:single_mfact", Hyprlang::FLOAT{0.5f});
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
