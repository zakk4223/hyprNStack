#pragma once

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/layout/IHyprLayout.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/helpers/math/Math.hpp>
#include <vector>
#include <list>
#include <deque>
#include <any>

enum eFullscreenMode : int8_t;

//orientation determines which side of the screen the master area resides
enum eColOrientation : uint8_t {
    NSTACK_ORIENTATION_LEFT = 0,
    NSTACK_ORIENTATION_TOP,
    NSTACK_ORIENTATION_RIGHT,
    NSTACK_ORIENTATION_BOTTOM,
    NSTACK_ORIENTATION_HCENTER,
    NSTACK_ORIENTATION_VCENTER,
};

struct SNstackNodeData {
    bool     isMaster       = false;
    bool     masterAdjusted = false;
    float    percMaster     = 0.5f;
    int      stackNum       = 0;

    PHLWINDOWREF pWindow;

    Vector2D position;
    Vector2D size;

    float    percSize = 1.f; // size multiplier for resizing children

    int      workspaceID = -1;
		bool		 ignoreFullscreenChecks = false;

    bool     operator==(const SNstackNodeData& rhs) const {
        return pWindow.lock() == rhs.pWindow.lock();
    }
};

struct SNstackWorkspaceData {
    int                workspaceID = -1;
    std::vector<float> stackPercs;
    std::vector<int>   stackNodeCount;
    int                m_iStackCount = 2;
		bool							 new_on_top = false;
		bool							 new_is_master = true;
	  bool               center_single_master = false;
		bool							 inherit_fullscreen = true;
		int						 		 no_gaps_when_only = 0;
		float							 master_factor = 0.0f;
		float							 single_master_factor = 0.5f;
		float							 special_scale_factor = 0.8f;
    eColOrientation    orientation = NSTACK_ORIENTATION_LEFT;

    bool               operator==(const SNstackWorkspaceData& rhs) const {
        return workspaceID == rhs.workspaceID;
    }
};

class CHyprNstackLayout : public IHyprLayout {
  public:
    virtual void                     onWindowCreatedTiling(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT);
    virtual void                     onWindowRemovedTiling(PHLWINDOW);
    virtual bool                     isWindowTiled(PHLWINDOW);
    virtual void                     recalculateMonitor(const MONITORID&);
    virtual void                     recalculateWindow(PHLWINDOW);
    virtual void                     resizeActiveWindow(const Vector2D&, eRectCorner corner, PHLWINDOW pWindow = nullptr);
    virtual void                     fullscreenRequestForWindow(PHLWINDOW, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE);
    virtual std::any                 layoutMessage(SLayoutMessageHeader, std::string);
    virtual SWindowRenderLayoutHints requestRenderHints(PHLWINDOW);
    virtual void                     switchWindows(PHLWINDOW, PHLWINDOW);
		virtual void										 moveWindowTo(PHLWINDOW, const std::string& dir, bool silent);
    virtual void                     alterSplitRatio(PHLWINDOW, float, bool);
    virtual std::string              getLayoutName();
    virtual void                     replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to);
    virtual Vector2D 								 predictSizeForNewWindowTiled();

    virtual void                     onEnable();
    virtual void                     onDisable();
		void 														removeWorkspaceData(const int& ws);

  private:
    std::list<SNstackNodeData>        m_lMasterNodesData;
    std::list<SNstackWorkspaceData> m_lMasterWorkspacesData;

    bool                              m_bForceWarps = false;

    void                              buildOrientationCycleVectorFromVars(std::vector<eColOrientation>& cycle, CVarList& vars);
    void                              buildOrientationCycleVectorFromEOperation(std::vector<eColOrientation>& cycle);
    void                              runOrientationCycle(SLayoutMessageHeader& header, CVarList* vars, int next);
    int                               getNodesOnWorkspace(const int&);
    void                              applyNodeDataToWindow(SNstackNodeData*);
    void                              resetNodeSplits(const int&);
    SNstackNodeData*                  getNodeFromWindow(PHLWINDOW);
    SNstackNodeData*                  getMasterNodeOnWorkspace(const int&);
    SNstackWorkspaceData*             getMasterWorkspaceData(const int&);
    void                              calculateWorkspace(PHLWORKSPACE);
    PHLWINDOW                          getNextWindow(PHLWINDOW, bool);
    int                               getMastersOnWorkspace(const int&);
    bool                              prepareLoseFocus(PHLWINDOW);
    void                              prepareNewFocus(PHLWINDOW, bool inherit_fullscreen);

    friend struct SNstackNodeData;
    friend struct SNstackWorkspaceData;
};


template <typename CharT>
struct std::formatter<SNstackNodeData*, CharT> : std::formatter<CharT> {
    template <typename FormatContext>
    auto format(const SNstackNodeData* const& node, FormatContext& ctx) const {
        auto out = ctx.out();
        if (!node)
            return std::format_to(out, "[Node nullptr]");
        std::format_to(out, "[Node {:x}: workspace: {}, pos: {:j2}, size: {:j2}", (uintptr_t)node, node->workspaceID, node->position, node->size);
        if (node->isMaster)
            std::format_to(out, ", master");
        if (!node->pWindow.expired())
            std::format_to(out, ", window: {:x}", node->pWindow.lock());
        return std::format_to(out, "]");
    }
};
