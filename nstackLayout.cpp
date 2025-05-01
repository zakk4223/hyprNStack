#include "nstackLayout.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/render/decorations/CHyprGroupBarDecoration.hpp>
#include <format>
#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>

SNstackNodeData* CHyprNstackLayout::getNodeFromWindow(PHLWINDOW pWindow) {
    for (auto& nd : m_lMasterNodesData) {
        if (nd.pWindow.lock() == pWindow)
            return &nd;
    }

    return nullptr;
}

int CHyprNstackLayout::getNodesOnWorkspace(const int& ws) {
    int no = 0;
    for (auto& n : m_lMasterNodesData) {
        if (n.workspaceID == ws)
            no++;
    }

    return no;
}

int CHyprNstackLayout::getMastersOnWorkspace(const int& ws) {
    int no = 0;
    for (auto& n : m_lMasterNodesData) {
        if (n.workspaceID == ws && n.isMaster)
            no++;
    }

    return no;
}

void CHyprNstackLayout::removeWorkspaceData(const int& ws) {

    SNstackWorkspaceData* wsdata = nullptr;
    for (auto& n : m_lMasterWorkspacesData) {
        if (n.workspaceID == ws)
            wsdata = &n;
    }

    if (wsdata)
        m_lMasterWorkspacesData.remove(*wsdata);
}

static void applyWorkspaceLayoutOptions(SNstackWorkspaceData* wsData) {

    const auto         wsrule       = g_pConfigManager->getWorkspaceRuleFor(g_pCompositor->getWorkspaceByID(wsData->workspaceID));
    const auto         wslayoutopts = wsrule.layoutopts;

    static auto* const orientation   = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:orientation")->getDataStaticPtr();
    std::string        wsorientation = *orientation;

    if (wslayoutopts.contains("nstack-orientation"))
        wsorientation = wslayoutopts.at("nstack-orientation");

    std::string cpporientation = wsorientation;
    //create on the fly if it doesn't exist yet
    if (cpporientation == "top") {
        wsData->orientation = NSTACK_ORIENTATION_TOP;
    } else if (cpporientation == "right") {
        wsData->orientation = NSTACK_ORIENTATION_RIGHT;
    } else if (cpporientation == "bottom") {
        wsData->orientation = NSTACK_ORIENTATION_BOTTOM;
    } else if (cpporientation == "left") {
        wsData->orientation = NSTACK_ORIENTATION_LEFT;
    } else if (cpporientation == "vcenter") {
        wsData->orientation = NSTACK_ORIENTATION_VCENTER;
    } else {
        wsData->orientation = NSTACK_ORIENTATION_HCENTER;
    }

    static auto* const NUMSTACKS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:stacks")->getDataStaticPtr();
    auto               wsstacks  = **NUMSTACKS;

    if (wslayoutopts.contains("nstack-stacks")) {
        try {
            std::string stackstr = wslayoutopts.at("nstack-stacks");
            wsstacks             = std::stol(stackstr);
        } catch (std::exception& e) { Debug::log(ERR, "Nstack layoutopt invalid rule value for nstack-stacks: {}", e.what()); }
    }
    if (wsstacks) {
        wsData->m_iStackCount = wsstacks;
    }

    static auto* const MFACT   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:mfact")->getDataStaticPtr();
    auto               wsmfact = **MFACT;
    if (wslayoutopts.contains("nstack-mfact")) {
        std::string mfactstr = wslayoutopts.at("nstack-mfact");
        try {
            wsmfact = std::stof(mfactstr);
        } catch (std::exception& e) { Debug::log(ERR, "Nstack layoutopt nstack-mfact format error: {}", e.what()); }
    }
    wsData->master_factor = wsmfact;

    static auto* const SMFACT   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:single_mfact")->getDataStaticPtr();
    auto               wssmfact = **SMFACT;

    if (wslayoutopts.contains("nstack-single_mfact")) {
        std::string smfactstr = wslayoutopts.at("nstack-single_mfact");
        try {
            wssmfact = std::stof(smfactstr);
        } catch (std::exception& e) { Debug::log(ERR, "Nstack layoutopt nstack-single_mfact format error: {}", e.what()); }
    }
    wsData->single_master_factor = wssmfact;

    static auto* const SSFACT   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:special_scale_factor")->getDataStaticPtr();
    auto               wsssfact = **SSFACT;
    if (wslayoutopts.contains("nstack-special_scale_factor")) {
        std::string ssfactstr = wslayoutopts.at("nstack-special_scale_factor");
        try {
            wsssfact = std::stof(ssfactstr);
        } catch (std::exception& e) { Debug::log(ERR, "Nstack layoutopt nstack-special_scale_factor format error: {}", e.what()); }
    }
    wsData->special_scale_factor = wsssfact;

    static auto* const NEWTOP   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:new_on_top")->getDataStaticPtr();
    auto               wsnewtop = **NEWTOP;
    if (wslayoutopts.contains("nstack-new_on_top"))
        wsnewtop = configStringToInt(wslayoutopts.at("nstack-new_on_top")).value_or(0);
    wsData->new_on_top = wsnewtop;

    static auto* const NEWMASTER   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:new_is_master")->getDataStaticPtr();
    auto               wsnewmaster = **NEWMASTER;
    if (wslayoutopts.contains("nstack-new_is_master"))
        wsnewmaster = configStringToInt(wslayoutopts.at("nstack-new_is_master")).value_or(0);
    wsData->new_is_master = wsnewmaster;

    static auto* const NGWO   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:no_gaps_when_only")->getDataStaticPtr();
    auto               wsngwo = **NGWO;
    if (wslayoutopts.contains("nstack-no_gaps_when_only"))
        wsngwo = configStringToInt(wslayoutopts.at("nstack-no_gaps_when_only")).value_or(0);
    wsData->no_gaps_when_only = wsngwo;

    static auto* const INHERITFS   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:inherit_fullscreen")->getDataStaticPtr();
    auto               wsinheritfs = **INHERITFS;
    if (wslayoutopts.contains("nstack-inherit_fullscreen"))
        wsinheritfs = configStringToInt(wslayoutopts.at("nstack-inherit_fullscreen")).value_or(0);
    wsData->inherit_fullscreen = wsinheritfs;

    static auto* const CENTERSM   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:center_single_master")->getDataStaticPtr();
    auto               wscentersm = **CENTERSM;
    if (wslayoutopts.contains("nstack-center_single_master"))
        wscentersm = configStringToInt(wslayoutopts.at("nstack-center_single_master")).value_or(0);
    wsData->center_single_master = wscentersm;
}

