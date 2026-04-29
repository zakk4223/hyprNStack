#include "globals.hpp"
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include "nstackLayout.hpp"
#include <unistd.h>
#include <thread>
// Methods
inline std::unique_ptr<Layout::Tiled::CHyprNstackAlgorithm> g_pNstackLayout;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::CStringValue>("plugin:nstack:layout:orientation", "orientation", "left"));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::CIntValue>("plugin:nstack:layout:new_on_top", "new on top", 0));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::CIntValue>("plugin:nstack:layout:new_is_master", "new is master", 0));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::CIntValue>("plugin:nstack:layout:no_gaps_when_only", "no gaps when only", 1));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::CFloatValue>("plugin:nstack:layout:special_scale_factor", "special scale factor", 0.8f));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::CIntValue>("plugin:nstack:layout:inherit_fullscreen", "inherit fullscreen", 1));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::CIntValue>("plugin:nstack:layout:stacks", "stacks", 2));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::CIntValue>("plugin:nstack:layout:center_single_master", "center single master", 0));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::CFloatValue>("plugin:nstack:layout:mfact", "master factor", 0.0f));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::CFloatValue>("plugin:nstack:layout:single_mfact", "single master factor", 1.0f));
    g_pNstackLayout  = std::make_unique<Layout::Tiled::CHyprNstackAlgorithm>();
	  if (!HyprlandAPI::addTiledAlgo(PHANDLE, "nStack", &typeid(Layout::Tiled::CHyprNstackAlgorithm), [] { return makeUnique<Layout::Tiled::CHyprNstackAlgorithm>(); })) {
			HyprlandAPI::addNotification(PHANDLE, "[hyprgollum] addTiledAlgo failed! Can't proceed.", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
    }

    return {"hyprNStack", "Plugin for column layout", "Zakk", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::invokeHyprctlCommand("seterror", "disable");
}
