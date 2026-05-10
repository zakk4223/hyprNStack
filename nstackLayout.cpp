#include "nstackLayout.hpp"
#include <limits>
#include <hyprland/src/config/shared/workspace/WorkspaceRuleManager.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include "hyprland/src/layout/target/WindowTarget.hpp"
#include "src/layout/LayoutManager.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/layout/target/WindowTarget.hpp>
#include <hyprland/src/render/decorations/CHyprGroupBarDecoration.hpp>
#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>
#include <hyprutils/cli/Logger.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <format>
#include <optional>




using namespace Layout;
using namespace Layout::Tiled;

static std::optional<Config::INTEGER> configStringToInt(const std::string& value) {
    if (value == "true" || value == "on" || value == "yes")
        return 1;
    if (value == "false" || value == "off" || value == "no")
        return 0;

    try {
        return std::stoll(value);
    } catch (std::exception& e) {
        Log::logger->log(Log::ERR, "Nstack layoutopt integer format error for '{}': {}", value, e.what());
        return std::nullopt;
    }
}

static const CConfigValue<Config::STRING>& nstackOrientation() {
    static const CConfigValue<Config::STRING> value("plugin:nstack:layout:orientation");
    return value;
}

static const CConfigValue<Config::INTEGER>& nstackStacks() {
    static const CConfigValue<Config::INTEGER> value("plugin:nstack:layout:stacks");
    return value;
}

static const CConfigValue<Config::FLOAT>& nstackMasterFactor() {
    static const CConfigValue<Config::FLOAT> value("plugin:nstack:layout:mfact");
    return value;
}

static const CConfigValue<Config::FLOAT>& nstackSingleMasterFactor() {
    static const CConfigValue<Config::FLOAT> value("plugin:nstack:layout:single_mfact");
    return value;
}

static const CConfigValue<Config::FLOAT>& nstackSpecialScaleFactor() {
    static const CConfigValue<Config::FLOAT> value("plugin:nstack:layout:special_scale_factor");
    return value;
}

static const CConfigValue<Config::INTEGER>& nstackNewOnTop() {
    static const CConfigValue<Config::INTEGER> value("plugin:nstack:layout:new_on_top");
    return value;
}

static const CConfigValue<Config::INTEGER>& nstackNewNearFocused() {
    static const CConfigValue<Config::INTEGER> value("plugin:nstack:layout:new_near_focused");
    return value;
}

static const CConfigValue<Config::INTEGER>& nstackNewIsMaster() {
    static const CConfigValue<Config::INTEGER> value("plugin:nstack:layout:new_is_master");
    return value;
}

static const CConfigValue<Config::INTEGER>& nstackNoGapsWhenOnly() {
    static const CConfigValue<Config::INTEGER> value("plugin:nstack:layout:no_gaps_when_only");
    return value;
}

static const CConfigValue<Config::INTEGER>& nstackInheritFullscreen() {
    static const CConfigValue<Config::INTEGER> value("plugin:nstack:layout:inherit_fullscreen");
    return value;
}

static const CConfigValue<Config::INTEGER>& nstackCenterSingleMaster() {
    static const CConfigValue<Config::INTEGER> value("plugin:nstack:layout:center_single_master");
    return value;
}

SP<SNstackNodeData> CHyprNstackAlgorithm::getNodeFromTarget(SP<ITarget> x) {
    for (auto& nd : m_lMasterNodesData) {
        if (nd->pTarget == x)
            return nd;
    }

    return nullptr;
}

SP<SNstackNodeData> CHyprNstackAlgorithm::getNodeFromWindow(PHLWINDOW pWindow) {
	  return pWindow ? getNodeFromTarget(pWindow->layoutTarget()) : nullptr;
}

int CHyprNstackAlgorithm::getMastersCount() {
	  int no = 0;
    for (auto& n : m_lMasterNodesData) {
        if (n->isMaster)
            no++;
    }
	  return no;
}

void CHyprNstackAlgorithm::applyWorkspaceLayoutOptions() {

    const auto                         wsrule       = Config::workspaceRuleMgr()->getWorkspaceRuleFor(m_parent->space()->workspace());
    static const std::map<std::string, std::string> EMPTY_LAYOUT_OPTS;
    const auto&                        wslayoutopts = wsrule ? wsrule->m_layoutopts : EMPTY_LAYOUT_OPTS;

    std::string        wsorientation = *nstackOrientation();

    if (wslayoutopts.contains("nstack-orientation"))
        wsorientation = wslayoutopts.at("nstack-orientation");

    std::string cpporientation = wsorientation;
    //create on the fly if it doesn't exist yet
    if (cpporientation == "top") {
        m_workspaceData.orientation = NSTACK_ORIENTATION_TOP;
    } else if (cpporientation == "right") {
        m_workspaceData.orientation = NSTACK_ORIENTATION_RIGHT;
    } else if (cpporientation == "bottom") {
        m_workspaceData.orientation = NSTACK_ORIENTATION_BOTTOM;
    } else if (cpporientation == "left") {
        m_workspaceData.orientation = NSTACK_ORIENTATION_LEFT;
    } else if (cpporientation == "vcenter") {
        m_workspaceData.orientation = NSTACK_ORIENTATION_VCENTER;
    } else {
        m_workspaceData.orientation = NSTACK_ORIENTATION_HCENTER;
    }

    auto               wsstacks  = *nstackStacks();

    if (wslayoutopts.contains("nstack-stacks")) {
        try {
            std::string stackstr = wslayoutopts.at("nstack-stacks");
            wsstacks             = std::stol(stackstr);
        } catch (std::exception& e) { Log::logger->log(Log::ERR, "Nstack layoutopt invalid rule value for nstack-stacks: {}", e.what()); }
    }
    if (wsstacks) {
        m_workspaceData.m_iStackCount = wsstacks;
    }

    auto               wsmfact = *nstackMasterFactor();
    if (wslayoutopts.contains("nstack-mfact")) {
        std::string mfactstr = wslayoutopts.at("nstack-mfact");
        try {
            wsmfact = std::stof(mfactstr);
        } catch (std::exception& e) { Log::logger->log(Log::ERR, "Nstack layoutopt nstack-mfact format error: {}", e.what()); }
    }
    m_workspaceData.master_factor = wsmfact;

    auto               wssmfact = *nstackSingleMasterFactor();

    if (wslayoutopts.contains("nstack-single_mfact")) {
        std::string smfactstr = wslayoutopts.at("nstack-single_mfact");
        try {
            wssmfact = std::stof(smfactstr);
        } catch (std::exception& e) { Log::logger->log(Log::ERR, "Nstack layoutopt nstack-single_mfact format error: {}", e.what()); }
    }
    m_workspaceData.single_master_factor = wssmfact;

    auto               wsssfact = *nstackSpecialScaleFactor();
    if (wslayoutopts.contains("nstack-special_scale_factor")) {
        std::string ssfactstr = wslayoutopts.at("nstack-special_scale_factor");
        try {
            wsssfact = std::stof(ssfactstr);
        } catch (std::exception& e) { Log::logger->log(Log::ERR, "Nstack layoutopt nstack-special_scale_factor format error: {}", e.what()); }
    }
    m_workspaceData.special_scale_factor = wsssfact;

    auto               wsnewtop = *nstackNewOnTop();
    if (wslayoutopts.contains("nstack-new_on_top"))
        wsnewtop = configStringToInt(wslayoutopts.at("nstack-new_on_top")).value_or(0);
    m_workspaceData.new_on_top = wsnewtop;

    auto               wsnewnearfocused = *nstackNewNearFocused();
    if (wslayoutopts.contains("nstack-new_near_focused"))
        wsnewnearfocused = configStringToInt(wslayoutopts.at("nstack-new_near_focused")).value_or(0);
    m_workspaceData.new_near_focused = wsnewnearfocused;

    auto               wsnewmaster = *nstackNewIsMaster();
    if (wslayoutopts.contains("nstack-new_is_master"))
        wsnewmaster = configStringToInt(wslayoutopts.at("nstack-new_is_master")).value_or(0);
    m_workspaceData.new_is_master = wsnewmaster;

    auto               wsngwo = *nstackNoGapsWhenOnly();
    if (wslayoutopts.contains("nstack-no_gaps_when_only"))
        wsngwo = configStringToInt(wslayoutopts.at("nstack-no_gaps_when_only")).value_or(0);
    m_workspaceData.no_gaps_when_only = wsngwo;

    auto               wsinheritfs = *nstackInheritFullscreen();
    if (wslayoutopts.contains("nstack-inherit_fullscreen"))
        wsinheritfs = configStringToInt(wslayoutopts.at("nstack-inherit_fullscreen")).value_or(0);
    m_workspaceData.inherit_fullscreen = wsinheritfs;

    auto               wscentersm = *nstackCenterSingleMaster();
    if (wslayoutopts.contains("nstack-center_single_master"))
        wscentersm = configStringToInt(wslayoutopts.at("nstack-center_single_master")).value_or(0);
    m_workspaceData.center_single_master = wscentersm;
}


