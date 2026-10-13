// Takes the focus from Gamescope's host window and gives it back, so the host sends a configure
// with the activated state flipped, as when the user switches windows on the desktop.
import QtQuick
import org.kde.kwin as KWin

Item {
    Timer {
        interval: 170
        repeat: true
        running: true
        onTriggered: {
            for (const window of KWin.Workspace.windows) {
                if (window.resourceClass !== "gamescope") {
                    continue;
                }
                window.minimized = !window.minimized;
                if (!window.minimized) {
                    KWin.Workspace.activeWindow = window;
                }
                console.info("focus_flap: gamescope minimized=" + window.minimized);
            }
        }
    }
}