SNstackWorkspaceData* CHyprNstackLayout::getMasterWorkspaceData(const int& ws) {
    SNstackWorkspaceData* retData = nullptr;
    for (auto& n : m_lMasterWorkspacesData) {
        if (n.workspaceID == ws) {
            retData = &n;
            break;
        }
    }
    const auto PWORKSPACE   = g_pCompositor->getWorkspaceByID(ws);
    const auto wsrule       = g_pConfigManager->getWorkspaceRuleFor(PWORKSPACE);
    const auto wslayoutopts = wsrule.layoutopts;

    if (retData == nullptr) {
        retData              = &m_lMasterWorkspacesData.emplace_back();
        retData->workspaceID = ws;
    }
    applyWorkspaceLayoutOptions(retData);
    return retData;
}

std::string CHyprNstackLayout::getLayoutName() {
    return "Nstack";
}

SNstackNodeData* CHyprNstackLayout::getMasterNodeOnWorkspace(const int& ws) {
    for (auto& n : m_lMasterNodesData) {
        if (n.isMaster && n.workspaceID == ws)
            return &n;
    }

    return nullptr;
}

void CHyprNstackLayout::resetNodeSplits(const int& ws) {

    removeWorkspaceData(ws);
    const auto PMONITOR = g_pCompositor->getWorkspaceByID(ws)->m_pMonitor.lock();
    recalculateMonitor(PMONITOR->ID);
}

void CHyprNstackLayout::onWindowCreatedTiling(PHLWINDOW pWindow, eDirection direction) {
    if (pWindow->m_bIsFloating)
        return;

    const auto WSID = pWindow->workspaceID();

    const auto WORKSPACEDATA = getMasterWorkspaceData(WSID);

    const auto PMONITOR = pWindow->m_pMonitor.lock();

    const auto PNODE = WORKSPACEDATA->new_on_top ? &m_lMasterNodesData.emplace_front() : &m_lMasterNodesData.emplace_back();

    PNODE->workspaceID = pWindow->workspaceID();
    PNODE->pWindow     = pWindow;

    auto       OPENINGON = isWindowTiled(g_pCompositor->m_pLastWindow.lock()) && g_pCompositor->m_pLastWindow.lock()->m_pWorkspace == pWindow->m_pWorkspace ?
              getNodeFromWindow(g_pCompositor->m_pLastWindow.lock()) :
              getMasterNodeOnWorkspace(pWindow->workspaceID());

    const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();

    const auto WINDOWSONWORKSPACE = getNodesOnWorkspace(PNODE->workspaceID);
    float      lastSplitPercent   = 0.5f;
    bool       lastMasterAdjusted = false;

    if (g_pInputManager->m_bWasDraggingWindow && OPENINGON) {
        if (OPENINGON->pWindow.lock()->checkInputOnDecos(INPUT_TYPE_DRAG_END, MOUSECOORDS, pWindow))
            return;
    }

    bool newWindowIsMaster = false;
    if (WORKSPACEDATA->new_is_master || WINDOWSONWORKSPACE == 1 || (!pWindow->m_bFirstMap && OPENINGON->isMaster))
        newWindowIsMaster = true;
    if (newWindowIsMaster) {
        for (auto& nd : m_lMasterNodesData) {
            if (nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                nd.isMaster        = false;
                lastSplitPercent   = nd.percMaster;
                lastMasterAdjusted = nd.masterAdjusted;
                break;
            }
        }

        PNODE->isMaster       = true;
        PNODE->percMaster     = lastSplitPercent;
        PNODE->masterAdjusted = lastMasterAdjusted;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = pWindow->requestedMaxSize(); MAXSIZE.x < PMONITOR->vecSize.x * lastSplitPercent || MAXSIZE.y < PMONITOR->vecSize.y) {
            // we can't continue. make it floating.
            pWindow->m_bIsFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    } else {
        PNODE->isMaster = false;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = pWindow->requestedMaxSize();
            MAXSIZE.x < PMONITOR->vecSize.x * (1 - lastSplitPercent) || MAXSIZE.y < PMONITOR->vecSize.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            // we can't continue. make it floating.
            pWindow->m_bIsFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    }

    // recalc
    recalculateMonitor(pWindow->monitorID());
}

void CHyprNstackLayout::onWindowRemovedTiling(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    pWindow->unsetWindowData(PRIORITY_LAYOUT);
    pWindow->updateWindowData();

    if (pWindow->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);

    const auto MASTERSLEFT = getMastersOnWorkspace(PNODE->workspaceID);

    if (PNODE->isMaster && MASTERSLEFT < 2) {
        // find new one
        for (auto& nd : m_lMasterNodesData) {
            if (!nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                nd.isMaster       = true;
                nd.percMaster     = PNODE->percMaster;
                nd.masterAdjusted = PNODE->masterAdjusted;
                break;
            }
        }
    }

    const auto WORKSPACEID = PNODE->workspaceID;

    m_lMasterNodesData.remove(*PNODE);

    if (getMastersOnWorkspace(WORKSPACEID) == getNodesOnWorkspace(WORKSPACEID) && MASTERSLEFT > 1) {
        for (auto it = m_lMasterNodesData.rbegin(); it != m_lMasterNodesData.rend(); it++) {
            if (it->workspaceID == WORKSPACEID) {
                it->isMaster = false;
                break;
            }
        }
    }

    recalculateMonitor(pWindow->monitorID());
}

void CHyprNstackLayout::recalculateMonitor(const MONITORID& monid) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);
    if (!PMONITOR || !PMONITOR->activeWorkspace)
        return;

    const auto PWORKSPACE = PMONITOR->activeWorkspace;

    if (!PWORKSPACE)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->activeSpecialWorkspace) {
        calculateWorkspace(PMONITOR->activeSpecialWorkspace);
    }
    calculateWorkspace(PWORKSPACE);
}