SP<SNstackNodeData> CHyprNstackAlgorithm::getMasterNode() {
    for (auto& n : m_lMasterNodesData) {
        if (n->isMaster)
            return n;
    }

    return nullptr;
}

void CHyprNstackAlgorithm::resetNodeSplits() {

    calculateWorkspace();
}

void CHyprNstackAlgorithm::newTarget(SP<ITarget> target) {
	addTarget(target, true);
}

void CHyprNstackAlgorithm::movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint) {
    if (!focalPoint)
        focalPoint = target->position().middle();

    addTarget(target, false, focalPoint);
}


int CHyprNstackAlgorithm::getNodesNo() {
	return m_lMasterNodesData.size();
}


void CHyprNstackAlgorithm::addTarget(SP<ITarget> target, bool firstMap, std::optional<Vector2D> insertionPoint) {
    const auto PWORKSPACE = m_parent->space()->workspace();

    applyWorkspaceLayoutOptions();

    const auto PMONITOR = PWORKSPACE->m_monitor;

    auto       OPENINGON = getMasterNode();
    if (insertionPoint)
        OPENINGON = getClosestNode(*insertionPoint);
    else if (isWindowTiled(Desktop::focusState()->window()) && Desktop::focusState()->window()->m_workspace == PWORKSPACE)
        OPENINGON = getNodeFromWindow(Desktop::focusState()->window());

    auto INSERTPOSITION = m_workspaceData.new_on_top ? m_lMasterNodesData.begin() : m_lMasterNodesData.end();
    if (insertionPoint && OPENINGON) {
        const auto OPENINGPOSITION = std::ranges::find(m_lMasterNodesData, OPENINGON);
        if (OPENINGPOSITION != m_lMasterNodesData.end()) {
            const auto ORIENTATION    = m_userWorkspaceData.orientation.value_or(m_workspaceData.orientation);
            const auto BOX            = OPENINGON->pTarget->position();
            const bool VERTICALSTACKS = ORIENTATION == NSTACK_ORIENTATION_TOP || ORIENTATION == NSTACK_ORIENTATION_BOTTOM || ORIENTATION == NSTACK_ORIENTATION_VCENTER;
            const bool INSERTAFTER    = VERTICALSTACKS ? insertionPoint->y > BOX.middle().y : insertionPoint->x > BOX.middle().x;
            INSERTPOSITION            = INSERTAFTER ? std::next(OPENINGPOSITION) : OPENINGPOSITION;
        }
    } else if (m_workspaceData.new_near_focused && OPENINGON) {
        const auto FOCUSEDPOSITION = std::ranges::find(m_lMasterNodesData, OPENINGON);
        if (FOCUSEDPOSITION != m_lMasterNodesData.end())
            INSERTPOSITION = std::next(FOCUSEDPOSITION);
    }

    const auto PNODE = *m_lMasterNodesData.emplace(INSERTPOSITION, makeShared<SNstackNodeData>());
	  PNODE->pTarget = target;

    const auto WINDOWSONWORKSPACE = getNodesNo();
    float      lastSplitPercent   = 0.5f;
    bool       lastMasterAdjusted = false;

	/*
	 * TODO
    if (g_pInputManager->m_wasDraggingWindow && OPENINGON) {
        if (OPENINGON->pWindow.lock()->checkInputOnDecos(INPUT_TYPE_DRAG_END, MOUSECOORDS, pWindow))
            return;
    }
		*/

    bool newWindowIsMaster = false;
    if (m_workspaceData.new_is_master || WINDOWSONWORKSPACE == 1 || (!firstMap && OPENINGON->isMaster))
        newWindowIsMaster = true;
    if (newWindowIsMaster) {
        for (auto& nd : m_lMasterNodesData) {
            if (nd->isMaster && nd->workspaceID == PNODE->workspaceID) {
                nd->isMaster        = false;
                lastSplitPercent   = nd->percMaster;
                lastMasterAdjusted = nd->masterAdjusted;
                break;
            }
        }

        PNODE->isMaster       = true;
        PNODE->percMaster     = lastSplitPercent;
        PNODE->masterAdjusted = lastMasterAdjusted;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = target->maxSize().value_or(Math::VECTOR2D_MAX); MAXSIZE.x < PMONITOR->m_size.x * lastSplitPercent || MAXSIZE.y < PMONITOR->m_size.y) {
            // we can't continue. make it floating.
			      m_parent->setFloating(target, true, true);
			      std::erase(m_lMasterNodesData, PNODE);
            return;
        }
    } else {
        PNODE->isMaster = false;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = target->maxSize().value_or(Math::VECTOR2D_MAX);
            MAXSIZE.x < PMONITOR->m_size.x * (1 - lastSplitPercent) || MAXSIZE.y < PMONITOR->m_size.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            // we can't continue. make it floating.
			      m_parent->setFloating(target, true, true);
			      std::erase(m_lMasterNodesData, PNODE);
            return;
        }
    }

    // recalc
    calculateWorkspace();
}


void CHyprNstackAlgorithm::recalculate(eRecalculateReason reason) {
	calculateWorkspace();
}


