import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    objectName: "defaultCover"

    CoverTemplate {
        objectName: "applicationCover"
        primaryText: "Парковка"
        secondaryText: qsTr("Smart Parking")
        icon {
            source: Qt.resolvedUrl("../icons/SmartParking.svg")
            sourceSize { width: icon.width; height: icon.height }
        }
    }
}