void CHyprNstackLayout::calculateWorkspace(PHLWORKSPACE PWORKSPACE) {
    if (!PWORKSPACE)
        return;

    const auto PMONITOR = PWORKSPACE->m_pMonitor.lock();

    if (!PMONITOR)
        return;

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        // massive hack from the fullscreen func
        const auto PFULLWINDOW = PWORKSPACE->getFullscreenWindow();

        if (PWORKSPACE->m_efFullscreenMode == FSMODE_FULLSCREEN) {
            *PFULLWINDOW->m_vRealPosition = PMONITOR->vecPosition;
            *PFULLWINDOW->m_vRealSize     = PMONITOR->vecSize;
        } else if (PWORKSPACE->m_efFullscreenMode == FSMODE_MAXIMIZED) {

            SNstackNodeData fakeNode;
            fakeNode.pWindow                = PFULLWINDOW;
            fakeNode.position               = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
            fakeNode.size                   = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
            fakeNode.workspaceID            = PWORKSPACE->m_iID;
            PFULLWINDOW->m_vPosition         = fakeNode.position;
            PFULLWINDOW->m_vSize             = fakeNode.size;
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }

        // if has fullscreen, don't calculate the rest
        return;
    }
    const auto      PWORKSPACEDATA = getMasterWorkspaceData(PWORKSPACE->m_iID);
    auto            NUMSTACKS      = PWORKSPACEDATA->m_iStackCount;

    const auto      PMASTERNODE = getMasterNodeOnWorkspace(PWORKSPACE->m_iID);
    const auto      NODECOUNT   = getNodesOnWorkspace(PWORKSPACE->m_iID);

    eColOrientation orientation = PWORKSPACEDATA->orientation;

    if (!PMASTERNODE)
        return;

    const auto MASTERS     = getMastersOnWorkspace(PWORKSPACE->m_iID);
    const auto ONLYMASTERS = !(NODECOUNT - MASTERS);

    if (NUMSTACKS < 3 && orientation > NSTACK_ORIENTATION_BOTTOM) {
        NUMSTACKS = 3;
    }

    if (!PMASTERNODE->masterAdjusted) {
        if (getNodesOnWorkspace(PWORKSPACE->m_iID) < NUMSTACKS) {
            PMASTERNODE->percMaster = PWORKSPACEDATA->master_factor ? PWORKSPACEDATA->master_factor : 1.0f / getNodesOnWorkspace(PWORKSPACE->m_iID);
        } else {
            PMASTERNODE->percMaster = PWORKSPACEDATA->master_factor ? PWORKSPACEDATA->master_factor : 1.0f / (NUMSTACKS);
        }
    }
    bool centerMasterWindow = false;
    if (PWORKSPACEDATA->center_single_master)
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

            if (!PMASTERNODE->masterAdjusted)
                PMASTERNODE->percMaster = PWORKSPACEDATA->single_master_factor ? PWORKSPACEDATA->single_master_factor : 0.5f;

            if (orientation == NSTACK_ORIENTATION_TOP || orientation == NSTACK_ORIENTATION_BOTTOM) {
                const float HEIGHT        = (PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y) * PMASTERNODE->percMaster;
                float       CENTER_OFFSET = (PMONITOR->vecSize.y - HEIGHT) / 2;
                MCONTAINERSIZE            = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x, HEIGHT);
                MCONTAINERPOS             = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(0.0, CENTER_OFFSET);
            } else {
                const float WIDTH         = (PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x) * PMASTERNODE->percMaster;
                float       CENTER_OFFSET = (PMONITOR->vecSize.x - WIDTH) / 2;
                MCONTAINERSIZE            = Vector2D(WIDTH, PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y);
                MCONTAINERPOS             = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(CENTER_OFFSET, 0.0);
            }
        } else {
            MCONTAINERPOS  = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition;
            MCONTAINERSIZE = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x,
                                      PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y);
        }
    } else {
        const float MASTERSIZE = orientation % 2 == 0 ? (PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x) * PMASTERNODE->percMaster :
                                                        (PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y) * PMASTERNODE->percMaster;

        if (orientation == NSTACK_ORIENTATION_RIGHT) {
            MCONTAINERPOS = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition +
                Vector2D(PMONITOR->vecSize.x - MASTERSIZE - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x, 0.0f);
            MCONTAINERSIZE = Vector2D(MASTERSIZE, PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y);
        } else if (orientation == NSTACK_ORIENTATION_LEFT) {
            MCONTAINERPOS  = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition;
            MCONTAINERSIZE = Vector2D(MASTERSIZE, PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y);
        } else if (orientation == NSTACK_ORIENTATION_TOP) {
            MCONTAINERPOS  = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition;
            MCONTAINERSIZE = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x, MASTERSIZE);
        } else if (orientation == NSTACK_ORIENTATION_BOTTOM) {
            MCONTAINERPOS = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition +
                Vector2D(0.0f, PMONITOR->vecSize.y - MASTERSIZE - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y);
            MCONTAINERSIZE = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x, MASTERSIZE);

        } else if (orientation == NSTACK_ORIENTATION_HCENTER) {
            float CENTER_OFFSET = (PMONITOR->vecSize.x - MASTERSIZE) / 2;
            MCONTAINERSIZE      = Vector2D(MASTERSIZE, PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y);
            MCONTAINERPOS       = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(CENTER_OFFSET, 0.0);
        } else if (orientation == NSTACK_ORIENTATION_VCENTER) {
            float CENTER_OFFSET = (PMONITOR->vecSize.y - MASTERSIZE) / 2;
            MCONTAINERSIZE      = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x, MASTERSIZE);
            MCONTAINERPOS       = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(0.0, CENTER_OFFSET);
        }
    }

    if (MCONTAINERSIZE != Vector2D(0, 0)) {
        float       nodeSpaceLeft = orientation % 2 == 0 ? MCONTAINERSIZE.y : MCONTAINERSIZE.x;
        int         nodesLeft     = MASTERS;
        float       nextNodeCoord = 0;
        const float MASTERSIZE    = orientation % 2 == 0 ? MCONTAINERSIZE.x : MCONTAINERSIZE.y;
        for (auto& n : m_lMasterNodesData) {
            if (n.workspaceID == PWORKSPACE->m_iID && n.isMaster) {
                if (orientation == NSTACK_ORIENTATION_RIGHT) {
                    n.position = MCONTAINERPOS + Vector2D(0.0f, nextNodeCoord);
                } else if (orientation == NSTACK_ORIENTATION_LEFT) {

                    n.position = MCONTAINERPOS + Vector2D(0.0, nextNodeCoord);
                } else if (orientation == NSTACK_ORIENTATION_TOP) {
                    n.position = MCONTAINERPOS + Vector2D(nextNodeCoord, 0.0);
                } else if (orientation == NSTACK_ORIENTATION_BOTTOM) {
                    n.position = MCONTAINERPOS + Vector2D(nextNodeCoord, 0.0);
                } else if (orientation == NSTACK_ORIENTATION_HCENTER) {
                    n.position = MCONTAINERPOS + Vector2D(0.0, nextNodeCoord);
                } else {
                    n.position = MCONTAINERPOS + Vector2D(nextNodeCoord, 0.0);
                }

                float NODESIZE = nodesLeft > 1 ? nodeSpaceLeft / nodesLeft * n.percSize : nodeSpaceLeft;
                if (NODESIZE > nodeSpaceLeft * 0.9f && nodesLeft > 1)
                    NODESIZE = nodeSpaceLeft * 0.9f;

                n.size = orientation % 2 == 0 ? Vector2D(MASTERSIZE, NODESIZE) : Vector2D(NODESIZE, MASTERSIZE);
                nodesLeft--;
                nodeSpaceLeft -= NODESIZE;
                nextNodeCoord += NODESIZE;
                applyNodeDataToWindow(&n);
            }
        }
    }

    //compute placement of slave window(s)
    int slavesLeft  = getNodesOnWorkspace(PWORKSPACE->m_iID) - MASTERS;
    int slavesTotal = slavesLeft;
    if (slavesTotal < 1)
        return;
    int numStacks      = slavesTotal > NUMSTACKS - 1 ? NUMSTACKS - 1 : slavesTotal;
    int numStackBefore = numStacks / 2 + numStacks % 2;
    int numStackAfter  = numStacks / 2;

    PWORKSPACEDATA->stackNodeCount.assign(numStacks + 1, 0);
    PWORKSPACEDATA->stackPercs.resize(numStacks + 1, 1.0f);

    float                 stackNodeSizeLeft = orientation % 2 == 1 ? PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x :
                                                                     PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y;
    int                   stackNum          = (slavesTotal - slavesLeft) % numStacks;
    std::vector<float>    nodeSpaceLeft(numStacks, stackNodeSizeLeft);
    std::vector<float>    nodeNextCoord(numStacks, 0);
    std::vector<Vector2D> stackCoords(numStacks, Vector2D(0, 0));

    const float STACKSIZE = orientation % 2 == 1 ? (PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y - PMASTERNODE->size.y) / numStacks :
                                                   (PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x - PMASTERNODE->size.x) / numStacks;

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
            coordAdjust = orientation % 2 == 1 ? PMASTERNODE->position.y + PMASTERNODE->size.y - PMONITOR->vecPosition.y - PMONITOR->vecReservedTopLeft.y :
                                                 PMASTERNODE->position.x + PMASTERNODE->size.x - PMONITOR->vecPosition.x - PMONITOR->vecReservedTopLeft.x;
        }
        float monMax     = orientation % 2 == 1 ? PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y :
                                                  PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x;
        float stackStart = 0.0f;
        if (i == numStackBefore && numStackAfter) {
            stackStart = coordAdjust;
        } else if (i) {
            stackStart = stackCoords[i - 1].y;
        }
        float scaledSize = useSize * PWORKSPACEDATA->stackPercs[i + 1];

        //Stacks at bottom and right always fill remaining space
        //Stacks that end adjacent to the master stack are pinned to it

        if (orientation == NSTACK_ORIENTATION_LEFT && i >= numStacks - 1) {
            scaledSize = monMax - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_RIGHT && i >= numStacks - 1) {
            scaledSize = (PMASTERNODE->position.x - PMONITOR->vecPosition.x - PMONITOR->vecReservedTopLeft.x) - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_TOP && i >= numStacks - 1) {
            scaledSize = monMax - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_BOTTOM && i >= numStacks - 1) {
            scaledSize = (PMASTERNODE->position.y - PMONITOR->vecPosition.y - PMONITOR->vecReservedTopLeft.y) - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_HCENTER) {
            if (i >= numStacks - 1) {
                scaledSize = monMax - stackStart;
            } else if (i == numStacks - 2) {
                scaledSize = (PMASTERNODE->position.x - PMONITOR->vecPosition.x - PMONITOR->vecReservedTopLeft.x) - stackStart;
            }
        } else if (orientation == NSTACK_ORIENTATION_VCENTER) {
            if (i >= numStacks - 1) {
                scaledSize = monMax - stackStart;
            } else if (i == numStacks - 2) {
                scaledSize = (PMASTERNODE->position.y - PMONITOR->vecPosition.y - PMONITOR->vecReservedTopLeft.y) - stackStart;
            }
        }
        stackCoords[i] = Vector2D(stackStart, stackStart + scaledSize);
    }

    for (auto& nd : m_lMasterNodesData) {
        if (nd.workspaceID != PWORKSPACE->m_iID || nd.isMaster)
            continue;

        Vector2D stackPos = stackCoords[stackNum];
        if (orientation % 2 == 0) {
            nd.position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(stackPos.x, nodeNextCoord[stackNum]);
        } else {
            nd.position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(nodeNextCoord[stackNum], stackPos.x);
        }

        int nodeDiv = slavesTotal / numStacks;
        if (slavesTotal % numStacks && stackNum < slavesTotal % numStacks)
            nodeDiv++;
        float NODESIZE = slavesLeft > numStacks ? (stackNodeSizeLeft / nodeDiv) * nd.percSize : nodeSpaceLeft[stackNum];
        if (NODESIZE > nodeSpaceLeft[stackNum] * 0.9f && slavesLeft > numStacks)
            NODESIZE = nodeSpaceLeft[stackNum] * 0.9f;
        nd.stackNum = stackNum + 1;
        nd.size     = orientation % 2 == 1 ? Vector2D(NODESIZE, stackPos.y - stackPos.x) : Vector2D(stackPos.y - stackPos.x, NODESIZE);
        PWORKSPACEDATA->stackNodeCount[nd.stackNum]++;
        slavesLeft--;
        nodeSpaceLeft[stackNum] -= NODESIZE;
        nodeNextCoord[stackNum] += NODESIZE;
        stackNum = (slavesTotal - slavesLeft) % numStacks;
        applyNodeDataToWindow(&nd);
    }
}

