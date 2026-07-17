import QtQuick

MouseArea {
    id: root

    property string hoveredLink: ""

    anchors.fill: parent
    acceptedButtons: Qt.NoButton
    hoverEnabled: true
    cursorShape: hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.IBeamCursor

    function refreshHoveredLink() {
        hoveredLink = parent.linkAt(mouseX, mouseY) ?? "";
    }

    onPositionChanged: refreshHoveredLink()
    onContainsMouseChanged: {
        if (containsMouse)
            refreshHoveredLink();
        else
            hoveredLink = "";
    }
}