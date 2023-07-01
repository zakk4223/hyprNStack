#include "nstackLayout.hpp"
#include <hyprland/src/Compositor.hpp>

SNstackNodeData* CHyprNstackLayout::getNodeFromWindow(CWindow* pWindow) {
    for (auto& nd : m_lMasterNodesData) {
        if (nd.pWindow == pWindow)
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

SNstackWorkspaceData* CHyprNstackLayout::getMasterWorkspaceData(const int& ws) {
    for (auto& n : m_lMasterWorkspacesData) {
        if (n.workspaceID == ws)
            return &n;
    }

    //create on the fly if it doesn't exist yet
    const auto PWORKSPACEDATA   = &m_lMasterWorkspacesData.emplace_back();
    PWORKSPACEDATA->workspaceID = ws;
    const auto orientation      = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:orientation")->strValue;
    if (*orientation == "top") {
        PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_TOP;
    } else if (*orientation == "right") {
        PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_RIGHT;
    } else if (*orientation == "bottom") {
        PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_BOTTOM;
    } else if (*orientation == "left") {
        PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_LEFT;
    } else if (*orientation == "vcenter") {
        PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_VCENTER;
    } else {
        PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_HCENTER;
    }
    static auto* const NUMSTACKS = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:stacks")->intValue;
    if (NUMSTACKS) {
        PWORKSPACEDATA->m_iStackCount = *NUMSTACKS;
    }

    return PWORKSPACEDATA;
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

    static auto* const MFACT         = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:mfact")->floatValue;
    const auto         WORKSPACE     = g_pCompositor->getWorkspaceByID(ws);
    const auto         WORKSPACEDATA = getMasterWorkspaceData(ws);
    if (!WORKSPACE || !WORKSPACEDATA)
        return;
    for (auto& nd : m_lMasterNodesData) {
        if (nd.workspaceID == ws) {
            nd.percSize       = 1.0f;
            nd.percMaster     = *MFACT;
            nd.masterAdjusted = false;
        }
    }

    WORKSPACEDATA->stackPercs.assign(WORKSPACEDATA->stackPercs.size(), 1.0f);
    recalculateMonitor(WORKSPACE->m_iMonitorID);
}

void CHyprNstackLayout::onWindowCreatedTiling(CWindow* pWindow) {
    if (pWindow->m_bIsFloating)
        return;

    static auto* const PNEWTOP = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:new_on_top")->intValue;

    const auto         PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    const auto PNODE = *PNEWTOP ? &m_lMasterNodesData.emplace_front() : &m_lMasterNodesData.emplace_back();

    PNODE->workspaceID = pWindow->m_iWorkspaceID;
    PNODE->pWindow     = pWindow;

    auto               OPENINGON = isWindowTiled(g_pCompositor->m_pLastWindow) && g_pCompositor->m_pLastWindow->m_iWorkspaceID == pWindow->m_iWorkspaceID ?
                      getNodeFromWindow(g_pCompositor->m_pLastWindow) :
                      getMasterNodeOnWorkspace(pWindow->m_iWorkspaceID);


    static auto* const PNEWISMASTER = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:new_is_master")->intValue;

    const auto         WINDOWSONWORKSPACE = getNodesOnWorkspace(PNODE->workspaceID);
    float              lastSplitPercent   = 0.5f;
    bool               lastMasterAdjusted = false;


    if (OPENINGON && OPENINGON->pWindow->m_sGroupData.pNextWindow && !OPENINGON->pWindow->getGroupHead()->m_sGroupData.locked && // target is an unlocked group
        (!pWindow->m_sGroupData.pNextWindow || !pWindow->getGroupHead()->m_sGroupData.locked)                                    // source is not group or is an unlocked group
        && OPENINGON != PNODE && !g_pKeybindManager->m_bGroupsLocked) {
        m_lMasterNodesData.remove(*PNODE);

        OPENINGON->pWindow->insertWindowToGroup(pWindow);

        pWindow->m_dWindowDecorations.emplace_back(std::make_unique<CHyprGroupBarDecoration>(pWindow));

        return;
    }

    bool newWindowIsMaster = false;
    if (*PNEWISMASTER || WINDOWSONWORKSPACE == 1 || (!pWindow->m_bFirstMap && OPENINGON->isMaster))
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
        if (const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(pWindow); MAXSIZE.x < PMONITOR->vecSize.x * lastSplitPercent || MAXSIZE.y < PMONITOR->vecSize.y) {
            // we can't continue. make it floating.
            pWindow->m_bIsFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    } else {
        PNODE->isMaster = false;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(pWindow);
            MAXSIZE.x < PMONITOR->vecSize.x * (1 - lastSplitPercent) || MAXSIZE.y < PMONITOR->vecSize.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            // we can't continue. make it floating.
            pWindow->m_bIsFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    }

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
        g_pCompositor->setWindowFullscreen(PFULLWINDOW, false, FULLSCREEN_FULL);
    }

    // recalc
    recalculateMonitor(pWindow->m_iMonitorID);
}

void CHyprNstackLayout::onWindowRemovedTiling(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    pWindow->m_sSpecialRenderData.rounding = true;
    pWindow->m_sSpecialRenderData.border   = true;
    pWindow->m_sSpecialRenderData.decorate = true;

    if (pWindow->m_bIsFullscreen)
        g_pCompositor->setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);

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

    recalculateMonitor(pWindow->m_iMonitorID);
}

