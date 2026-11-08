import QtQuick 2.0
import Sailfish.Silica 1.0
import "../design"

// История парковок и расходы (#22). Данные — только historyService (read-only SQLite).
// Экран обновляется при активации и возврате в приложение, без таймера опроса.
Page {
    id: page

    signal backRequested()

    readonly property var history: typeof historyService !== "undefined" ? historyService : null
    property var lastResult: ({})
    readonly property string errorCode: lastResult && lastResult.ok === false ? lastResult.errorCode : ""
    readonly property int shown: history !== null && history.entries ? history.entries.length : 0
    readonly property int total: history !== null ? history.totalSessions : 0

    function reload() {
        if (history === null) {
            lastResult = { ok: false, errorCode: "STORAGE_ERROR",
                           message: qsTr("Сервис истории недоступен."), data: {} }
            return
        }
        lastResult = history.refresh()
    }

    function plural(count, one, few, many) {
        var mod10 = count % 10
        var mod100 = count % 100
        if (mod10 === 1 && mod100 !== 11)
            return one
        if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14))
            return few
        return many
    }

    onStatusChanged: {
        if (status === PageStatus.Activating)
            reload()
    }

    Connections {
        target: Qt.application
        onStateChanged: {
            if (Qt.application.state === Qt.ApplicationActive && page.status === PageStatus.Active)
                page.reload()
        }
    }

    SilicaListView {
        id: list

        anchors.fill: parent
        model: page.errorCode.length === 0 && page.history !== null ? page.history.entries : []

        header: Column {
            width: list.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("История парковок")
            }

            // Сводка: только при успешной загрузке, ошибка не выдаётся за ноль.
            Column {
                visible: page.errorCode.length === 0 && page.total > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                spacing: Theme.paddingSmall

                Label {
                    text: qsTr("Всего по парковкам")
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeSmall
                }
                PriceLabel {
                    width: parent.width
                    amountKopecks: page.history !== null ? page.history.totalKopecks : 0
                    freeText: ""
                    font.pixelSize: Theme.fontSizeExtraLarge
                }
                Label {
                    width: parent.width
                    text: page.total + " " + page.plural(page.total, qsTr("парковка"), qsTr("парковки"), qsTr("парковок"))
                          + " · " + qsTr("оплаты демонстрационные")
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    wrapMode: Text.Wrap
                }
                Label {
                    visible: page.total > page.shown
                    width: parent.width
                    text: qsTr("В списке последние") + " " + page.shown + " " + qsTr("из") + " " + page.total
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    wrapMode: Text.Wrap
                }
            }

            StatusPanel {
                visible: page.errorCode === "NOT_AUTHENTICATED"
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                title: qsTr("Нужен вход")
                message: page.lastResult.message || ""
                retryText: qsTr("Назад")
                onRetryRequested: page.backRequested()
            }

            StatusPanel {
                visible: page.errorCode.length > 0 && page.errorCode !== "NOT_AUTHENTICATED"
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                isError: true
                title: qsTr("История не загрузилась")
                message: page.lastResult.message || ""
                onRetryRequested: page.reload()
            }

            Item { width: 1; height: Theme.paddingSmall }
        }

        delegate: ListItem {
            id: item

            contentHeight: row.height + 2 * Theme.paddingMedium

            Item {
                id: row

                x: Theme.horizontalPageMargin
                y: Theme.paddingMedium
                width: parent.width - 2 * x
                height: Math.max(textColumn.height, amount.height)

                Column {
                    id: textColumn

                    width: parent.width - amount.width - Theme.paddingMedium
                    spacing: Theme.paddingSmall / 2

                    Label {
                        width: parent.width
                        text: modelData.zoneName
                        color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                        font.pixelSize: Theme.fontSizeMedium
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        width: parent.width
                        text: modelData.address
                        color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                        wrapMode: Text.Wrap
                    }
                    Label {
                        width: parent.width
                        text: modelData.plate + " · " + AppStyle.formatLocalTime(modelData.startsAtUtc)
                              + " · " + (modelData.status === "active" ? qsTr("идёт") : qsTr("завершена"))
                        color: modelData.status === "active" ? AppStyle.accentColor
                                                             : (item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        wrapMode: Text.Wrap
                    }
                }

                PriceLabel {
                    id: amount

                    anchors.right: parent.right
                    width: Math.min(implicitWidth, row.width / 2)
                    horizontalAlignment: Text.AlignRight
                    amountKopecks: modelData.amountKopecks
                    freeText: ""
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
            }
        }

        ViewPlaceholder {
            enabled: page.errorCode.length === 0 && page.history !== null && page.total === 0
            text: qsTr("Пока нет парковок")
            hintText: qsTr("Оформленные парковки и расходы появятся здесь.")
        }

        VerticalScrollDecorator {}
    }
}