void CHyprNstackAlgorithm::calculateWorkspace() {

	  const auto PMASTERNODE = getMasterNode();
		if (!PMASTERNODE)
		  return;

	  Hyprutils::Utils::CScopeGuard x([this] {
		    g_pHyprRenderer->damageMonitor(m_parent->space()->workspace()->m_monitor.lock());
	  });


    auto            NUMSTACKS      = std::max(2, m_userWorkspaceData.m_iStackCount.value_or(m_workspaceData.m_iStackCount));

    const auto      NODECOUNT   = getNodesNo();

	  const auto WORKAREA = m_parent->space()->workArea();
	  const auto PMONITOR = m_parent->space()->workspace()->m_monitor;



    eColOrientation orientation = m_userWorkspaceData.orientation.value_or(m_workspaceData.orientation);


    const auto MASTERS     = getMastersCount();
    const auto ONLYMASTERS = !(NODECOUNT - MASTERS);
	  const auto MASTERFACTOR = m_userWorkspaceData.master_factor.value_or(m_workspaceData.master_factor);


    if (NUMSTACKS < 3 && orientation > NSTACK_ORIENTATION_BOTTOM) {
        NUMSTACKS = 3;
    }

    if (!PMASTERNODE->masterAdjusted) {
        if (getNodesNo() < NUMSTACKS) {
            PMASTERNODE->percMaster = MASTERFACTOR ? MASTERFACTOR : 1.0f / getNodesNo();
        } else {
            PMASTERNODE->percMaster = MASTERFACTOR ? MASTERFACTOR : 1.0f / (NUMSTACKS);
        }
    }
    bool centerMasterWindow = false;
    if (m_workspaceData.center_single_master)
        centerMasterWindow = true;
    if (!ONLYMASTERS && NODECOUNT - MASTERS < 2) {
        if (orientation == NSTACK_ORIENTATION_HCENTER) {
            orientation = NSTACK_ORIENTATION_LEFT;
        } else if (orientation == NSTACK_ORIENTATION_VCENTER) {
            orientation = NSTACK_ORIENTATION_TOP;
        }
    }

    auto MCONTAINERPOS  = Vector2D(0.0f, 0.0f);
    auto MCONTAINERSIZE = Vector2D(0.0f, 0.0f);

    if (ONLYMASTERS) {
        if (centerMasterWindow) {

			      const auto SINGLEMFACT = m_userWorkspaceData.single_master_factor.value_or(m_workspaceData.single_master_factor);
            if (!PMASTERNODE->masterAdjusted)
                PMASTERNODE->percMaster = SINGLEMFACT ? SINGLEMFACT: 0.5f;

            if (orientation == NSTACK_ORIENTATION_TOP || orientation == NSTACK_ORIENTATION_BOTTOM) {
                const float HEIGHT        = WORKAREA.h * PMASTERNODE->percMaster;
                float       CENTER_OFFSET = (WORKAREA.h - HEIGHT) / 2;
                MCONTAINERSIZE            = Vector2D(WORKAREA.w, HEIGHT);
                MCONTAINERPOS             = Vector2D(0.0, CENTER_OFFSET);
            } else {
                const float WIDTH        = WORKAREA.w * PMASTERNODE->percMaster;
                float       CENTER_OFFSET = (WORKAREA.w - WIDTH) / 2;
                MCONTAINERSIZE            = Vector2D(WIDTH, WORKAREA.h);
                MCONTAINERPOS             = Vector2D(CENTER_OFFSET, 0.0);
            }
        } else {
            MCONTAINERPOS  = Vector2D(0.0f, 0.0f); 
            MCONTAINERSIZE = WORKAREA.size(); 
        }
    } else {
        const float MASTERSIZE = orientation % 2 == 0 ? (WORKAREA.w * PMASTERNODE->percMaster) :
                                                        (WORKAREA.h * PMASTERNODE->percMaster);

        if (orientation == NSTACK_ORIENTATION_RIGHT) {
						MCONTAINERPOS = Vector2D(WORKAREA.w - MASTERSIZE, 0.0f);
						MCONTAINERSIZE = Vector2D(MASTERSIZE, WORKAREA.h);
        } else if (orientation == NSTACK_ORIENTATION_LEFT) {
            MCONTAINERPOS  = Vector2D(0.0f, 0.0f);
						MCONTAINERSIZE = Vector2D(MASTERSIZE, WORKAREA.h);
        } else if (orientation == NSTACK_ORIENTATION_TOP) {
						MCONTAINERPOS = Vector2D(0.0f, 0.0f); 
					  MCONTAINERSIZE = Vector2D(WORKAREA.w, MASTERSIZE);
        } else if (orientation == NSTACK_ORIENTATION_BOTTOM) {
						MCONTAINERPOS =  Vector2D(0.0f, WORKAREA.h - MASTERSIZE);
            MCONTAINERSIZE = Vector2D(WORKAREA.w, MASTERSIZE);
        } else if (orientation == NSTACK_ORIENTATION_HCENTER) {
            float CENTER_OFFSET = (WORKAREA.w - MASTERSIZE) / 2;
            MCONTAINERSIZE      = Vector2D(MASTERSIZE, WORKAREA.h);
            MCONTAINERPOS       = Vector2D(CENTER_OFFSET, 0.0);
        } else if (orientation == NSTACK_ORIENTATION_VCENTER) {
            float CENTER_OFFSET = (WORKAREA.h - MASTERSIZE) / 2;
            MCONTAINERSIZE      = Vector2D(WORKAREA.w, MASTERSIZE);
            MCONTAINERPOS       = Vector2D(0.0, CENTER_OFFSET);
        }
    }

	  
    if (MCONTAINERSIZE != Vector2D(0, 0)) {
        float       nodeSpaceLeft = orientation % 2 == 0 ? MCONTAINERSIZE.y : MCONTAINERSIZE.x;
        int         nodesLeft     = MASTERS;
        float       nextNodeCoord = 0;
        const float MASTERSIZE    = orientation % 2 == 0 ? MCONTAINERSIZE.x : MCONTAINERSIZE.y;
        for (auto& n : m_lMasterNodesData) {
            if (n->isMaster) {
                if (orientation == NSTACK_ORIENTATION_RIGHT) {
                    n->position = MCONTAINERPOS + Vector2D(0.0f, nextNodeCoord);
                } else if (orientation == NSTACK_ORIENTATION_LEFT) {
                    n->position = MCONTAINERPOS + Vector2D(0.0, nextNodeCoord);
                } else if (orientation == NSTACK_ORIENTATION_TOP) {
                    n->position = MCONTAINERPOS + Vector2D(nextNodeCoord, 0.0);
                } else if (orientation == NSTACK_ORIENTATION_BOTTOM) {
                    n->position = MCONTAINERPOS + Vector2D(nextNodeCoord, 0.0);
                } else if (orientation == NSTACK_ORIENTATION_HCENTER) {
                    n->position = MCONTAINERPOS + Vector2D(0.0, nextNodeCoord);
                } else {
                    n->position = MCONTAINERPOS + Vector2D(nextNodeCoord, 0.0);
                }

                float NODESIZE = nodesLeft > 1 ? nodeSpaceLeft / nodesLeft * n->percSize : nodeSpaceLeft;
                if (NODESIZE > nodeSpaceLeft * 0.9f && nodesLeft > 1)
                    NODESIZE = nodeSpaceLeft * 0.9f;

                n->size = orientation % 2 == 0 ? Vector2D(MASTERSIZE, NODESIZE) : Vector2D(NODESIZE, MASTERSIZE);
                nodesLeft--;
                nodeSpaceLeft -= NODESIZE;
                nextNodeCoord += NODESIZE;
								n->pTarget->setPositionGlobal({n->position + WORKAREA.pos(), n->size});
            }
        }
    }

    //compute placement of slave window(s)
    int slavesLeft  = getNodesNo() - MASTERS;
    int slavesTotal = slavesLeft;
    if (slavesTotal < 1)
        return;
    int numStacks      = slavesTotal > NUMSTACKS - 1 ? NUMSTACKS - 1 : slavesTotal;
    int numStackBefore = numStacks / 2 + numStacks % 2;
    int numStackAfter  = numStacks / 2;

    m_workspaceData.stackNodeCount.assign(numStacks + 1, 0);
    m_workspaceData.stackPercs.resize(numStacks + 1, 1.0f);

    float                 stackNodeSizeLeft = orientation % 2 == 1 ? WORKAREA.w : WORKAREA.h;
    int                   stackNum          = (slavesTotal - slavesLeft) % numStacks;
    std::vector<float>    nodeSpaceLeft(numStacks, stackNodeSizeLeft);
    std::vector<float>    nodeNextCoord(numStacks, 0);
    std::vector<Vector2D> stackCoords(numStacks, Vector2D(0, 0));

    const float STACKSIZE = orientation % 2 == 1 ? (WORKAREA.h - PMASTERNODE->size.y) / numStacks :
                                                   (WORKAREA.w - PMASTERNODE->size.x) / numStacks;

    const float STACKSIZEBEFORE = numStackBefore ? ((STACKSIZE * numStacks) / 2) / numStackBefore : 0.0f;
    const float STACKSIZEAFTER  = numStackAfter ? ((STACKSIZE * numStacks) / 2) / numStackAfter : 0.0f;

    //Pre calculate each stack's coordinates so we can take into account manual resizing
    if (orientation == NSTACK_ORIENTATION_LEFT || orientation == NSTACK_ORIENTATION_TOP) {
        numStackBefore = 0;
        numStackAfter  = numStacks;
    } else if (orientation == NSTACK_ORIENTATION_RIGHT || orientation == NSTACK_ORIENTATION_BOTTOM) {
        numStackAfter  = 0;
        numStackBefore = numStacks;
    }

    for (int i = 0; i < numStacks; i++) {
        float useSize = STACKSIZE;
        if (orientation > NSTACK_ORIENTATION_BOTTOM) {
            if (i < numStackBefore)
                useSize = STACKSIZEBEFORE;
            else
                useSize = STACKSIZEAFTER;
        }

        //The Vector here isn't 'x,y', it is 'stack start, stack end'
        double coordAdjust = 0;
        if (i == numStackBefore && numStackAfter) {
            coordAdjust = orientation % 2 == 1 ? PMASTERNODE->position.y + PMASTERNODE->size.y :
                                                 PMASTERNODE->position.x + PMASTERNODE->size.x;
        }
        float monMax     = orientation % 2 == 1 ? WORKAREA.h : WORKAREA.w; 
        float stackStart = 0.0f;
        if (i == numStackBefore && numStackAfter) {
            stackStart = coordAdjust;
        } else if (i) {
            stackStart = stackCoords[i - 1].y;
        }
        float scaledSize = useSize * m_workspaceData.stackPercs[i + 1];

        //Stacks at bottom and right always fill remaining space
        //Stacks that end adjacent to the master stack are pinned to it

        if (orientation == NSTACK_ORIENTATION_LEFT && i >= numStacks - 1) {
            scaledSize = monMax - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_RIGHT && i >= numStacks - 1) {
            scaledSize = (WORKAREA.w - PMASTERNODE->position.x) - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_TOP && i >= numStacks - 1) {
            scaledSize = monMax - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_BOTTOM && i >= numStacks - 1) {
            scaledSize = (WORKAREA.h - PMASTERNODE->position.y) - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_HCENTER) {
            if (i >= numStacks - 1) {
                scaledSize = monMax - stackStart;
            } else if (i == numStacks - 2) {
                scaledSize = (PMASTERNODE->position.x) - stackStart;
            }
        } else if (orientation == NSTACK_ORIENTATION_VCENTER) {
            if (i >= numStacks - 1) {
                scaledSize = monMax - stackStart;
            } else if (i == numStacks - 2) {
                scaledSize = (PMASTERNODE->position.y) - stackStart;
            }
        }
        stackCoords[i] = Vector2D(stackStart, stackStart + scaledSize);
    }

    for (auto& nd : m_lMasterNodesData) {
        if (nd->isMaster)
            continue;

        Vector2D stackPos = stackCoords[stackNum];
        if (orientation % 2 == 0) {
            nd->position = Vector2D(stackPos.x,  nodeNextCoord[stackNum]);
        } else {
            nd->position = Vector2D(nodeNextCoord[stackNum], stackPos.x);
        }

        int nodeDiv = slavesTotal / numStacks;
        if (slavesTotal % numStacks && stackNum < slavesTotal % numStacks)
            nodeDiv++;
        float NODESIZE = slavesLeft > numStacks ? (stackNodeSizeLeft / nodeDiv) * nd->percSize : nodeSpaceLeft[stackNum];
        if (NODESIZE > nodeSpaceLeft[stackNum] * 0.9f && slavesLeft > numStacks)
            NODESIZE = nodeSpaceLeft[stackNum] * 0.9f;
        nd->stackNum = stackNum + 1;
        nd->size     = orientation % 2 == 1 ? Vector2D(NODESIZE, stackPos.y - stackPos.x) : Vector2D(stackPos.y - stackPos.x, NODESIZE);
        m_workspaceData.stackNodeCount[nd->stackNum]++;
        slavesLeft--;
        nodeSpaceLeft[stackNum] -= NODESIZE;
        nodeNextCoord[stackNum] += NODESIZE;
        stackNum = (slavesTotal - slavesLeft) % numStacks;
				nd->pTarget->setPositionGlobal({nd->position + WORKAREA.pos(), nd->size});
    }
}