void CHyprNstackLayout::recalculateMonitor(const int& monid) {
    const auto PMONITOR   = g_pCompositor->getMonitorFromID(monid);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);

    if (!PWORKSPACE)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->specialWorkspaceID) {
        calculateWorkspace(PMONITOR->specialWorkspaceID);
    }

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        if (PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL)
            return;

        // massive hack from the fullscreen func
        const auto      PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

        SNstackNodeData fakeNode;
        fakeNode.pWindow         = PFULLWINDOW;
        fakeNode.position        = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
        fakeNode.size            = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
        fakeNode.workspaceID     = PWORKSPACE->m_iID;
        PFULLWINDOW->m_vPosition = fakeNode.position;
        PFULLWINDOW->m_vSize     = fakeNode.size;

        applyNodeDataToWindow(&fakeNode);

        return;
    }

    // calc the WS
    calculateWorkspace(PWORKSPACE->m_iID);
}

void CHyprNstackLayout::calculateWorkspace(const int& ws) {
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(ws);

    if (!PWORKSPACE)
        return;

    static auto* const MFACT          = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:mfact")->floatValue;
    const auto         PWORKSPACEDATA = getMasterWorkspaceData(ws);
    auto         NUMSTACKS      = PWORKSPACEDATA->m_iStackCount;

    const auto         PMONITOR = g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID);

    const auto         PMASTERNODE = getMasterNodeOnWorkspace(PWORKSPACE->m_iID);
    const auto         NODECOUNT = getNodesOnWorkspace(PWORKSPACE->m_iID);

    eColOrientation    orientation        = PWORKSPACEDATA->orientation;

    if (!PMASTERNODE)
        return;


    const auto MASTERS = getMastersOnWorkspace(PWORKSPACE->m_iID);
    const auto ONLYMASTERS = !(NODECOUNT-MASTERS);

    if (NUMSTACKS < 3 && orientation > NSTACK_ORIENTATION_BOTTOM ) {
      NUMSTACKS = 3;
    }
    
    if (!PMASTERNODE->masterAdjusted) {
        if (getNodesOnWorkspace(PWORKSPACE->m_iID) < NUMSTACKS) {
            PMASTERNODE->percMaster = *MFACT ? *MFACT : 1.0f / getNodesOnWorkspace(PWORKSPACE->m_iID);
        } else {
            PMASTERNODE->percMaster = *MFACT ? *MFACT : 1.0f / (NUMSTACKS);
        }
    }
    bool               centerMasterWindow = false;
    static auto* const ALWAYSCENTER       = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:center_single_master")->intValue;
    if (*ALWAYSCENTER == 1)
        centerMasterWindow = true;
    if (!ONLYMASTERS && NODECOUNT-MASTERS < 3) {
        if (orientation == NSTACK_ORIENTATION_HCENTER) {
            orientation = NSTACK_ORIENTATION_LEFT;
        } else if (orientation == NSTACK_ORIENTATION_VCENTER) {
            orientation = NSTACK_ORIENTATION_TOP;
        }
    }

    auto MCONTAINERPOS = Vector2D(0.0f, 0.0f);
    auto MCONTAINERSIZE = Vector2D(0.0f, 0.0f);

    if (ONLYMASTERS) {
      if (centerMasterWindow) {

            if (!PMASTERNODE->masterAdjusted)
                PMASTERNODE->percMaster = *MFACT ? *MFACT : 0.5f;

            if (orientation == NSTACK_ORIENTATION_TOP || orientation == NSTACK_ORIENTATION_BOTTOM) {
                const float HEIGHT        = (PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y) * PMASTERNODE->percMaster;
                float       CENTER_OFFSET = (PMONITOR->vecSize.y - HEIGHT) / 2;
                MCONTAINERSIZE         = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x, HEIGHT);
                MCONTAINERPOS     = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(0, CENTER_OFFSET);
            } else {
                const float WIDTH         = (PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x) * PMASTERNODE->percMaster;
                float       CENTER_OFFSET = (PMONITOR->vecSize.x - WIDTH) / 2;
                MCONTAINERSIZE         = Vector2D(WIDTH, PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y);
                MCONTAINERPOS     = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(CENTER_OFFSET, 0);
            }
      } else {
        MCONTAINERPOS = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition;
        MCONTAINERSIZE = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x,
                                   PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y);
      }
    } else {
      const float MASTERSIZE = orientation % 2 == 0 ? (PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x) * PMASTERNODE->percMaster : (PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y) * PMASTERNODE->percMaster;

      if (orientation == NSTACK_ORIENTATION_RIGHT) {
        MCONTAINERPOS = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(PMONITOR->vecSize.x - MASTERSIZE - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x, 0.0f);
        MCONTAINERSIZE = Vector2D(MASTERSIZE, PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y);
      } else if (orientation == NSTACK_ORIENTATION_LEFT) {
        MCONTAINERPOS = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition;
        MCONTAINERSIZE = Vector2D(MASTERSIZE, PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y);
      } else if (orientation == NSTACK_ORIENTATION_TOP) {
        MCONTAINERPOS = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition;
        MCONTAINERSIZE = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x, MASTERSIZE);
      } else if (orientation == NSTACK_ORIENTATION_BOTTOM) {
        MCONTAINERPOS = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(0.0f, PMONITOR->vecSize.y - MASTERSIZE - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y);
        MCONTAINERSIZE = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x, MASTERSIZE);

      } else if (orientation == NSTACK_ORIENTATION_HCENTER) {
        float       CENTER_OFFSET = (PMONITOR->vecSize.x - MASTERSIZE) / 2;
        MCONTAINERSIZE         = Vector2D(MASTERSIZE, PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y);
        MCONTAINERPOS     = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(CENTER_OFFSET, 0);
      } else if (orientation == NSTACK_ORIENTATION_VCENTER) {
                float       CENTER_OFFSET = (PMONITOR->vecSize.y - MASTERSIZE) / 2;
                MCONTAINERSIZE         = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x, MASTERSIZE);
                MCONTAINERPOS     = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(0, CENTER_OFFSET);

      }

    } 

    if (MCONTAINERSIZE != Vector2D(0,0)) {
        float       nodeSpaceLeft = orientation % 2 == 0 ? MCONTAINERSIZE.y : MCONTAINERSIZE.x; 
        int         nodesLeft     = MASTERS;
        float       nextNodeCoord = 0;
        const float MASTERSIZE    = orientation % 2 == 0 ? MCONTAINERSIZE.x : MCONTAINERSIZE.y; 
        for (auto& n : m_lMasterNodesData) {
            if (n.workspaceID == PWORKSPACE->m_iID && n.isMaster) {
                if (orientation == NSTACK_ORIENTATION_RIGHT) {
                    n.position = MCONTAINERPOS + Vector2D(0.0f, nextNodeCoord);
                } else if (orientation == NSTACK_ORIENTATION_LEFT) {

                    n.position = MCONTAINERPOS + Vector2D(0, nextNodeCoord);
                } else if (orientation == NSTACK_ORIENTATION_TOP) {
                    n.position = MCONTAINERPOS + Vector2D(nextNodeCoord, 0);
                } else if (orientation == NSTACK_ORIENTATION_BOTTOM) {
                    n.position = MCONTAINERPOS +  Vector2D(nextNodeCoord, 0);
                } else if (orientation == NSTACK_ORIENTATION_HCENTER) {
                    n.position          = MCONTAINERPOS + Vector2D(0, nextNodeCoord);
                } else {
                    n.position          = MCONTAINERPOS + Vector2D(nextNodeCoord, 0); 
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
    int slavesLeft     = getNodesOnWorkspace(PWORKSPACE->m_iID) - MASTERS;
    int slavesTotal    = slavesLeft;
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
        float monMax     = orientation % 2 == 1 ? PMONITOR->vecSize.y : PMONITOR->vecSize.x;
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
    CMonitor* PMONITOR = nullptr;

    if (g_pCompositor->isWorkspaceSpecial(pNode->workspaceID)) {
        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->specialWorkspaceID == pNode->workspaceID) {
                PMONITOR = m.get();
                break;
            }
        }
    } else {
        PMONITOR = g_pCompositor->getMonitorFromID(g_pCompositor->getWorkspaceByID(pNode->workspaceID)->m_iMonitorID);
    }

    if (!PMONITOR) {
        Debug::log(ERR, "Orphaned Node %x (workspace ID: %i)!!", pNode, pNode->workspaceID);
        return;
    }

    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(pNode->position.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->position.x + pNode->size.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pNode->position.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->position.y + pNode->size.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    const auto PBORDERSIZE = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;
    const auto PGAPSIN     = &g_pConfigManager->getConfigValuePtr("general:gaps_in")->intValue;
    const auto PGAPSOUT    = &g_pConfigManager->getConfigValuePtr("general:gaps_out")->intValue;

    const auto PWINDOW = pNode->pWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW)) {
        Debug::log(ERR, "Node %x holding invalid window %x!!", pNode, PWINDOW);
        return;
    }

    static auto* const PNOGAPSWHENONLY = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:no_gaps_when_only")->intValue;

    PWINDOW->m_vSize     = pNode->size;
    PWINDOW->m_vPosition = pNode->position;

    auto calcPos  = PWINDOW->m_vPosition + Vector2D(*PBORDERSIZE, *PBORDERSIZE);
    auto calcSize = PWINDOW->m_vSize - Vector2D(2 * *PBORDERSIZE, 2 * *PBORDERSIZE);

    if (*PNOGAPSWHENONLY && !g_pCompositor->isWorkspaceSpecial(PWINDOW->m_iWorkspaceID) &&
        (getNodesOnWorkspace(PWINDOW->m_iWorkspaceID) == 1 ||
         (PWINDOW->m_bIsFullscreen && g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID)->m_efFullscreenMode == FULLSCREEN_MAXIMIZED))) {
        PWINDOW->m_vRealPosition = calcPos - Vector2D(*PBORDERSIZE, *PBORDERSIZE);
        PWINDOW->m_vRealSize     = calcSize + Vector2D(2 * *PBORDERSIZE, 2 * *PBORDERSIZE);

        PWINDOW->updateWindowDecos();

        PWINDOW->m_sSpecialRenderData.rounding = false;
        PWINDOW->m_sSpecialRenderData.border   = false;
        PWINDOW->m_sSpecialRenderData.decorate = false;

        return;
    }

    PWINDOW->m_sSpecialRenderData.rounding = true;
    PWINDOW->m_sSpecialRenderData.border   = true;
    PWINDOW->m_sSpecialRenderData.decorate = true;

    const auto OFFSETTOPLEFT = Vector2D(DISPLAYLEFT ? *PGAPSOUT : *PGAPSIN, DISPLAYTOP ? *PGAPSOUT : *PGAPSIN);

    const auto OFFSETBOTTOMRIGHT = Vector2D(DISPLAYRIGHT ? *PGAPSOUT : *PGAPSIN, DISPLAYBOTTOM ? *PGAPSOUT : *PGAPSIN);

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    if (g_pCompositor->isWorkspaceSpecial(PWINDOW->m_iWorkspaceID)) {
        static auto* const PSCALEFACTOR = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:special_scale_factor")->floatValue;

        PWINDOW->m_vRealPosition = calcPos + (calcSize - calcSize * *PSCALEFACTOR) / 2.f;
        PWINDOW->m_vRealSize     = calcSize * *PSCALEFACTOR;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize * *PSCALEFACTOR);
    } else {
        PWINDOW->m_vRealSize     = calcSize;
        PWINDOW->m_vRealPosition = calcPos;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize);
    }

    if (m_bForceWarps) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_vRealPosition.warp();
        PWINDOW->m_vRealSize.warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