void CHyprNstackLayout::applyNodeDataToWindow(SNstackNodeData* pNode) {
    PHLMONITOR PMONITOR = nullptr;

    if (g_pCompositor->isWorkspaceSpecial(pNode->workspaceID)) {
        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->activeSpecialWorkspaceID() == pNode->workspaceID) {
                PMONITOR = m;
                break;
            }
        }
    } else {
        PMONITOR = g_pCompositor->getWorkspaceByID(pNode->workspaceID)->m_pMonitor.lock();
    }

    if (!PMONITOR) {
        Debug::log(ERR, "Orphaned Node {} (workspace ID: {})!!", static_cast<void*>(pNode), pNode->workspaceID);
        return;
    }

    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(pNode->position.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->position.x + pNode->size.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pNode->position.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->position.y + pNode->size.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    const auto PWINDOW        = pNode->pWindow.lock();
    const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->workspaceID());
    const auto WORKSPACERULE  = g_pConfigManager->getWorkspaceRuleFor(g_pCompositor->getWorkspaceByID(PWINDOW->workspaceID()));

    if (PWINDOW->isFullscreen() && !pNode->ignoreFullscreenChecks)
        return;

    PWINDOW->unsetWindowData(PRIORITY_LAYOUT);
    PWINDOW->updateWindowData();

    static auto* const PANIMATE = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("misc:animate_manual_resizes");

    static auto* const PGAPSINDATA  = (Hyprlang::CUSTOMTYPE* const*)g_pConfigManager->getConfigValuePtr("general:gaps_in");
    static auto* const PGAPSOUTDATA = (Hyprlang::CUSTOMTYPE* const*)g_pConfigManager->getConfigValuePtr("general:gaps_out");
    auto* const        PGAPSIN      = (CCssGapData*)(*PGAPSINDATA)->getData();
    auto* const        PGAPSOUT     = (CCssGapData*)(*PGAPSOUTDATA)->getData();

    auto               gapsIn  = WORKSPACERULE.gapsIn.value_or(*PGAPSIN);
    auto               gapsOut = WORKSPACERULE.gapsOut.value_or(*PGAPSOUT);

    if (!validMapped(PWINDOW)) {
        Debug::log(ERR, "Node {} holding invalid window {}!!", pNode, PWINDOW);
        return;
    }

    PWINDOW->m_vSize     = pNode->size;
    PWINDOW->m_vPosition = pNode->position;

    //auto calcPos  = PWINDOW->m_vPosition + Vector2D(*PBORDERSIZE, *PBORDERSIZE);
    //auto calcSize = PWINDOW->m_vSize - Vector2D(2 * *PBORDERSIZE, 2 * *PBORDERSIZE);

    const auto OFFSETTOPLEFT = Vector2D((double)(DISPLAYLEFT ? gapsOut.left : gapsIn.left), (double)(DISPLAYTOP ? gapsOut.top : gapsIn.top));

    const auto OFFSETBOTTOMRIGHT = Vector2D((double)(DISPLAYRIGHT ? gapsOut.right : gapsIn.right), (double)(DISPLAYBOTTOM ? gapsOut.bottom : gapsIn.bottom));

    PWINDOW->m_vPosition = PWINDOW->m_vPosition + OFFSETTOPLEFT;
    PWINDOW->m_vSize     = PWINDOW->m_vSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    PWINDOW->m_vPosition = PWINDOW->m_vPosition + RESERVED.topLeft;
    PWINDOW->m_vSize     = PWINDOW->m_vSize - (RESERVED.topLeft + RESERVED.bottomRight);

    if (g_pCompositor->isWorkspaceSpecial(PWINDOW->workspaceID())) {

        CBox wb = {PWINDOW->m_vPosition + (PWINDOW->m_vSize - PWINDOW->m_vSize * PWORKSPACEDATA->special_scale_factor) / 2.f, PWINDOW->m_vSize * PWORKSPACEDATA->special_scale_factor};
        wb.round(); // avoid rounding mess

        *PWINDOW->m_vRealPosition = wb.pos();
        *PWINDOW->m_vRealSize     = wb.size();

    } else {
        CBox wb = {PWINDOW->m_vPosition, PWINDOW->m_vSize};
        wb.round();
        *PWINDOW->m_vRealSize     = wb.size();
        *PWINDOW->m_vRealPosition = wb.pos();
    }

    if (m_bForceWarps && !**PANIMATE) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_vRealPosition->warp();
        PWINDOW->m_vRealSize->warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

