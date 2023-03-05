#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <src/layout/IHyprLayout.hpp>
#include <vector>
#include <list>
#include <deque>
#include <any>

enum eFullscreenMode : uint8_t;

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

    bool     operator==(const SNstackNodeData& rhs) const {
        return pWindow == rhs.pWindow;
    }
};

struct SNstackWorkspaceData {
    int                workspaceID = -1;
    std::vector<float> stackPercs;
    std::vector<int>   stackNodeCount;
    int                m_iStackCount = 2;

    eColOrientation    orientation = NSTACK_ORIENTATION_LEFT;

    bool               operator==(const SNstackWorkspaceData& rhs) const {
        return workspaceID == rhs.workspaceID;
    }
};

class CHyprNstackLayout : public IHyprLayout {
  public:
    virtual void                     onWindowCreatedTiling(CWindow*);
    virtual void                     onWindowRemovedTiling(CWindow*);
    virtual bool                     isWindowTiled(CWindow*);
    virtual void                     recalculateMonitor(const int&);
    virtual void                     recalculateWindow(CWindow*);
    virtual void                     resizeActiveWindow(const Vector2D&, CWindow* pWindow = nullptr);
    virtual void                     fullscreenRequestForWindow(CWindow*, eFullscreenMode, bool);
    virtual std::any                 layoutMessage(SLayoutMessageHeader, std::string);
    virtual SWindowRenderLayoutHints requestRenderHints(CWindow*);
    virtual void                     switchWindows(CWindow*, CWindow*);
    virtual void                     alterSplitRatio(CWindow*, float, bool);
    virtual std::string              getLayoutName();
    virtual void                     replaceWindowDataWith(CWindow* from, CWindow* to);

    virtual void                     onEnable();
    virtual void                     onDisable();

  private:
    std::list<SNstackNodeData>        m_lMasterNodesData;
    std::vector<SNstackWorkspaceData> m_lMasterWorkspacesData;

    bool                              m_bForceWarps = false;

    int                               getNodesOnWorkspace(const int&);
    void                              applyNodeDataToWindow(SNstackNodeData*);
    void                              resetNodeSplits(const int&);
    SNstackNodeData*                  getNodeFromWindow(CWindow*);
    SNstackNodeData*                  getMasterNodeOnWorkspace(const int&);
    SNstackWorkspaceData*             getMasterWorkspaceData(const int&);
    void                              calculateWorkspace(const int&);
    CWindow*                          getNextWindow(CWindow*, bool);
    int                               getMastersOnWorkspace(const int&);
    bool                              prepareLoseFocus(CWindow*);
    void                              prepareNewFocus(CWindow*, bool inherit_fullscreen);

    friend struct SNstackNodeData;
    friend struct SNstackWorkspaceData;
};