bool CHyprNstackLayout::isWindowTiled(CWindow* pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CHyprNstackLayout::resizeActiveWindow(const Vector2D& pixResize, CWindow* pWindow) {
    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    const auto PNODE = getNodeFromWindow(PWINDOW);

    if (!PNODE) {
        PWINDOW->m_vRealSize = Vector2D(std::max((PWINDOW->m_vRealSize.goalv() + pixResize).x, 20.0), std::max((PWINDOW->m_vRealSize.goalv() + pixResize).y, 20.0));
        PWINDOW->updateWindowDecos();
        return;
    }

    // get monitor
    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    m_bForceWarps = true;

    const auto PMASTERNODE    = getMasterNodeOnWorkspace(PWINDOW->m_iWorkspaceID);
    const auto PWORKSPACEDATA = getMasterWorkspaceData(PMONITOR->activeWorkspace);
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

        for (auto& n : m_lMasterNodesData) {
            if (n.isMaster && n.workspaceID == PMONITOR->activeWorkspace) {
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
        } else if (!PNODE->isMaster && (getNodesOnWorkspace(PWINDOW->m_iWorkspaceID) - getMastersOnWorkspace(PNODE->workspaceID)) > 1) {
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
        } else if (!PNODE->isMaster && (getNodesOnWorkspace(PWINDOW->m_iWorkspaceID) - getMastersOnWorkspace(PNODE->workspaceID)) > 1) {
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

    recalculateMonitor(PMONITOR->ID);

    m_bForceWarps = false;
}

void CHyprNstackLayout::fullscreenRequestForWindow(CWindow* pWindow, eFullscreenMode fullscreenMode, bool on) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    if (on == pWindow->m_bIsFullscreen || g_pCompositor->isWorkspaceSpecial(pWindow->m_iWorkspaceID))
        return; // ignore

    const auto PMONITOR   = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow && on) {
        // if the window wants to be fullscreen but there already is one,
        // ignore the request.
        return;
    }

    // otherwise, accept it.
    pWindow->m_bIsFullscreen           = on;
    PWORKSPACE->m_bHasFullscreenWindow = !PWORKSPACE->m_bHasFullscreenWindow;

    g_pEventManager->postEvent(SHyprIPCEvent{"fullscreen", std::to_string((int)on)});
    EMIT_HOOK_EVENT("fullscreen", pWindow);

    if (!pWindow->m_bIsFullscreen) {
        // if it got its fullscreen disabled, set back its node if it had one
        const auto PNODE = getNodeFromWindow(pWindow);
        if (PNODE)
            applyNodeDataToWindow(PNODE);
        else {
            // get back its' dimensions from position and size
            pWindow->m_vRealPosition = pWindow->m_vLastFloatingPosition;
            pWindow->m_vRealSize     = pWindow->m_vLastFloatingSize;

            pWindow->m_sSpecialRenderData.rounding = true;
            pWindow->m_sSpecialRenderData.border   = true;
            pWindow->m_sSpecialRenderData.decorate = true;
        }
    } else {
        // if it now got fullscreen, make it fullscreen

        PWORKSPACE->m_efFullscreenMode = fullscreenMode;

        // save position and size if floating
        if (pWindow->m_bIsFloating) {
            pWindow->m_vLastFloatingSize     = pWindow->m_vRealSize.goalv();
            pWindow->m_vLastFloatingPosition = pWindow->m_vRealPosition.goalv();
            pWindow->m_vPosition             = pWindow->m_vRealPosition.goalv();
            pWindow->m_vSize                 = pWindow->m_vRealSize.goalv();
        }

        // apply new pos and size being monitors' box
        if (fullscreenMode == FULLSCREEN_FULL) {
            pWindow->m_vRealPosition = PMONITOR->vecPosition;
            pWindow->m_vRealSize     = PMONITOR->vecSize;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SNstackNodeData fakeNode;
            fakeNode.pWindow     = pWindow;
            fakeNode.position    = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
            fakeNode.size        = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
            fakeNode.workspaceID = pWindow->m_iWorkspaceID;
            pWindow->m_vPosition = fakeNode.position;
            pWindow->m_vSize     = fakeNode.size;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goalv());

    g_pCompositor->moveWindowToTop(pWindow);

    recalculateMonitor(PMONITOR->ID);
}

void CHyprNstackLayout::recalculateWindow(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    recalculateMonitor(pWindow->m_iMonitorID);
}

SWindowRenderLayoutHints CHyprNstackLayout::requestRenderHints(CWindow* pWindow) {
    // window should be valid, insallah

    SWindowRenderLayoutHints hints;

    return hints; // master doesnt have any hints
}

void CHyprNstackLayout::switchWindows(CWindow* pWindow, CWindow* pWindow2) {
    // windows should be valid, insallah

    const auto PNODE  = getNodeFromWindow(pWindow);
    const auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    const auto inheritFullscreen = prepareLoseFocus(pWindow);

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_iMonitorID, pWindow->m_iMonitorID);
        std::swap(pWindow2->m_iWorkspaceID, pWindow->m_iWorkspaceID);
    }

    // massive hack: just swap window pointers, lol
    PNODE->pWindow  = pWindow2;
    PNODE2->pWindow = pWindow;

    recalculateMonitor(pWindow->m_iMonitorID);
    if (PNODE2->workspaceID != PNODE->workspaceID)
        recalculateMonitor(pWindow2->m_iMonitorID);

    g_pHyprRenderer->damageWindow(pWindow);
    g_pHyprRenderer->damageWindow(pWindow2);

    prepareNewFocus(pWindow2, inheritFullscreen);
}

