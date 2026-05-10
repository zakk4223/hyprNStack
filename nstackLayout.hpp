#pragma once

#include "globals.hpp"
#include <string_view>
#include <vector>

#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/shared/ConfigErrors.hpp>
#include <hyprutils/string/VarList2.hpp>



namespace Layout {
  class CAlgorithm;
}



namespace Layout::Tiled {
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
	    bool         isMaster       = false;
	    bool         masterAdjusted = false;
	    float        percMaster     = 0.5f;
	    int          stackNum       = 0;
	
	    WP<ITarget> pTarget;
	
	    Vector2D     position;
	    Vector2D     size;
	
	    float        percSize = 1.f; // size multiplier for resizing children
	
	    int          workspaceID            = -1;
	    bool         ignoreFullscreenChecks = false;
	
	    bool         operator==(const SNstackNodeData& rhs) const {
	        return pTarget.lock() == rhs.pTarget.lock();
	    }
	};
	
	struct SNstackWorkspaceData {
	    WORKSPACEID                workspaceID = WORKSPACE_INVALID;
	    std::vector<float> stackPercs;
	    std::vector<int>   stackNodeCount;
	    int                m_iStackCount        = 2;
	    bool               new_on_top           = false;
	    bool               new_near_focused     = true;
	    bool               new_is_master        = true;
	    bool               center_single_master = false;
	    bool               inherit_fullscreen   = true;
	    int                no_gaps_when_only    = 0;
	    float              master_factor        = 0.0f;
	    float              single_master_factor = 0.5f;
	    float              special_scale_factor = 0.8f;
	    eColOrientation    orientation          = NSTACK_ORIENTATION_LEFT;
	
	    bool               operator==(const SNstackWorkspaceData& rhs) const {
	        return workspaceID == rhs.workspaceID;
	    }
	};
	
	struct SNstackUserWorkspaceData {
	    std::optional<int>                m_iStackCount;
	    std::optional<float> master_factor;
	    std::optional<float> single_master_factor;
	    std::optional<eColOrientation>    orientation;
	};

	class CHyprNstackAlgorithm : public ITiledAlgorithm {
	  public:
			virtual void                    newTarget(SP<ITarget> target);
	    virtual void                    movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt);
	    virtual void                    removeTarget(SP<ITarget> target);
	    virtual void                    resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
	    virtual void                    recalculate(eRecalculateReason reason = RECALCULATE_REASON_UNKNOWN);
	
	    virtual SP<ITarget>              getNextCandidate(SP<ITarget> old);
	    virtual Config::ErrorResult                 layoutMsg(const std::string_view& sv);
	    virtual std::optional<Vector2D>                 predictSizeForNewTarget();
	    virtual void                     swapTargets(SP<ITarget> a, SP<ITarget> b);
      virtual void                     moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent);

	
	  private:
	    std::vector<SP<SNstackNodeData>>      m_lMasterNodesData;
	    
	    SNstackWorkspaceData            m_workspaceData;
      SNstackUserWorkspaceData        m_userWorkspaceData;
	
	    bool                            m_bForceWarps = false;
	
	    void                            addTarget(SP<ITarget> target, bool firstMap, std::optional<Vector2D> insertionPoint = std::nullopt);
	    void                            buildOrientationCycleVectorFromVars(std::vector<eColOrientation>& cycle, Hyprutils::String::CVarList2* vars);
	    void                            buildOrientationCycleVectorFromEOperation(std::vector<eColOrientation>& cycle);
	    void                            runOrientationCycle(Hyprutils::String::CVarList2* vars, int next);
	    int                             getNodesNo();
	    void                            applyNodeDataToWindow(SNstackNodeData*);
	    void                            resetNodeSplits();
      SP<SNstackNodeData>             getClosestNode(const Vector2D& point);
	    SP<SNstackNodeData>                getNodeFromWindow(PHLWINDOW);
      SP<SNstackNodeData>             getNodeFromTarget(SP<ITarget> x);
	    SP<SNstackNodeData>                getMasterNode();
	    void                            calculateWorkspace();
			int 														getMastersCount();
	    bool                            prepareLoseFocus(PHLWINDOW);
	    void                            prepareNewFocus(PHLWINDOW, bool inherit_fullscreen);
			void 														applyWorkspaceLayoutOptions();
      bool 														isWindowTiled(PHLWINDOW pWindow);
	    SP<ITarget>                     getNextTarget(SP<ITarget>, bool, bool);
	    SP<ITarget>                     getDirectionalTarget(SP<ITarget>, Math::eDirection, bool);
	
	    friend struct SNstackNodeData;
	    friend struct SNstackWorkspaceData;
	};
}
