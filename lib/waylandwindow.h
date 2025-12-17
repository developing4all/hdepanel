/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
 * Authors:
 *   Haydar Alkaduhimi <haydar@developing4all.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef WAYLANDWINDOW_H
#define WAYLANDWINDOW_H

#include <QString>

// Comprehensive representation of a "window" for panels/taskbars
struct WaylandWindow {
    // Basic identification
    QString title;
    QString appId;
    QString iconName;
    QString wmClass;
    QString wmInstanceClass;
    QString role;
    
    // Workspace/Desktop information
    int workspaceIndex;
    QString workspaceName;
    bool onAllWorkspaces;
    bool isOnCurrentWorkspace;
    
    // Screen/Monitor information
    int monitorIndex;
    
    // Geometry
    int x;
    int y;
    int width;
    int height;
    
    // Window state
    bool visible;
    bool focused;
    bool minimized;
    bool maximized;
    bool maximizedHorizontally;
    bool maximizedVertically;
    
    // Window flags
    bool demandsAttention;
    bool urgent;
    bool skipTaskbar;
    bool skipPager;
    bool decorated;
    bool resizable;
    bool moveable;
    
    // Client type
    bool isWayland;
    bool isX11;
    QString clientType;
    
    // Process information
    int pid;
    QString sandboxedAppId;
    
    // Visual properties
    double opacity;
    
    // Group information
    bool hasGroup;
    unsigned long groupLeaderId;
    
    // Timestamps
    unsigned long createdTime;
    unsigned long focusTime;
    
    // Legacy fields for compatibility
    void* surface;
    void* toplevel;
};

#endif // WAYLANDWINDOW_H