void CHyprNstackLayout::alterSplitRatio(CWindow* pWindow, float ratio, bool exact) {
    // window should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto PMASTER = getMasterNodeOnWorkspace(pWindow->m_iWorkspaceID);

    float      newRatio     = exact ? ratio : PMASTER->percMaster + ratio;
    PMASTER->percMaster     = std::clamp(newRatio, 0.05f, 0.95f);
    PMASTER->masterAdjusted = true;

    recalculateMonitor(pWindow->m_iMonitorID);
}

CWindow* CHyprNstackLayout::getNextWindow(CWindow* pWindow, bool next) {
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

    return CANDIDATE == nodes.end() ? nullptr : CANDIDATE->pWindow;
}




bool CHyprNstackLayout::prepareLoseFocus(CWindow* pWindow) {
    if (!pWindow)
        return false;

    //if the current window is fullscreen, make it normal again if we are about to lose focus
    if (pWindow->m_bIsFullscreen) {
        g_pCompositor->setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);
        static auto* const INHERIT = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:inherit_fullscreen")->intValue;
        return *INHERIT == 1;
    }

    return false;
}

void CHyprNstackLayout::prepareNewFocus(CWindow* pWindow, bool inheritFullscreen) {
    if (!pWindow)
        return;

    if (inheritFullscreen)
        g_pCompositor->setWindowFullscreen(pWindow, true, g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID)->m_efFullscreenMode);
}