bool CHyprNstackLayout::isWindowTiled(PHLWINDOW pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CHyprNstackLayout::resizeActiveWindow(const Vector2D& pixResize, eRectCorner corner, PHLWINDOW pWindow) {
    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_pLastWindow.lock();

    if (!validMapped(PWINDOW))
        return;

    const auto PNODE = getNodeFromWindow(PWINDOW);

    if (!PNODE) {
        *PWINDOW->m_vRealSize = Vector2D(std::max((PWINDOW->m_vRealSize->goal() + pixResize).x, 20.0), std::max((PWINDOW->m_vRealSize->goal() + pixResize).y, 20.0));
        PWINDOW->updateWindowDecos();
        return;
    }

    // get monitor
    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->monitorID());

    m_bForceWarps = true;

    const auto PMASTERNODE    = getMasterNodeOnWorkspace(PWINDOW->workspaceID());
    const auto PWORKSPACEDATA = getMasterWorkspaceData(PMONITOR->activeWorkspaceID());
    bool       xResizeDone    = false;
    bool       yResizeDone    = false;

    bool       masterAdjacent = false;

    /*
    if ((PWORKSPACEDATA->orientation == NSTACK_ORIENTATION_LEFT || PWORKSPACEDATA->orientation == NSTACK_ORIENTATION_TOP)  && PNODE->stackNum == 1) {
	    masterAdjacent = true;
    } else if ((PWORKSPACEDATA->orientation == NSTACK_ORIENTATION_RIGHT || PWORKSPACEDATA->orientation == NSTACK_ORIENTATION_BOTTOM) && PNODE->stackNum == PWORKSPACEDATA->stackNodeCount.size()-1) {
	    masterAdjacent = true;
    } else if ((PWORKSPACEDATA->orientation == NSTACK_ORIENTATION_HCENTER || PWORKSPACEDATA->orientation == NSTACK_ORIENTATION_VCENTER) && (PNODE->stackNum >= PWORKSPACEDATA->stackNodeCount.size()-2)) {
	    masterAdjacent = true;
    }*/

    if (PNODE->isMaster || masterAdjacent) {
        double delta = 0;

        switch (PWORKSPACEDATA->orientation) {
            case NSTACK_ORIENTATION_LEFT:
                delta       = pixResize.x / PMONITOR->vecSize.x;
                xResizeDone = true;
                break;
            case NSTACK_ORIENTATION_RIGHT:
                delta       = -pixResize.x / PMONITOR->vecSize.x;
                xResizeDone = true;
                break;
            case NSTACK_ORIENTATION_BOTTOM:
                delta       = -pixResize.y / PMONITOR->vecSize.y;
                yResizeDone = true;
                break;
            case NSTACK_ORIENTATION_TOP:
                delta       = pixResize.y / PMONITOR->vecSize.y;
                yResizeDone = true;
                break;
            case NSTACK_ORIENTATION_HCENTER:
                delta       = pixResize.x / PMONITOR->vecSize.x;
                xResizeDone = true;
                break;
            case NSTACK_ORIENTATION_VCENTER:
                delta       = pixResize.y / PMONITOR->vecSize.y;
                yResizeDone = true;
                break;
            default: UNREACHABLE();
        }

        const auto workspaceIdForResizing = PMONITOR->activeSpecialWorkspace ? PMONITOR->activeSpecialWorkspaceID() : PMONITOR->activeWorkspaceID();

        for (auto& n : m_lMasterNodesData) {
            if (n.isMaster && n.workspaceID == workspaceIdForResizing) {
                n.percMaster     = std::clamp(n.percMaster + delta, 0.05, 0.95);
                n.masterAdjusted = true;
            }
        }
    }

    if (pixResize.x != 0 && !xResizeDone) {
        //Only handle master 'in stack' resizing. Master column resizing is handled above
        if (PNODE->isMaster && getMastersOnWorkspace(PNODE->workspaceID) > 1 && PWORKSPACEDATA->orientation % 2 == 1) {
            const auto SIZE = (PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x) / getMastersOnWorkspace(PNODE->workspaceID);
            PNODE->percSize = std::clamp(PNODE->percSize + pixResize.x / SIZE, 0.05, 1.95);
        } else if (!PNODE->isMaster && (getNodesOnWorkspace(PWINDOW->workspaceID()) - getMastersOnWorkspace(PNODE->workspaceID)) > 1) {
            if (PWORKSPACEDATA->orientation % 2 == 1) { //In stack resize

                const auto SIZE = (PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x) / PWORKSPACEDATA->stackNodeCount[PNODE->stackNum];
                PNODE->percSize = std::clamp(PNODE->percSize + pixResize.x / SIZE, 0.05, 1.95);
            } else {
                const auto SIZE =
                    (PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x - PMASTERNODE->size.x) / PWORKSPACEDATA->stackNodeCount.size();
                PWORKSPACEDATA->stackPercs[PNODE->stackNum] = std::clamp(PWORKSPACEDATA->stackPercs[PNODE->stackNum] + pixResize.x / SIZE, 0.05, 1.95);
            }
        }
    }

    if (pixResize.y != 0 && !yResizeDone) {
        //Only handle master 'in stack' resizing. Master column resizing is handled above
        if (PNODE->isMaster && getMastersOnWorkspace(PNODE->workspaceID) > 1 && PWORKSPACEDATA->orientation % 2 == 0) {
            const auto SIZE = (PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y) / getMastersOnWorkspace(PNODE->workspaceID);
            PNODE->percSize = std::clamp(PNODE->percSize + pixResize.y / SIZE, 0.05, 1.95);
        } else if (!PNODE->isMaster && (getNodesOnWorkspace(PWINDOW->workspaceID()) - getMastersOnWorkspace(PNODE->workspaceID)) > 1) {
            if (PWORKSPACEDATA->orientation % 2 == 0) { //In stack resize

                const auto SIZE = (PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y) / PWORKSPACEDATA->stackNodeCount[PNODE->stackNum];
                PNODE->percSize = std::clamp(PNODE->percSize + pixResize.y / SIZE, 0.05, 1.95);
            } else {
                const auto SIZE =
                    (PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y - PMASTERNODE->size.y) / PWORKSPACEDATA->stackNodeCount.size();
                PWORKSPACEDATA->stackPercs[PNODE->stackNum] = std::clamp(PWORKSPACEDATA->stackPercs[PNODE->stackNum] + pixResize.y / SIZE, 0.05, 1.95);
            }
        }
    }

    recalculateMonitor(PMONITOR->activeWorkspaceID());

    m_bForceWarps = false;
}

