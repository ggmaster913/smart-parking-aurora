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

    // Кнопка никогда не шире панели. Если подпись не помещается в стандартную
    // кнопку, вместо неё показывается полноширинное действие с переносом текста.
    readonly property bool showRetry: !busy && retryText.length > 0
    readonly property real retryButtonWidth: Math.min(Theme.buttonWidthLarge, width)
    readonly property bool retryFitsButton: retryProbe.implicitWidth + 2 * Theme.paddingLarge
                                            <= retryButtonWidth

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

    // Невидимый образец для измерения подписи; Column не отводит ему места.
    Label {
        id: retryProbe
        visible: false
        text: panel.retryText
        font.pixelSize: Theme.fontSizeMedium
    }

    Button {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: panel.showRetry && panel.retryFitsButton
        width: panel.retryButtonWidth
        preferredWidth: panel.retryButtonWidth
        text: panel.retryText
        onClicked: panel.retryRequested()
    }

    BackgroundItem {
        id: wideRetry
        visible: panel.showRetry && !panel.retryFitsButton
        width: parent.width
        height: Math.max(Theme.itemSizeSmall, wideRetryLabel.height + 2 * Theme.paddingMedium)
        onClicked: panel.retryRequested()

        Label {
            id: wideRetryLabel
            anchors.centerIn: parent
            width: parent.width - 2 * Theme.paddingMedium
            horizontalAlignment: Text.AlignHCenter
            text: panel.retryText
            color: wideRetry.highlighted ? Theme.highlightColor : Theme.primaryColor
            font.pixelSize: Theme.fontSizeMedium
            wrapMode: Text.Wrap
        }
    }
}
