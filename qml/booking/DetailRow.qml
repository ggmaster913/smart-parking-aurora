import QtQuick 2.0
import Sailfish.Silica 1.0

// Строка «подпись — значение» для экранов бронирования. Значение переносится.
Column {
    property alias label: labelText.text
    property alias value: valueText.text
    property alias valueColor: valueText.color
    property alias valueSize: valueText.font.pixelSize

    width: parent ? parent.width : implicitWidth
    spacing: Theme.paddingSmall / 2
    visible: valueText.text.length > 0

    Label {
        id: labelText
        width: parent.width
        color: Theme.secondaryHighlightColor
        font.pixelSize: Theme.fontSizeExtraSmall
        wrapMode: Text.Wrap
    }

    Label {
        id: valueText
        width: parent.width
        color: Theme.highlightColor
        font.pixelSize: Theme.fontSizeMedium
        wrapMode: Text.Wrap
    }
}