void CHyprNstackLayout::fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE) {
    const auto PMONITOR   = pWindow->m_pMonitor.lock();
    const auto PWORKSPACE = pWindow->m_pWorkspace;

    // save position and size if floating
    if (pWindow->m_bIsFloating && CURRENT_EFFECTIVE_MODE == FSMODE_NONE) {
        pWindow->m_vLastFloatingSize     = pWindow->m_vRealSize->goal();
        pWindow->m_vLastFloatingPosition = pWindow->m_vRealPosition->goal();
        pWindow->m_vPosition             = pWindow->m_vRealPosition->goal();
        pWindow->m_vSize                 = pWindow->m_vRealSize->goal();
    }

    if (EFFECTIVE_MODE == FSMODE_NONE) {
        // if it got its fullscreen disabled, set back its node if it had one
        const auto PNODE = getNodeFromWindow(pWindow);
        if (PNODE)
            applyNodeDataToWindow(PNODE);
        else {
            // get back its' dimensions from position and size
            *pWindow->m_vRealPosition = pWindow->m_vLastFloatingPosition;
            *pWindow->m_vRealSize     = pWindow->m_vLastFloatingSize;

            pWindow->unsetWindowData(PRIORITY_LAYOUT);
            pWindow->updateWindowData();
        }
    } else {
        // apply new pos and size being monitors' box
        if (EFFECTIVE_MODE == FSMODE_FULLSCREEN) {
            *pWindow->m_vRealPosition = PMONITOR->vecPosition;
            *pWindow->m_vRealSize     = PMONITOR->vecSize;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SNstackNodeData fakeNode;
            fakeNode.pWindow                = pWindow;
            fakeNode.position               = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
            fakeNode.size                   = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
            fakeNode.workspaceID            = PWORKSPACE->m_iID;
            pWindow->m_vPosition             = fakeNode.position;
            pWindow->m_vSize                 = fakeNode.size;
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->changeWindowZOrder(pWindow, true);
}

void CHyprNstackLayout::recalculateWindow(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    recalculateMonitor(pWindow->monitorID());
}

SWindowRenderLayoutHints CHyprNstackLayout::requestRenderHints(PHLWINDOW pWindow) {
    // window should be valid, insallah

    SWindowRenderLayoutHints hints;

    return hints; // master doesnt have any hints
}

void CHyprNstackLayout::switchWindows(PHLWINDOW pWindow, PHLWINDOW pWindow2) {
    // windows should be valid, insallah

    Debug::log(LOG, "SWITCH WINDOWS {} <-> {}", pWindow, pWindow2);

    const auto PNODE  = getNodeFromWindow(pWindow);
    const auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_pMonitor, pWindow->m_pMonitor);
        std::swap(pWindow2->m_pWorkspace, pWindow->m_pWorkspace);
    }

    // massive hack: just swap window pointers, lol
    PNODE->pWindow  = pWindow2;
    PNODE2->pWindow = pWindow;

    recalculateMonitor(pWindow->monitorID());
    if (PNODE2->workspaceID != PNODE->workspaceID)
        recalculateMonitor(pWindow2->monitorID());

    g_pHyprRenderer->damageWindow(pWindow);
    g_pHyprRenderer->damageWindow(pWindow2);
}

