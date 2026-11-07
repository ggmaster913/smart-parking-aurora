import QtQuick 2.0
import Sailfish.Silica 1.0

// Состояние загрузки, пустоты или ошибки. Контракт: docs/DESIGN.md, раздел 5.
// Панель не вызывает сервисы: повтор выполняет страница по сигналу retryRequested.
Column {
    id: panel

    property string title
    property string message
    property bool busy: false
    property bool isError: false
    property string retryText: qsTr("Повторить")

    signal retryRequested()

    width: parent ? parent.width : implicitWidth
    spacing: Theme.paddingMedium

    BusyIndicator {
        anchors.horizontalCenter: parent.horizontalCenter
        size: BusyIndicatorSize.Medium
        running: panel.busy
        visible: panel.busy
    }

    Row {
        width: parent.width
        spacing: Theme.paddingSmall
        visible: panel.title.length > 0

        Image {
            id: warningIcon
            visible: panel.isError && !panel.busy
            source: "image://theme/icon-m-warning"
            sourceSize.width: Theme.iconSizeMedium
            sourceSize.height: Theme.iconSizeMedium
            anchors.verticalCenter: parent.verticalCenter
        }

        Label {
            width: parent.width - (warningIcon.visible ? warningIcon.width + parent.spacing : 0)
            text: panel.title
            color: panel.isError ? Theme.errorColor : Theme.highlightColor
            font.pixelSize: Theme.fontSizeLarge
            font.family: Theme.fontFamilyHeading
            wrapMode: Text.Wrap
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Label {
        width: parent.width
        visible: panel.message.length > 0
        text: panel.message
        color: Theme.secondaryHighlightColor
        font.pixelSize: Theme.fontSizeSmall
        wrapMode: Text.Wrap
    }

    Button {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: !panel.busy && panel.retryText.length > 0
        text: panel.retryText
        onClicked: panel.retryRequested()
    }
}
