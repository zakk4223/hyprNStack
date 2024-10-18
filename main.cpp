#include "globals.hpp"
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include "nstackLayout.hpp"
#include <unistd.h>
#include <thread>
// Methods
inline std::unique_ptr<CHyprNstackLayout> g_pNstackLayout;

static void deleteWorkspaceData(int ws) {
	if (g_pNstackLayout)
		g_pNstackLayout->removeWorkspaceData(ws);
}
// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}


void moveWorkspaceCallback(void *self, SCallbackInfo &cinfo, std::any data) {
	std::vector<std::any> moveData = std::any_cast<std::vector<std::any>>(data);
	PHLWORKSPACE ws = std::any_cast<PHLWORKSPACE>(moveData.front());
	deleteWorkspaceData(ws->m_iID);
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
		static auto MWCB = HyprlandAPI::registerCallbackDynamic(PHANDLE, "moveWorkspace", moveWorkspaceCallback);

	
		static auto DWCB = HyprlandAPI::registerCallbackDynamic(PHANDLE, "destroyWorkspace", [&](void *self, SCallbackInfo &, std::any data) {
			CWorkspace *ws = std::any_cast<CWorkspace *>(data);
			deleteWorkspaceData(ws->m_iID);
		});
	
    HyprlandAPI::addLayout(PHANDLE, "nstack", g_pNstackLayout.get());

    HyprlandAPI::reloadConfig();

    return {"hyprNStack", "Plugin for column layout", "Zakk", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::invokeHyprctlCommand("seterror", "disable");
}