void CHyprNstackLayout::alterSplitRatio(PHLWINDOW pWindow, float ratio, bool exact) {
    // window should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto PMASTER = getMasterNodeOnWorkspace(pWindow->workspaceID());

    float      newRatio     = exact ? ratio : PMASTER->percMaster + ratio;
    PMASTER->percMaster     = std::clamp(newRatio, 0.05f, 0.95f);
    PMASTER->masterAdjusted = true;

    recalculateMonitor(pWindow->monitorID());
}

PHLWINDOW CHyprNstackLayout::getNextWindow(PHLWINDOW pWindow, bool next) {
    if (!isWindowTiled(pWindow))
        return nullptr;

    const auto PNODE = getNodeFromWindow(pWindow);

    auto       nodes = m_lMasterNodesData;
    if (!next)
        std::reverse(nodes.begin(), nodes.end());

    const auto NODEIT = std::find(nodes.begin(), nodes.end(), *PNODE);

    const bool ISMASTER = PNODE->isMaster;

    auto CANDIDATE = std::find_if(NODEIT, nodes.end(), [&](const auto& other) { return other != *PNODE && ISMASTER == other.isMaster && other.workspaceID == PNODE->workspaceID; });
    if (CANDIDATE == nodes.end())
        CANDIDATE =
            std::find_if(nodes.begin(), nodes.end(), [&](const auto& other) { return other != *PNODE && ISMASTER != other.isMaster && other.workspaceID == PNODE->workspaceID; });

    return CANDIDATE == nodes.end() ? nullptr : CANDIDATE->pWindow.lock();
}

std::any CHyprNstackLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    auto switchToWindow = [&](PHLWINDOW PWINDOWTOCHANGETO) {
        if (!validMapped(PWINDOWTOCHANGETO))
            return;

        if (header.pWindow->isFullscreen()) {
            const auto PWORKSPACE    = header.pWindow->m_pWorkspace;
            const auto FSMODE        = header.pWindow->m_sFullscreenState.internal;
            const auto WORKSPACEDATA = getMasterWorkspaceData(PWORKSPACE->m_iID);
            g_pCompositor->setWindowFullscreenInternal(header.pWindow, FSMODE_NONE);
            g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
            if (WORKSPACEDATA->inherit_fullscreen)
                g_pCompositor->setWindowFullscreenInternal(PWINDOWTOCHANGETO, FSMODE);

        } else {
            g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
            g_pCompositor->warpCursorTo(PWINDOWTOCHANGETO->middle());
        }
        g_pInputManager->m_pForcedFocus = PWINDOWTOCHANGETO;
        g_pInputManager->simulateMouseMovement();
        g_pInputManager->m_pForcedFocus.reset();
    };

    CVarList vars(message, 0, ' ');

    if (vars.size() < 1 || vars[0].empty()) {
        Debug::log(ERR, "layoutmsg called without params");
        return 0;
    }

    auto command = vars[0];

    // swapwithmaster <master | child | auto>
    // first message argument can have the following values:
    // * master - keep the focus at the new master
    // * child - keep the focus at the new child
    // * auto (default) - swap the focus (keep the focus of the previously selected window)
    if (command == "swapwithmaster") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        if (!isWindowTiled(PWINDOW))
            return 0;

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->workspaceID());

        if (!PMASTER)
            return 0;

        const auto NEWCHILD = PMASTER->pWindow.lock();

        if (PMASTER->pWindow.lock() != PWINDOW) {
            const auto NEWMASTER       = PWINDOW;
            const bool newFocusToChild = vars.size() >= 2 && vars[1] == "child";
            switchWindows(NEWMASTER, NEWCHILD);
            const auto NEWFOCUS = newFocusToChild ? NEWCHILD : NEWMASTER;
            switchToWindow(NEWFOCUS);
        } else {
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    const auto NEWMASTER = n.pWindow.lock();
                    switchWindows(NEWMASTER, NEWCHILD);
                    const bool newFocusToMaster = vars.size() >= 2 && vars[1] == "master";
                    const auto NEWFOCUS         = newFocusToMaster ? NEWMASTER : NEWCHILD;
                    switchToWindow(NEWFOCUS);
                    break;
                }
            }
        }

        return 0;
    }
    // focusmaster <master | auto>
    // first message argument can have the following values:
    // * master - keep the focus at the new master, even if it was focused before
    // * auto (default) - swap the focus with the first child, if the current focus was master, otherwise focus master
    else if (command == "focusmaster") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->workspaceID());

        if (!PMASTER)
            return 0;

        if (PMASTER->pWindow.lock() != PWINDOW) {
            switchToWindow(PMASTER->pWindow.lock());
        } else if (vars.size() >= 2 && vars[1] == "master") {
            return 0;
        } else {
            // if master is focused keep master focused (don't do anything)
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    switchToWindow(n.pWindow.lock());
                    break;
                }
            }
        }

        return 0;
    } else if (command == "cyclenext") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const auto PNEXTWINDOW = getNextWindow(PWINDOW, true);
        switchToWindow(PNEXTWINDOW);
    } else if (command == "cycleprev") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const auto PPREVWINDOW = getNextWindow(PWINDOW, false);
        switchToWindow(PPREVWINDOW);
    } else if (command == "swapnext") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating) {
            g_pKeybindManager->m_mDispatchers["swapnext"]("");
            return 0;
        }

        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, true);

        if (PWINDOWTOSWAPWITH) {
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            g_pCompositor->focusWindow(header.pWindow);
        }
    } else if (command == "swapprev") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating) {
            g_pKeybindManager->m_mDispatchers["swapnext"]("prev");
            return 0;
        }

        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, false);

        if (PWINDOWTOSWAPWITH) {
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            g_pCompositor->focusWindow(header.pWindow);
        }
    } else if (command == "addmaster") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating)
            return 0;

        const auto PNODE = getNodeFromWindow(header.pWindow);

        if (!PNODE || PNODE->isMaster) {
            // first non-master node
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == header.pWindow->workspaceID() && !n.isMaster) {
                    n.isMaster = true;
                    break;
                }
            }
        } else {
            PNODE->isMaster = true;
        }

        recalculateMonitor(header.pWindow->monitorID());

    } else if (command == "removemaster") {

        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating)
            return 0;

        const auto PNODE = getNodeFromWindow(header.pWindow);

        const auto WINDOWS = getNodesOnWorkspace(header.pWindow->workspaceID());
        const auto MASTERS = getMastersOnWorkspace(header.pWindow->workspaceID());

        if (WINDOWS < 2 || MASTERS < 2)
            return 0;

        if (!PNODE || !PNODE->isMaster) {
            // first non-master node
            for (auto it = m_lMasterNodesData.rbegin(); it != m_lMasterNodesData.rend(); it++) {
                if (it->workspaceID == header.pWindow->workspaceID() && it->isMaster) {
                    it->isMaster = false;
                    break;
                }
            }
        } else {
            PNODE->isMaster = false;
        }

        recalculateMonitor(header.pWindow->monitorID());
    } else if (command == "orientationleft" || command == "orientationright" || command == "orientationtop" || command == "orientationbottom" || command == "orientationcenter" ||
               command == "orientationhcenter" || command == "orientationvcenter") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->workspaceID());

        if (command == "orientationleft")
            PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_LEFT;
        else if (command == "orientationright")
            PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_RIGHT;
        else if (command == "orientationtop")
            PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_TOP;
        else if (command == "orientationbottom")
            PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_BOTTOM;
        else if (command == "orientationcenter" || command == "orientationhcenter")
            PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_HCENTER;
        else if (command == "orientationvcenter")
            PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_VCENTER;

        recalculateMonitor(header.pWindow->monitorID());

    } else if (command == "orientationnext") {
        runOrientationCycle(header, nullptr, 1);
    } else if (command == "orientationprev") {
        runOrientationCycle(header, nullptr, -1);
    } else if (command == "orientationcycle") {
        runOrientationCycle(header, &vars, 1);
    } else if (command == "resetsplits") {
        const auto PWINDOW = header.pWindow;
        if (!PWINDOW)
            return 0;
        resetNodeSplits(PWINDOW->workspaceID());
    } else if (command == "setstackcount") {
        const auto PWINDOW = header.pWindow;
        if (!PWINDOW)
            return 0;
        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->workspaceID());
        if (!PWORKSPACEDATA)
            return 0;

        if (vars.size() >= 2) {
            int newStackCount = 2;
            switch (vars[1][0]) {
                case '+':
                case '-': newStackCount = PWORKSPACEDATA->m_iStackCount + std::stoi(vars[1]); break;
                default: newStackCount = std::stoi(vars[1]); break;
            }
            if (newStackCount) {
                if (newStackCount < 2)
                    newStackCount = 2;
                PWORKSPACEDATA->m_iStackCount = newStackCount;
                recalculateMonitor(PWINDOW->monitorID());
            }
        }
    }

    return 0;
}