bool CHyprNstackAlgorithm::isWindowTiled(PHLWINDOW pWindow) {
    return pWindow && !pWindow->layoutTarget()->floating();
}

void CHyprNstackAlgorithm::resizeTarget(const Vector2D& pixResize, SP<ITarget> target, eRectCorner corner) {
    const auto PNODE = getNodeFromTarget(target);

    if (!PNODE)
        return;

    // get monitor
    const auto PMONITOR = m_parent->space()->workspace()->m_monitor; 

    m_bForceWarps = true;

	  const auto WORKAREA = m_parent->space()->workArea();

    const auto ORIENTATION = m_userWorkspaceData.orientation.value_or(m_workspaceData.orientation);
    const auto PMASTERNODE    = getMasterNode();
    bool       xResizeDone    = false;
    bool       yResizeDone    = false;

    bool       masterAdjacent = false;

    /*
    if ((m_workspaceData.orientation == NSTACK_ORIENTATION_LEFT || m_workspaceData.orientation == NSTACK_ORIENTATION_TOP)  && PNODE->stackNum == 1) {
	    masterAdjacent = true;
    } else if ((m_workspaceData.orientation == NSTACK_ORIENTATION_RIGHT || m_workspaceData.orientation == NSTACK_ORIENTATION_BOTTOM) && PNODE->stackNum == m_workspaceData.stackNodeCount.size()-1) {
	    masterAdjacent = true;
    } else if ((m_workspaceData.orientation == NSTACK_ORIENTATION_HCENTER || m_workspaceData.orientation == NSTACK_ORIENTATION_VCENTER) && (PNODE->stackNum >= m_workspaceData.stackNodeCount.size()-2)) {
	    masterAdjacent = true;
    }*/

    if (PNODE->isMaster || masterAdjacent) {
        double delta = 0;

        switch (ORIENTATION) {
            case NSTACK_ORIENTATION_LEFT:
                delta       = pixResize.x / WORKAREA.w;
                xResizeDone = true;
                break;
            case NSTACK_ORIENTATION_RIGHT:
                delta       = -pixResize.x / WORKAREA.w; 
                xResizeDone = true;
                break;
            case NSTACK_ORIENTATION_BOTTOM:
                delta       = -pixResize.y / WORKAREA.h;
                yResizeDone = true;
                break;
            case NSTACK_ORIENTATION_TOP:
                delta       = pixResize.y / WORKAREA.h;
                yResizeDone = true;
                break;
            case NSTACK_ORIENTATION_HCENTER:
                delta       = pixResize.x / WORKAREA.w;
                xResizeDone = true;
                break;
            case NSTACK_ORIENTATION_VCENTER:
                delta       = pixResize.y / WORKAREA.h; 
                yResizeDone = true;
                break;
            default: UNREACHABLE();
        }

        for (auto& n : m_lMasterNodesData) {
            if (n->isMaster) { 
                n->percMaster     = std::clamp(n->percMaster + delta, 0.05, 0.95);
                n->masterAdjusted = true;
            }
        }
    }

    if (pixResize.x != 0 && !xResizeDone) {
        //Only handle master 'in stack' resizing. Master column resizing is handled above
        if (PNODE->isMaster && getMastersCount() > 1 && ORIENTATION % 2 == 1) {
            const auto SIZE = WORKAREA.w / getMastersCount();
            PNODE->percSize = std::clamp(PNODE->percSize + pixResize.x / SIZE, 0.05, 1.95);
        } else if (!PNODE->isMaster && (getNodesNo() - getMastersCount()) > 1) {
            if (ORIENTATION % 2 == 1) { //In stack resize

                const auto SIZE = WORKAREA.w / m_workspaceData.stackNodeCount[PNODE->stackNum];
                PNODE->percSize = std::clamp(PNODE->percSize + pixResize.x / SIZE, 0.05, 1.95);
            } else {
                const auto SIZE =
                    WORKAREA.w / m_workspaceData.stackNodeCount.size();
                // Check if this is the rightmost stack (which fills remaining space)
                // For LEFT/RIGHT orientations, the rightmost stack doesn't have its own percentage
                // Instead, we need to adjust the adjacent stack to make the rightmost stack appear to resize
                bool isRightmostStack = false;
                if (ORIENTATION == NSTACK_ORIENTATION_LEFT && PNODE->stackNum == m_workspaceData.stackNodeCount.size() - 1) {
                    isRightmostStack = true;
                } else if (ORIENTATION  == NSTACK_ORIENTATION_RIGHT && PNODE->stackNum == m_workspaceData.stackNodeCount.size() - 1) {
                    isRightmostStack = true;
                }
                
                if (isRightmostStack && PNODE->stackNum > 1) {
                    // For the rightmost stack, adjust the adjacent stack (to its left) instead
                    m_workspaceData.stackPercs[PNODE->stackNum - 1] = std::clamp(m_workspaceData.stackPercs[PNODE->stackNum - 1] + pixResize.x / SIZE, 0.05, 1.95);
                } else {
                    m_workspaceData.stackPercs[PNODE->stackNum] = std::clamp(m_workspaceData.stackPercs[PNODE->stackNum] + pixResize.x / SIZE, 0.05, 1.95);
                }
            }
        }
    }

    if (pixResize.y != 0 && !yResizeDone) {
        //Only handle master 'in stack' resizing. Master column resizing is handled above
        if (PNODE->isMaster && getMastersCount() > 1 && ORIENTATION % 2 == 0) {
            const auto SIZE = WORKAREA.h / getMastersCount();
            PNODE->percSize = std::clamp(PNODE->percSize + pixResize.y / SIZE, 0.05, 1.95);
        } else if (!PNODE->isMaster && (getNodesNo() - getMastersCount()) > 1) {
            if (ORIENTATION % 2 == 0) { //In stack resize

                const auto SIZE = WORKAREA.h / m_workspaceData.stackNodeCount[PNODE->stackNum];
                PNODE->percSize = std::clamp(PNODE->percSize + pixResize.y / SIZE, 0.05, 1.95);
            } else {
                const auto SIZE =
                    WORKAREA.h / m_workspaceData.stackNodeCount.size();
                // Check if this is the bottommost stack (which fills remaining space)
                // For TOP/BOTTOM orientations, the bottommost stack doesn't have its own percentage
                // Instead, we need to adjust the adjacent stack to make the bottommost stack appear to resize
                bool isBottommostStack = false;
                if (ORIENTATION == NSTACK_ORIENTATION_TOP && PNODE->stackNum == m_workspaceData.stackNodeCount.size() - 1) {
                    isBottommostStack = true;
                } else if (ORIENTATION == NSTACK_ORIENTATION_BOTTOM && PNODE->stackNum == m_workspaceData.stackNodeCount.size() - 1) {
                    isBottommostStack = true;
                }
                
                if (isBottommostStack && PNODE->stackNum > 1) {
                    // For the bottommost stack, adjust the adjacent stack (above it) instead
                    m_workspaceData.stackPercs[PNODE->stackNum - 1] = std::clamp(m_workspaceData.stackPercs[PNODE->stackNum - 1] + pixResize.y / SIZE, 0.05, 1.95);
                } else {
                    m_workspaceData.stackPercs[PNODE->stackNum] = std::clamp(m_workspaceData.stackPercs[PNODE->stackNum] + pixResize.y / SIZE, 0.05, 1.95);
                }
            }
        }
    }

   calculateWorkspace();

    m_bForceWarps = false;
}