std::any CHyprNstackLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    auto switchToWindow = [&](CWindow* PWINDOWTOCHANGETO) {
        if (!g_pCompositor->windowValidMapped(PWINDOWTOCHANGETO))
            return;

        g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
        g_pCompositor->warpCursorTo(PWINDOWTOCHANGETO->m_vRealPosition.goalv() + PWINDOWTOCHANGETO->m_vRealSize.goalv() / 2.f);
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

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->m_iWorkspaceID);

        if (!PMASTER)
            return 0;

        const auto NEWCHILD = PMASTER->pWindow;

        if (PMASTER->pWindow != PWINDOW) {
            const auto NEWMASTER         = PWINDOW;
            const bool newFocusToChild   = vars.size() >= 2 && vars[1] == "child";
            const bool inheritFullscreen = prepareLoseFocus(NEWMASTER);
            switchWindows(NEWMASTER, NEWCHILD);
            const auto NEWFOCUS = newFocusToChild ? NEWCHILD : NEWMASTER;
            switchToWindow(NEWFOCUS);
            prepareNewFocus(NEWFOCUS, inheritFullscreen);
        } else {
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    const auto NEWMASTER         = n.pWindow;
                    const bool inheritFullscreen = prepareLoseFocus(NEWCHILD);
                    switchWindows(NEWMASTER, NEWCHILD);
                    const bool newFocusToMaster = vars.size() >= 2 && vars[1] == "master";
                    const auto NEWFOCUS         = newFocusToMaster ? NEWMASTER : NEWCHILD;
                    switchToWindow(NEWFOCUS);
                    prepareNewFocus(NEWFOCUS, inheritFullscreen);
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

        const bool inheritFullscreen = prepareLoseFocus(PWINDOW);

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->m_iWorkspaceID);

        if (!PMASTER)
            return 0;

        if (PMASTER->pWindow != PWINDOW) {
            switchToWindow(PMASTER->pWindow);
            prepareNewFocus(PMASTER->pWindow, inheritFullscreen);
        } else if (vars.size() >= 2 && vars[1] == "master") {
            return 0;
        } else {
            // if master is focused keep master focused (don't do anything)
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    switchToWindow(n.pWindow);
                    prepareNewFocus(n.pWindow, inheritFullscreen);
                    break;
                }
            }
        }

        return 0;
    } else if (command == "cyclenext") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool inheritFullscreen = prepareLoseFocus(PWINDOW);

        const auto PNEXTWINDOW = getNextWindow(PWINDOW, true);
        switchToWindow(PNEXTWINDOW);
        prepareNewFocus(PNEXTWINDOW, inheritFullscreen);
    } else if (command == "cycleprev") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool inheritFullscreen = prepareLoseFocus(PWINDOW);

        const auto PPREVWINDOW = getNextWindow(PWINDOW, false);
        switchToWindow(PPREVWINDOW);
        prepareNewFocus(PPREVWINDOW, inheritFullscreen);
    } else if (command == "swapnext") {
        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating) {
            g_pKeybindManager->m_mDispatchers["swapnext"]("");
            return 0;
        }

        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, true);

        if (PWINDOWTOSWAPWITH) {
            prepareLoseFocus(header.pWindow);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            g_pCompositor->focusWindow(header.pWindow);
        }
    } else if (command == "swapprev") {
        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating) {
            g_pKeybindManager->m_mDispatchers["swapnext"]("prev");
            return 0;
        }

        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, false);

        if (PWINDOWTOSWAPWITH) {
            prepareLoseFocus(header.pWindow);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            g_pCompositor->focusWindow(header.pWindow);
        }
    } else if (command == "addmaster") {
        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating)
            return 0;

        const auto PNODE = getNodeFromWindow(header.pWindow);


        prepareLoseFocus(header.pWindow);

        if (!PNODE || PNODE->isMaster) {
            // first non-master node
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == header.pWindow->m_iWorkspaceID && !n.isMaster) {
                    n.isMaster = true;
                    break;
                }
            }
        } else {
            PNODE->isMaster = true;
        }

        recalculateMonitor(header.pWindow->m_iMonitorID);

    } else if (command == "removemaster") {

        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating)
            return 0;

        const auto PNODE = getNodeFromWindow(header.pWindow);

        const auto WINDOWS = getNodesOnWorkspace(header.pWindow->m_iWorkspaceID);
        const auto MASTERS = getMastersOnWorkspace(header.pWindow->m_iWorkspaceID);

        if (WINDOWS < 2 || MASTERS < 2)
            return 0;

        prepareLoseFocus(header.pWindow);

        if (!PNODE || !PNODE->isMaster) {
            // first non-master node
            for (auto it = m_lMasterNodesData.rbegin(); it != m_lMasterNodesData.rend(); it++) {
                if (it->workspaceID == header.pWindow->m_iWorkspaceID && it->isMaster) {
                    it->isMaster = false;
                    break;
                }
            }
        } else {
            PNODE->isMaster = false;
        }

        recalculateMonitor(header.pWindow->m_iMonitorID);
    } else if (command == "orientationleft" || command == "orientationright" || command == "orientationtop" || command == "orientationbottom" || command == "orientationcenter" ||
               command == "orientationhcenter" || command == "orientationvcenter") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        prepareLoseFocus(PWINDOW);

        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->m_iWorkspaceID);

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

        recalculateMonitor(header.pWindow->m_iMonitorID);

    } else if (command == "orientationnext") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        prepareLoseFocus(PWINDOW);

        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->m_iWorkspaceID);

        if (PWORKSPACEDATA->orientation == NSTACK_ORIENTATION_VCENTER) {
            PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_LEFT;
        } else {
            PWORKSPACEDATA->orientation = (eColOrientation)(PWORKSPACEDATA->orientation + 1);
        }

        recalculateMonitor(header.pWindow->m_iMonitorID);
    } else if (command == "orientationprev") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        prepareLoseFocus(PWINDOW);

        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->m_iWorkspaceID);

        if (PWORKSPACEDATA->orientation == NSTACK_ORIENTATION_LEFT) {
            PWORKSPACEDATA->orientation = NSTACK_ORIENTATION_VCENTER;
        } else {
            PWORKSPACEDATA->orientation = (eColOrientation)(PWORKSPACEDATA->orientation - 1);
        }

        recalculateMonitor(header.pWindow->m_iMonitorID);
    } else if (command == "resetsplits") {
        const auto PWINDOW = header.pWindow;
        if (!PWINDOW)
            return 0;
        resetNodeSplits(PWINDOW->m_iWorkspaceID);
    } else if (command == "setstackcount") {
        const auto PWINDOW = header.pWindow;
        if (!PWINDOW)
            return 0;
        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->m_iWorkspaceID);
        if (!PWORKSPACEDATA)
            return 0;

        if (vars.size() >= 2) {
            int newStackCount = std::stoi(vars[1]);
            if (newStackCount) {
                if (newStackCount < 2)
                    newStackCount = 2;
                PWORKSPACEDATA->m_iStackCount = newStackCount;
                recalculateMonitor(PWINDOW->m_iMonitorID);
            }
        }
    }

    return 0;
}

void CHyprNstackLayout::replaceWindowDataWith(CWindow* from, CWindow* to) {
    const auto PNODE = getNodeFromWindow(from);

    if (!PNODE)
        return;

    PNODE->pWindow = to;

    applyNodeDataToWindow(PNODE);
}

void CHyprNstackLayout::onEnable() {
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bIsFloating || !w->m_bMappedX11 || !w->m_bIsMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w.get());
    }
}

void CHyprNstackLayout::onDisable() {
    m_lMasterNodesData.clear();
}