// If vars is null, we use the default list
void CHyprNstackLayout::runOrientationCycle(SLayoutMessageHeader& header, CVarList* vars, int direction) {
    std::vector<eColOrientation> cycle;
    if (vars != nullptr)
        buildOrientationCycleVectorFromVars(cycle, *vars);

    if (cycle.size() == 0)
        buildOrientationCycleVectorFromEOperation(cycle);

    const auto PWINDOW = header.pWindow;

    if (!PWINDOW)
        return;

    const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->workspaceID());

    int        nextOrPrev = 0;
    for (size_t i = 0; i < cycle.size(); ++i) {
        if (PWORKSPACEDATA->orientation == cycle.at(i)) {
            nextOrPrev = i + direction;
            break;
        }
    }

    if (nextOrPrev >= (int)cycle.size())
        nextOrPrev = nextOrPrev % (int)cycle.size();
    else if (nextOrPrev < 0)
        nextOrPrev = cycle.size() + (nextOrPrev % (int)cycle.size());

    PWORKSPACEDATA->orientation = cycle.at(nextOrPrev);
    recalculateMonitor(header.pWindow->monitorID());
}

void CHyprNstackLayout::buildOrientationCycleVectorFromEOperation(std::vector<eColOrientation>& cycle) {
    for (int i = 0; i <= NSTACK_ORIENTATION_VCENTER; ++i) {
        cycle.push_back((eColOrientation)i);
    }
}

void CHyprNstackLayout::buildOrientationCycleVectorFromVars(std::vector<eColOrientation>& cycle, CVarList& vars) {
    for (size_t i = 1; i < vars.size(); ++i) {
        if (vars[i] == "top") {
            cycle.push_back(NSTACK_ORIENTATION_TOP);
        } else if (vars[i] == "right") {
            cycle.push_back(NSTACK_ORIENTATION_RIGHT);
        } else if (vars[i] == "bottom") {
            cycle.push_back(NSTACK_ORIENTATION_BOTTOM);
        } else if (vars[i] == "left") {
            cycle.push_back(NSTACK_ORIENTATION_LEFT);
        } else if (vars[i] == "hcenter") {
            cycle.push_back(NSTACK_ORIENTATION_HCENTER);
        } else if (vars[i] == "vcenter") {
            cycle.push_back(NSTACK_ORIENTATION_VCENTER);
        }
    }
}

void CHyprNstackLayout::moveWindowTo(PHLWINDOW pWindow, const std::string& dir, bool silent) {
    if (!isDirection(dir))
        return;

    const auto PWINDOW2 = g_pCompositor->getWindowInDirection(pWindow, dir[0]);

    if (!PWINDOW2)
        return;

    pWindow->setAnimationsToMove();

    if (pWindow->m_pWorkspace != PWINDOW2->m_pWorkspace) {
        // if different monitors, send to monitor
        onWindowRemovedTiling(pWindow);
        pWindow->moveToWorkspace(PWINDOW2->m_pWorkspace);
        pWindow->m_pMonitor = PWINDOW2->m_pMonitor;
        if (!silent) {
            const auto pMonitor = pWindow->m_pMonitor.lock();
            g_pCompositor->setActiveMonitor(pMonitor);
        }
        onWindowCreatedTiling(pWindow);
    } else {
        // if same monitor, switch windows
        switchWindows(pWindow, PWINDOW2);
        if (silent)
            g_pCompositor->focusWindow(PWINDOW2);
    }
}

void CHyprNstackLayout::replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) {
    const auto PNODE = getNodeFromWindow(from);

    if (!PNODE)
        return;

    PNODE->pWindow = to;

    applyNodeDataToWindow(PNODE);
}

void CHyprNstackLayout::onEnable() {
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bIsFloating || !w->m_bIsMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w);
    }
}

void CHyprNstackLayout::onDisable() {
    m_lMasterNodesData.clear();
}

Vector2D CHyprNstackLayout::predictSizeForNewWindowTiled() {
    //What the fuck is this shit. Seriously.
    return {};
}