SP<ITarget> CHyprNstackAlgorithm::getNextTarget(SP<ITarget> t, bool next, bool loop) {
    if (!t || t->floating())
        return nullptr;

    const auto PNODE = getNodeFromTarget(t);

    auto       nodes = m_lMasterNodesData;
    if (!next)
        std::ranges::reverse(nodes);

    const auto NODEIT = std::ranges::find(nodes, PNODE);

    const bool ISMASTER = PNODE->isMaster;

    auto       CANDIDATE = std::find_if(NODEIT, nodes.end(), [&](const auto& other) { return other != PNODE && ISMASTER == other->isMaster; });
    if (CANDIDATE == nodes.end())
        CANDIDATE = std::ranges::find_if(nodes, [&](const auto& other) { return other != PNODE && ISMASTER != other->isMaster; });

    if (CANDIDATE != nodes.end() && !loop) {
        if ((*CANDIDATE)->isMaster && next)
            return nullptr;
        if (!(*CANDIDATE)->isMaster && ISMASTER && !next)
            return nullptr;
    }

    return CANDIDATE == nodes.end() ? nullptr : (*CANDIDATE)->pTarget.lock();
}


void CHyprNstackAlgorithm::swapTargets(SP<ITarget> a, SP<ITarget> b) {

	auto nodeA = getNodeFromTarget(a);
	auto nodeB = getNodeFromTarget(b);

	if (nodeA)
		nodeA->pTarget = b;
	if (nodeB)
		nodeB->pTarget = a;
}


