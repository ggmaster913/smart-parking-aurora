import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    objectName: "mainPage"
    allowedOrientations: Orientation.All

    PageHeader {
        id: header
        objectName: "pageHeader"
        title: qsTr("Smart Parking")
        extraContent.children: [
            IconButton {
                objectName: "aboutButton"
                icon.source: "image://theme/icon-m-about"
                anchors.verticalCenter: parent.verticalCenter

                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
        ]
    }

    Label {
        objectName: "bootstrapText"
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            margins: Theme.horizontalPageMargin
        }
        color: palette.highlightColor
        font.pixelSize: Theme.fontSizeMedium
        wrapMode: Text.WordWrap
        text: qsTr("#bootstrapText")
    }
}
