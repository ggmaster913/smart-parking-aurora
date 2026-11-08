import QtQuick 2.0
import Sailfish.Silica 1.0
import "../design"
import "BookingUtils.js" as Utils

// Активная парковка: крупный остаток, шкала времени, продление и возврат к зоне (#5, #19).
// Остаток всегда считается как max(0, expiresAtUtc - nowUtc), таймер только перерисовывает экран.
Page {
    id: page

    signal backRequested()
    // Открыть карту на зоне сохранённой сессии; обработчик подключает интегратор (#19).
    signal locateParkingRequested(string zoneId)

    readonly property var booking: typeof bookingService !== "undefined" ? bookingService : null
    readonly property var parking: typeof parkingService !== "undefined" ? parkingService : null

    readonly property var activeSession: booking !== null ? booking.activeSession : ({})
    readonly property bool hasActive: Utils.hasSession(activeSession)
    // Последняя показанная сессия: нужна, чтобы после завершения показать «Время истекло».
    property var lastSession: ({})
    readonly property var shownSession: hasActive ? activeSession : lastSession
    readonly property bool hasShown: Utils.hasSession(shownSession)

    property real nowUtc: Utils.nowUtcSeconds()
    readonly property int remaining: hasShown ? Utils.remainingSeconds(shownSession.expiresAtUtc, nowUtc) : 0
    readonly property bool expired: hasShown && (!hasActive || remaining === 0)
    readonly property bool lowTime: hasShown && !expired && remaining < AppStyle.lowTimeSeconds
    readonly property color timeColor: expired ? AppStyle.dangerColor : AppStyle.timeStateColor(remaining)
    readonly property real timeFraction: hasShown && !expired
                                         ? AppStyle.remainingFraction(shownSession.startsAtUtc, shownSession.expiresAtUtc, nowUtc)
                                         : 0

    property bool expiryRefreshDone: false

    property int extendMinutes: Utils.extensionOptions[0]
    property string extendOperationId: ""
    property bool extending: false
    property var extendError: null
    property var extendResult: null

    onActiveSessionChanged: {
        if (hasActive) {
            lastSession = activeSession
            expiryRefreshDone = false
        }
    }

    onRemainingChanged: {
        // Срок вышел: один раз просим сервис завершить сессию и освободить место.
        if (hasActive && remaining === 0 && !expiryRefreshDone && booking !== null) {
            expiryRefreshDone = true
            booking.refresh()
        }
    }

    function resync() {
        nowUtc = Utils.nowUtcSeconds()
        if (booking !== null)
            booking.refresh()
    }

    function setExtendMinutes(minutes) {
        if (extending || minutes === extendMinutes)
            return
        extendMinutes = minutes
        extendOperationId = ""      // другие параметры — новая операция
        extendError = null
    }

    function submitExtend() {
        if (extending || !hasActive || expired || booking === null)
            return
        if (extendOperationId.length === 0)
            extendOperationId = Utils.newOperationId()
        extendError = null
        extendResult = null
        extending = true
        extendTimer.start()
    }

    function performExtend() {
        var result = booking.extend(activeSession.id, extendMinutes, extendOperationId)
        extending = false
        nowUtc = Utils.nowUtcSeconds()
        if (result && result.ok === true && Utils.hasSession(result.data)) {
            extendResult = result.data
            extendOperationId = ""  // следующая кнопка — новая операция
            return
        }
        extendError = result || { ok: false, errorCode: "STORAGE_ERROR",
                                  message: qsTr("Не удалось продлить парковку."), data: {} }
        if (extendError.errorCode === "SESSION_EXPIRED") {
            extendOperationId = ""
            booking.refresh()
        } else if (extendError.errorCode === "IDEMPOTENCY_CONFLICT") {
            extendOperationId = ""
        }
    }

    Component.onCompleted: {
        if (hasActive)
            lastSession = activeSession
        resync()
    }

    onStatusChanged: {
        if (status === PageStatus.Activating)
            resync()
    }

    Connections {
        target: Qt.application
        onStateChanged: {
            if (Qt.application.state === Qt.ApplicationActive)
                page.resync()   // после сна устройства или возврата в приложение
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: page.hasActive && page.status === PageStatus.Active
                 && Qt.application.state === Qt.ApplicationActive
        triggeredOnStart: true
        onTriggered: page.nowUtc = Utils.nowUtcSeconds()
    }

    Timer {
        id: extendTimer
        interval: 1
        repeat: false
        onTriggered: page.performExtend()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        Column {
            id: content
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            spacing: Theme.paddingLarge

            PageHeader {
                x: -Theme.horizontalPageMargin
                width: page.width
                title: qsTr("Парковка")
            }

            StatusPanel {
                visible: !page.hasShown
                title: page.booking === null ? qsTr("Сервис бронирования недоступен")
                                             : qsTr("Активной парковки нет")
                message: qsTr("Выберите парковку на карте, чтобы начать.")
                retryText: qsTr("Найти парковку")
                onRetryRequested: page.backRequested()
            }

            // Главный блок: статус словами, крупный остаток и шкала. Цвет дублирует подпись.
            Column {
                visible: page.hasShown
                width: parent.width
                spacing: Theme.paddingSmall

                Label {
                    width: parent.width
                    text: page.expired ? qsTr("Время истекло")
                                       : page.lowTime ? qsTr("Осталось меньше 15 минут") : qsTr("Идёт парковка")
                    color: page.timeColor
                    font.pixelSize: Theme.fontSizeMedium
                    wrapMode: Text.Wrap
                }

                Label {
                    width: parent.width
                    text: AppStyle.formatRemaining(page.remaining)
                    color: page.expired ? Theme.secondaryHighlightColor : page.timeColor
                    font.pixelSize: Theme.fontSizeHuge
                    font.family: Theme.fontFamilyHeading
                }

                // Шкала оставшегося времени: одна полоса, обновляется тем же секундным таймером.
                Rectangle {
                    id: timeTrack

                    width: parent.width
                    height: Math.max(2, Math.round(Theme.paddingSmall))
                    radius: height / 2
                    color: AppStyle.hairlineColor

                    Rectangle {
                        height: parent.height
                        radius: parent.radius
                        width: parent.width * page.timeFraction
                        color: page.timeColor
                        Behavior on width {
                            enabled: AppStyle.motionEnabled
                            NumberAnimation { duration: AppStyle.motionProgress }
                        }
                    }
                }

                Label {
                    width: parent.width
                    text: (page.expired ? qsTr("закончилась") + " " : qsTr("до") + " ")
                          + AppStyle.formatLocalTime(page.shownSession.expiresAtUtc)
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.Wrap
                }
            }

            DetailRow {
                label: qsTr("Автомобиль")
                value: page.hasShown ? page.shownSession.plate : ""
            }

            DetailRow {
                label: qsTr("Парковка и место")
                value: page.hasShown
                       ? Utils.zoneName(page.parking, page.shownSession.zoneId) + ", "
                         + qsTr("место") + " "
                         + Utils.spaceLabel(page.parking, page.shownSession.zoneId, page.shownSession.spaceId)
                       : ""
            }

            BackgroundItem {
                id: locateItem

                visible: page.hasShown && !!page.shownSession.zoneId
                x: -Theme.horizontalPageMargin
                width: page.width
                height: Theme.itemSizeSmall
                onClicked: page.locateParkingRequested(page.shownSession.zoneId)

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Показать зону на карте")
                    color: locateItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    font.pixelSize: Theme.fontSizeMedium
                    truncationMode: TruncationMode.Fade
                }
            }

            Column {
                visible: page.hasShown
                width: parent.width
                spacing: Theme.paddingSmall / 2

                Label {
                    text: qsTr("Оплачено всего (демо)")
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }

                PriceLabel {
                    width: parent.width
                    amountKopecks: page.hasShown ? page.shownSession.amountKopecks : 0
                    freeText: ""
                }
            }

            Label {
                visible: page.hasShown && !page.expired
                width: parent.width
                text: page.hasShown ? qsTr("Талон") + " " + page.shownSession.ticketNumber : ""
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WrapAnywhere
            }

            // ---- Продление ----
            ComboBox {
                visible: page.hasActive && !page.expired
                x: -Theme.horizontalPageMargin
                width: page.width
                label: qsTr("Продлить на")
                enabled: !page.extending
                currentIndex: Utils.indexOfOption(Utils.extensionOptions, page.extendMinutes)
                menu: ContextMenu {
                    Repeater {
                        model: Utils.extensionOptions
                        MenuItem {
                            text: AppStyle.formatDuration(modelData)
                            onClicked: page.setExtendMinutes(modelData)
                        }
                    }
                }
            }

            Label {
                visible: page.hasActive && !page.expired
                width: parent.width
                text: qsTr("ДЕМО: стоимость продления считается по тарифу этой парковки, деньги не списываются.")
                color: AppStyle.accentColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
            }

            Button {
                visible: page.hasActive && !page.expired && page.extendError === null
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(Theme.buttonWidthLarge, parent.width)
                preferredWidth: width
                text: qsTr("Продлить (демо)")
                enabled: !page.extending
                onClicked: page.submitExtend()
            }

            StatusPanel {
                visible: page.extending
                busy: true
                title: qsTr("Продлеваем…")
                retryText: ""
            }

            StatusPanel {
                visible: !page.extending && page.extendResult !== null && page.extendError === null
                title: page.extendResult
                       ? qsTr("Продлено на") + " " + AppStyle.formatDuration(page.extendResult.additionalMinutes)
                       : ""
                message: page.extendResult
                         ? qsTr("Списано (демо):") + " " + AppStyle.formatKopecks(page.extendResult.chargedKopecks)
                           + ". " + qsTr("Новый срок: до") + " " + AppStyle.formatLocalTime(page.extendResult.expiresAtUtc)
                         : ""
                retryText: ""
            }

            StatusPanel {
                visible: !page.extending && page.extendError !== null
                isError: true
                title: qsTr("Парковка не продлена")
                message: page.extendError && page.extendError.message ? page.extendError.message : ""
                retryText: page.extendError && page.extendError.errorCode !== "SESSION_EXPIRED"
                           && page.extendError.errorCode !== "NOT_OWNER"
                           && page.extendError.errorCode !== "NOT_FOUND"
                           ? qsTr("Повторить") : ""
                onRetryRequested: {
                    page.extendError = null
                    page.submitExtend()     // тот же ключ: повтор не спишет дважды
                }
            }

            Button {
                visible: page.expired
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(Theme.buttonWidthLarge, parent.width)
                preferredWidth: width
                text: qsTr("Найти парковку")
                onClicked: page.backRequested()
            }
        }

        VerticalScrollDecorator {}
    }
}