void CHyprNstackAlgorithm::removeTarget(SP<ITarget> target) {
    const auto PNODE = getNodeFromTarget(target);

    if (!PNODE)
        return;

    if (target->fullscreenMode() != FSMODE_NONE)
        g_pCompositor->setWindowFullscreenInternal(target->window(), FSMODE_NONE);

    const auto MASTERSLEFT = getMastersCount();

    if (PNODE->isMaster && MASTERSLEFT < 2) {
        // find new one
        for (auto& nd : m_lMasterNodesData) {
            if (!nd->isMaster) {
                nd->isMaster       = true;
                nd->percMaster     = PNODE->percMaster;
                nd->masterAdjusted = PNODE->masterAdjusted;
                break;
            }
        }
    }


	  std::erase(m_lMasterNodesData, PNODE);

    if (getMastersCount() == getNodesNo() && MASTERSLEFT > 1) {
        for (auto& it : m_lMasterNodesData | std::views::reverse) { 
                it->isMaster = false;
                break;
        }
    }
		if (getNodesNo() == 1) {
        for (auto& nd : m_lMasterNodesData) {
            if (!nd->isMaster) {
                nd->isMaster = true;
                break;
            }
        }
    }
		calculateWorkspace();
}


std::optional<Vector2D> CHyprNstackAlgorithm::predictSizeForNewTarget() {

    const auto  MONITOR = m_parent->space()->workspace()->m_monitor;

    if (!MONITOR)
        return std::nullopt;

    const int NODES = getNodesNo();

    if (NODES <= 0)
        return Desktop::focusState()->monitor()->m_size;

    const auto MASTER = getMasterNode();
    if (!MASTER) // wtf
        return std::nullopt;

    if (m_workspaceData.new_is_master) {
        return MASTER->size;
    } else {
        const auto SLAVES = NODES - getMastersCount();

        // TODO: make this better
        if (SLAVES == 0)
            return Vector2D{MONITOR->m_size.x / 2.F, MONITOR->m_size.y};
        else
            return Vector2D{MONITOR->m_size.x - MASTER->size.x, MONITOR->m_size.y / (SLAVES + 1)};
    }

    return std::nullopt;
}

SP<ITarget> CHyprNstackAlgorithm::getNextCandidate(SP<ITarget> old) {
    const auto MIDDLE = old->position().middle();

    if (const auto NODE = getClosestNode(MIDDLE); NODE)
        return NODE->pTarget.lock();

    if (const auto NODE = getMasterNode(); NODE)
        return NODE->pTarget.lock();

    return nullptr;
}
SP<SNstackNodeData> CHyprNstackAlgorithm::getClosestNode(const Vector2D& point) {
    SP<SNstackNodeData> res         = nullptr;
    double              distClosest = -1;
    for (auto& n : m_lMasterNodesData) {
        if (n->pTarget && Desktop::View::validMapped(n->pTarget->window())) {
            const auto BOX         = n->pTarget->position();
            auto       distAnother = vecToRectDistanceSquared(point, BOX.pos(), BOX.pos() + BOX.size());
            if (!res || distAnother < distClosest) {
                res         = n;
                distClosest = distAnother;
            }
        }
    }
    return res;
}

SP<ITarget> CHyprNstackAlgorithm::getDirectionalTarget(SP<ITarget> t, Math::eDirection dir, bool loop) {
    if (!t || !validMapped(t->window()))
        return nullptr;

    if (const auto PWINDOW2 = g_pCompositor->getWindowInDirection(t->window(), dir); PWINDOW2 && PWINDOW2 != t->window() && PWINDOW2->m_workspace == t->window()->m_workspace) {
        if (const auto NODE = getNodeFromWindow(PWINDOW2); NODE)
            return NODE->pTarget.lock();
    }

    if (!loop)
        return nullptr;

    const auto CURRENT = getNodeFromTarget(t);
    if (!CURRENT)
        return nullptr;

    const Vector2D CURRENTCENTER = CURRENT->position + CURRENT->size / 2.F;

    SP<SNstackNodeData> directTarget = nullptr;
    double              directScore  = std::numeric_limits<double>::infinity();
    SP<SNstackNodeData> wrapTarget   = nullptr;
    double              wrapScore    = std::numeric_limits<double>::infinity();

    for (auto& n : m_lMasterNodesData) {
        const auto TARGET = n->pTarget.lock();
        if (!TARGET || TARGET == t || !validMapped(TARGET->window()) || TARGET->window()->m_workspace != t->window()->m_workspace)
            continue;

        const Vector2D CENTER = n->position + n->size / 2.F;
        double         primaryDelta = 0;
        double         crossDelta   = 0;
        double         wrapPrimary  = 0;

        switch (dir) {
            case Math::DIRECTION_UP:
                primaryDelta = CURRENTCENTER.y - CENTER.y;
                crossDelta   = CURRENTCENTER.x - CENTER.x;
                wrapPrimary  = -CENTER.y;
                break;
            case Math::DIRECTION_RIGHT:
                primaryDelta = CENTER.x - CURRENTCENTER.x;
                crossDelta   = CURRENTCENTER.y - CENTER.y;
                wrapPrimary  = CENTER.x;
                break;
            case Math::DIRECTION_DOWN:
                primaryDelta = CENTER.y - CURRENTCENTER.y;
                crossDelta   = CURRENTCENTER.x - CENTER.x;
                wrapPrimary  = CENTER.y;
                break;
            case Math::DIRECTION_LEFT:
                primaryDelta = CURRENTCENTER.x - CENTER.x;
                crossDelta   = CURRENTCENTER.y - CENTER.y;
                wrapPrimary  = -CENTER.x;
                break;
            default: return nullptr;
        }

        const double crossScore = crossDelta * crossDelta;
        if (primaryDelta > 0) {
            const double score = primaryDelta * primaryDelta + crossScore;
            if (score < directScore) {
                directTarget = n;
                directScore  = score;
            }
        }

        const double score = wrapPrimary * 1000000.0 + crossScore;
        if (score < wrapScore) {
            wrapTarget = n;
            wrapScore  = score;
        }
    }

    const auto TARGET = directTarget ? directTarget : wrapTarget;
    return TARGET ? TARGET->pTarget.lock() : nullptr;
}

void CHyprNstackAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {
    static auto PMONITORFALLBACK = CConfigValue<Hyprlang::INT>("binds:window_direction_monitor_fallback");

    const auto  PWINDOW2 = g_pCompositor->getWindowInDirection(t->window(), dir);

    if (!t->window())
        return;

    PHLWORKSPACE targetWs;

    if (!PWINDOW2 && t->space() && t->space()->workspace()) {
        // try to find a monitor in dir
        const auto PMONINDIR = g_pCompositor->getMonitorInDirection(t->space()->workspace()->m_monitor.lock(), dir);
        if (PMONINDIR)
            targetWs = PMONINDIR->m_activeWorkspace;
    } else
        targetWs = PWINDOW2->m_workspace;

    if (!targetWs)
        return;

    t->window()->setAnimationsToMove();

    if (t->window()->m_workspace != targetWs) {
        if (!*PMONITORFALLBACK)
            return; // noop

        t->assignToSpace(targetWs->m_space, focalPointForDir(t, dir));
    } else if (PWINDOW2) {
        // if same monitor, switch windows
        PWINDOW2->setAnimationsToMove();
        g_layoutManager->switchTargets(t, PWINDOW2->layoutTarget());
        if (silent)
            Desktop::focusState()->fullWindowFocus(PWINDOW2, Desktop::FOCUS_REASON_KEYBIND);

        recalculate();
    }
}

Config::ErrorResult CHyprNstackAlgorithm::layoutMsg(const std::string_view& sv) {

    auto switchToWindow = [&](SP<ITarget> target) {
        if (!target || !validMapped(target->window()))
            return;

        Desktop::focusState()->fullWindowFocus(target->window(), Desktop::FOCUS_REASON_KEYBIND);
        g_pCompositor->warpCursorTo(target->position().middle());

        g_pInputManager->m_forcedFocus = target->window(); 
        g_pInputManager->simulateMouseMovement();
        g_pInputManager->m_forcedFocus.reset();
    };

    auto swapWithWindow = [&](PHLWINDOW window, SP<ITarget> target) {
        if (!window || !target || !validMapped(target->window()))
            return;

        g_pCompositor->setWindowFullscreenInternal(window, FSMODE_NONE);
        window->setAnimationsToMove();
        target->window()->setAnimationsToMove();
        g_layoutManager->switchTargets(window->layoutTarget(), target);
        switchToWindow(window->layoutTarget());
    };

    Hyprutils::String::CVarList2 vars(std::string{sv}, 0, 's');

    if (vars.size() < 1 || vars[0].empty()) {
        Log::logger->log(Log::ERR, "layoutmsg called without params");
        return Config::configError("layoutmsg without params"); 
    }

    auto command = vars[0];

    // swapwithmaster <master | child | auto>
    // first message argument can have the following values:
    // * master - keep the focus at the new master
    // * child - keep the focus at the new child
    // * auto (default) - swap the focus (keep the focus of the previously selected window)
	
	  const auto PWINDOW = Desktop::focusState()->window();
    if (command == "swapwithmaster") {

        if (!PWINDOW)
            return Config::configError("No focused window");

        if (!isWindowTiled(PWINDOW))
            return Config::configError("focused window isn't tiled");

        const auto PMASTER = getMasterNode();

        if (!PMASTER)
            return Config::configError("no master node");

        const auto NEWCHILD = PMASTER->pTarget.lock();

        if (PMASTER->pTarget.lock() != PWINDOW->layoutTarget()) {
            const auto NEWMASTER       = PWINDOW->layoutTarget();
            const bool newFocusToChild = vars.size() >= 2 && vars[1] == "child";
            g_layoutManager->switchTargets(NEWMASTER, NEWCHILD);
            const auto NEWFOCUS = newFocusToChild ? NEWCHILD : NEWMASTER;
            switchToWindow(NEWFOCUS);
        } else {
            for (auto& n : m_lMasterNodesData) {
                if (!n->isMaster) {
                    const auto NEWMASTER = n->pTarget.lock();
                    g_layoutManager->switchTargets(NEWMASTER, NEWCHILD);
                    const bool newFocusToMaster = vars.size() >= 2 && vars[1] == "master";
                    const auto NEWFOCUS         = newFocusToMaster ? NEWMASTER : NEWCHILD;
                    switchToWindow(NEWFOCUS);
                    break;
                }
            }
        }

        return {};
    }
    // focusmaster <master | auto>
    // first message argument can have the following values:
    // * master - keep the focus at the new master, even if it was focused before
    // * auto (default) - swap the focus with the first child, if the current focus was master, otherwise focus master
    else if (command == "focusmaster") {

        if (!PWINDOW)
            return Config::configError("no focused window");

        const auto PMASTER = getMasterNode();

        if (!PMASTER)
            return Config::configError("no master");

        if (PMASTER->pTarget.lock() != PWINDOW->layoutTarget()) {
            switchToWindow(PMASTER->pTarget.lock());
        } else if (vars.size() >= 2 && vars[1] == "master") {
            return {};
        } else {
            // if master is focused keep master focused (don't do anything)
            for (auto& n : m_lMasterNodesData) {
                if (!n->isMaster) {
                    switchToWindow(n->pTarget.lock());
                    break;
                }
            }
        }

        return {};
    } else if (command == "cyclenext") {

        if (!PWINDOW)
            return Config::configError("no window");

		    const bool NOLOOP = vars.size() >= 2 && vars[1] == "noloop";
        const auto PNEXTWINDOW = getNextTarget(PWINDOW->layoutTarget(), true, !NOLOOP);
        switchToWindow(PNEXTWINDOW);
    } else if (command == "cycleprev") {

        if (!PWINDOW)
            return Config::configError("no window");

		    const bool NOLOOP = vars.size() >= 2 && vars[1] == "noloop";
        const auto PPREVWINDOW = getNextTarget(PWINDOW->layoutTarget(), false, !NOLOOP);
        switchToWindow(PPREVWINDOW);
    } else if (command == "swapnext") {
        if (!validMapped(PWINDOW))
            return Config::configError("no window");

        if (PWINDOW->layoutTarget()->floating()) {
            g_pKeybindManager->m_dispatchers["swapnext"]("");
            return {};
        }

		    const bool NOLOOP = vars.size() >= 2 && vars[1] == "noloop";
        const auto PWINDOWTOSWAPWITH = getNextTarget(PWINDOW->layoutTarget(), true, !NOLOOP);

        if (PWINDOWTOSWAPWITH) {
            swapWithWindow(PWINDOW, PWINDOWTOSWAPWITH);
        }
    } else if (command == "swapprev") {
        if (!validMapped(PWINDOW))
            return Config::configError("no window");

        if (PWINDOW->layoutTarget()->floating()) {
            g_pKeybindManager->m_dispatchers["swapprev"]("");
            return {};
        }

		    const bool NOLOOP = vars.size() >= 2 && vars[1] == "noloop";
        const auto PWINDOWTOSWAPWITH = getNextTarget(PWINDOW->layoutTarget(), false, !NOLOOP);

        if (PWINDOWTOSWAPWITH) {
            swapWithWindow(PWINDOW, PWINDOWTOSWAPWITH);
        }
    } else if (command == "swapdirection") {
        if (!validMapped(PWINDOW))
            return Config::configError("no window");

        if (vars.size() < 2 || vars[1].empty())
            return Config::configError("missing direction");

        if (PWINDOW->layoutTarget()->floating()) {
            g_pKeybindManager->m_dispatchers["swapwindow"](std::string{vars[1]});
            return {};
        }

        const bool NOLOOP             = vars.size() >= 3 && vars[2] == "noloop";
        const auto DIR                = Math::fromChar(vars[1][0]);
        if (DIR == Math::DIRECTION_DEFAULT)
            return Config::configError("bad direction");

        const auto PWINDOWTOSWAPWITH = getDirectionalTarget(PWINDOW->layoutTarget(), DIR, !NOLOOP);
        if (PWINDOWTOSWAPWITH) {
            swapWithWindow(PWINDOW, PWINDOWTOSWAPWITH);
        }
    } else if (command == "addmaster") {
        if (!validMapped(PWINDOW))
            return Config::configError("no window");

        if (PWINDOW->layoutTarget()->floating())
            return Config::configError("window is floating");

        const auto PNODE = getNodeFromTarget(PWINDOW->layoutTarget());

        if (!PNODE || PNODE->isMaster) {
            // first non-master node
            for (auto& n : m_lMasterNodesData) {
                if (!n->isMaster) {
                    n->isMaster = true;
                    break;
                }
            }
        } else {
            PNODE->isMaster = true;
        }

        calculateWorkspace(); 

    } else if (command == "removemaster") {

        if (!validMapped(PWINDOW))
            return Config::configError("no window");

        if (PWINDOW->layoutTarget()->floating())
            return Config::configError("window isn't tiled");

        const auto PNODE = getNodeFromTarget(PWINDOW->layoutTarget());

        const auto WINDOWS = getNodesNo();
        const auto MASTERS = getMastersCount();

        if (WINDOWS < 2 || MASTERS < 2)
            return Config::configError("nothing to do");

        if (!PNODE || !PNODE->isMaster) {
            // first non-master node
            for (auto &nd : m_lMasterNodesData | std::views::reverse) {
                if (nd->isMaster) {
                    nd->isMaster = false;
                    break;
                }
            }
        } else {
            PNODE->isMaster = false;
        }
        calculateWorkspace();

    } else if (command == "orientationleft" || command == "orientationright" || command == "orientationtop" || command == "orientationbottom" || command == "orientationcenter" ||
               command == "orientationhcenter" || command == "orientationvcenter") {

        if (!PWINDOW)
            return Config::configError("no window");


		    g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);
        if (command == "orientationleft")
            m_userWorkspaceData.orientation = NSTACK_ORIENTATION_LEFT;
        else if (command == "orientationright")
            m_userWorkspaceData.orientation = NSTACK_ORIENTATION_RIGHT;
        else if (command == "orientationtop")
            m_userWorkspaceData.orientation = NSTACK_ORIENTATION_TOP;
        else if (command == "orientationbottom")
            m_userWorkspaceData.orientation = NSTACK_ORIENTATION_BOTTOM;
        else if (command == "orientationcenter" || command == "orientationhcenter")
            m_userWorkspaceData.orientation = NSTACK_ORIENTATION_HCENTER;
        else if (command == "orientationvcenter")
            m_userWorkspaceData.orientation = NSTACK_ORIENTATION_VCENTER;

        calculateWorkspace();

    } else if (command == "orientationnext") {
        runOrientationCycle(nullptr, 1);
    } else if (command == "orientationprev") {
        runOrientationCycle(nullptr, -1);
    } else if (command == "orientationcycle") {
        runOrientationCycle(&vars, 1);
    } else if (command == "resetsplits") {
        if (!PWINDOW)
            return Config::configError("no window");
        resetNodeSplits();
  	} else if (command == "mfact") {
		   const bool exact = vars[1] == "exact";
			 float ratio = 0.F;
		   try {
         ratio = std::stof(std::string{exact ? vars[2] : vars[1]});
   		} catch (...) { return Config::configError("bad ratio");}
		  float newRatio = exact ? ratio : m_userWorkspaceData.master_factor.value_or(m_workspaceData.master_factor) + ratio;
		  m_userWorkspaceData.master_factor = std::clamp(newRatio, 0.05f, 0.95f);
		  recalculate();
    } else if (command == "setstackcount") {
        if (!PWINDOW)
            return Config::configError("no window");
        if (vars.size() >= 2) {
            int newStackCount = 2;
            switch (vars[1][0]) {
                case '+':
				case '-': newStackCount = m_userWorkspaceData.m_iStackCount.value_or(m_workspaceData.m_iStackCount) + std::stoi(std::string(vars[1])); break;
				default: newStackCount = std::stoi(std::string(vars[1])); break;
            }
            if (newStackCount) {
                if (newStackCount < 2)
                    newStackCount = 2;
                m_userWorkspaceData.m_iStackCount = newStackCount;
                calculateWorkspace();
            }
        }
    }

    return {};
}

