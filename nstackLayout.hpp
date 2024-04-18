#pragma once

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/layout/IHyprLayout.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
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

    CWindow* pWindow = nullptr;

    Vector2D position;
    Vector2D size;

    float    percSize = 1.f; // size multiplier for resizing children

    int      workspaceID = -1;
		bool		 ignoreFullscreenChecks = false;

    bool     operator==(const SNstackNodeData& rhs) const {
        return pWindow == rhs.pWindow;
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
    virtual void                     onWindowCreatedTiling(CWindow*, eDirection direction = DIRECTION_DEFAULT);
    virtual void                     onWindowRemovedTiling(CWindow*);
    virtual bool                     isWindowTiled(CWindow*);
    virtual void                     recalculateMonitor(const int&);
    virtual void                     recalculateWindow(CWindow*);
    virtual void                     resizeActiveWindow(const Vector2D&, eRectCorner corner, CWindow* pWindow = nullptr);
    virtual void                     fullscreenRequestForWindow(CWindow*, eFullscreenMode, bool);
    virtual std::any                 layoutMessage(SLayoutMessageHeader, std::string);
    virtual SWindowRenderLayoutHints requestRenderHints(CWindow*);
    virtual void                     switchWindows(CWindow*, CWindow*);
		virtual void										 moveWindowTo(CWindow *, const std::string& dir, bool silent);
    virtual void                     alterSplitRatio(CWindow*, float, bool);
    virtual std::string              getLayoutName();
    virtual void                     replaceWindowDataWith(CWindow* from, CWindow* to);
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
    SNstackNodeData*                  getNodeFromWindow(CWindow*);
    SNstackNodeData*                  getMasterNodeOnWorkspace(const int&);
    SNstackWorkspaceData*             getMasterWorkspaceData(const int&);
    void                              calculateWorkspace(PHLWORKSPACE);
    CWindow*                          getNextWindow(CWindow*, bool);
    int                               getMastersOnWorkspace(const int&);
    bool                              prepareLoseFocus(CWindow*);
    void                              prepareNewFocus(CWindow*, bool inherit_fullscreen);

    friend struct SNstackNodeData;
    friend struct SNstackWorkspaceData;
};