// If vars is null, we use the default list
void CHyprNstackAlgorithm::runOrientationCycle(Hyprutils::String::CVarList2* vars, int direction) {
	  eColOrientation current_orientation;
    std::vector<eColOrientation> cycle;
    if (vars != nullptr)
        buildOrientationCycleVectorFromVars(cycle, vars);

    if (cycle.size() == 0)
        buildOrientationCycleVectorFromEOperation(cycle);

    const auto PWINDOW = Desktop::focusState()->window(); 

    if (!PWINDOW)
        return;


	  g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

	  current_orientation = m_userWorkspaceData.orientation.value_or(m_workspaceData.orientation);
    int        nextOrPrev = 0;
    for (size_t i = 0; i < cycle.size(); ++i) {
        if (current_orientation == cycle.at(i)) {
            nextOrPrev = i + direction;
            break;
        }
    }

    if (nextOrPrev >= (int)cycle.size())
        nextOrPrev = nextOrPrev % (int)cycle.size();
    else if (nextOrPrev < 0)
        nextOrPrev = cycle.size() + (nextOrPrev % (int)cycle.size());

    m_userWorkspaceData.orientation = cycle.at(nextOrPrev);
    calculateWorkspace();
}

void CHyprNstackAlgorithm::buildOrientationCycleVectorFromEOperation(std::vector<eColOrientation>& cycle) {
    for (int i = 0; i <= NSTACK_ORIENTATION_VCENTER; ++i) {
        cycle.push_back((eColOrientation)i);
    }
}

void CHyprNstackAlgorithm::buildOrientationCycleVectorFromVars(std::vector<eColOrientation>& cycle, Hyprutils::String::CVarList2* vars) {
    for (size_t i = 1; i < vars->size(); ++i) {
        if ((*vars)[i] == "top") {
            cycle.push_back(NSTACK_ORIENTATION_TOP);
        } else if ((*vars)[i] == "right") {
            cycle.push_back(NSTACK_ORIENTATION_RIGHT);
        } else if ((*vars)[i] == "bottom") {
            cycle.push_back(NSTACK_ORIENTATION_BOTTOM);
        } else if ((*vars)[i] == "left") {
            cycle.push_back(NSTACK_ORIENTATION_LEFT);
        } else if ((*vars)[i] == "hcenter") {
            cycle.push_back(NSTACK_ORIENTATION_HCENTER);
        } else if ((*vars)[i] == "vcenter") {
            cycle.push_back(NSTACK_ORIENTATION_VCENTER);
        }
    }
}
