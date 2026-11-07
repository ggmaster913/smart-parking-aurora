import QtQuick 2.0
import Sailfish.Silica 1.0
import "../design"
import "BookingUtils.js" as Utils

// Подтверждение брони с симуляцией оплаты (#5). Контракт BookingService — docs/CONTRACTS.md, #9.
Page {
    id: page

    property string zoneId
    property string spaceId
    property string vehicleId
    property int durationMinutes: 60

    signal ticketReady(var session)
    signal activeParkingRequested()
    signal backRequested()

    readonly property var booking: typeof bookingService !== "undefined" ? bookingService : null
    readonly property var parking: typeof parkingService !== "undefined" ? parkingService : null
    readonly property var account: typeof accountService !== "undefined" ? accountService : null

    // Ключ создаётся при первом нажатии «Оплатить» и живёт, пока не изменились параметры.
    property string operationId: ""
    property bool submitting: false
    property bool completed: false
    property var quoteResult: ({})
    property var bookError: null

    readonly property bool quoteOk: !!quoteResult && quoteResult.ok === true
    readonly property bool canSubmit: booking !== null && quoteOk && !submitting && !completed

    function requote() {
        if (booking === null) {
            quoteResult = { ok: false, errorCode: "STORAGE_ERROR",
                            message: qsTr("Сервис бронирования недоступен."), data: {} }
            return
        }
        quoteResult = booking.quote(zoneId, durationMinutes)
    }

    function setDuration(minutes) {
        if (submitting || completed || minutes === durationMinutes)
            return
        durationMinutes = minutes
        operationId = ""        // другие параметры — новая операция
        bookError = null
        requote()
    }

    function submit() {
        if (!canSubmit)
            return
        if (operationId.length === 0)
            operationId = Utils.newOperationId()
        bookError = null
        submitting = true
        submitTimer.start()     // дать интерфейсу показать загрузку до синхронного вызова
    }

    function performBook() {
        var result = booking.book(zoneId, spaceId, vehicleId, durationMinutes, operationId)
        submitting = false
        if (result && result.ok === true && Utils.hasSession(result.data)) {
            completed = true
            ticketReady(result.data)
            return
        }
        bookError = result || { ok: false, errorCode: "STORAGE_ERROR",
                                message: qsTr("Не удалось выполнить бронирование."), data: {} }
        if (bookError.errorCode === "IDEMPOTENCY_CONFLICT")
            operationId = ""
    }

    function errorActionText(code) {
        if (code === "SPACE_UNAVAILABLE" || code === "NOT_FOUND")
            return qsTr("Выбрать другое место")
        if (code === "ACTIVE_SESSION_EXISTS")
            return qsTr("К активной парковке")
        if (code === "NOT_AUTHENTICATED" || code === "NOT_OWNER" || code === "INVALID_INPUT")
            return ""
        return qsTr("Повторить")
    }

    function onErrorAction(code) {
        if (code === "SPACE_UNAVAILABLE" || code === "NOT_FOUND")
            backRequested()
        else if (code === "ACTIVE_SESSION_EXISTS")
            activeParkingRequested()
        else
            submit()            // тот же operationId: повтор не создаст вторую оплату
    }

    Component.onCompleted: requote()

    Timer {
        id: submitTimer
        interval: 1
        repeat: false
        onTriggered: page.performBook()
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
                title: qsTr("Подтверждение")
            }

            DetailRow {
                label: qsTr("Парковка")
                value: Utils.zoneName(page.parking, page.zoneId)
                valueSize: Theme.fontSizeLarge
            }

            DetailRow {
                label: qsTr("Адрес")
                value: Utils.zoneAddress(page.parking, page.zoneId)
                valueSize: Theme.fontSizeSmall
            }

            DetailRow {
                label: qsTr("Место")
                value: Utils.spaceLabel(page.parking, page.zoneId, page.spaceId)
            }

            DetailRow {
                label: qsTr("Автомобиль")
                value: Utils.vehiclePlate(page.account, page.vehicleId)
            }

            ComboBox {
                x: -Theme.horizontalPageMargin
                width: page.width
                label: qsTr("Длительность")
                enabled: !page.submitting && !page.completed
                currentIndex: Utils.indexOfOption(Utils.durationOptions, page.durationMinutes)
                value: AppStyle.formatDuration(page.durationMinutes)
                menu: ContextMenu {
                    Repeater {
                        model: Utils.durationOptions
                        MenuItem {
                            text: AppStyle.formatDuration(modelData)
                            onClicked: page.setDuration(modelData)
                        }
                    }
                }
            }

            Separator {
                width: parent.width
                color: Theme.primaryColor
                horizontalAlignment: Qt.AlignHCenter
            }

            Column {
                width: parent.width
                spacing: Theme.paddingSmall
                visible: page.quoteOk

                Label {
                    text: qsTr("К оплате")
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeSmall
                }

                PriceLabel {
                    width: parent.width
                    amountKopecks: page.quoteOk ? page.quoteResult.data.amountKopecks : 0
                    freeText: ""
                    font.pixelSize: Theme.fontSizeExtraLarge
                }
            }

            StatusPanel {
                visible: !page.quoteOk
                isError: true
                title: qsTr("Не удалось рассчитать стоимость")
                message: page.quoteResult && page.quoteResult.message ? page.quoteResult.message : ""
                onRetryRequested: page.requote()
            }

            Label {
                width: parent.width
                text: qsTr("ДЕМО: оплата симулируется, деньги не списываются, данные карты не нужны.")
                color: AppStyle.accentColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                preferredWidth: Theme.buttonWidthLarge
                text: qsTr("Оплатить (демо)")
                enabled: page.canSubmit
                visible: !page.completed && page.bookError === null
                onClicked: page.submit()
            }

            StatusPanel {
                visible: page.submitting
                busy: true
                title: qsTr("Оформляем парковку…")
                retryText: ""
            }

            StatusPanel {
                visible: !page.submitting && page.bookError !== null
                isError: true
                title: qsTr("Парковка не оформлена")
                message: page.bookError && page.bookError.message ? page.bookError.message : ""
                retryText: page.bookError ? page.errorActionText(page.bookError.errorCode) : ""
                onRetryRequested: page.onErrorAction(page.bookError.errorCode)
            }
        }

        VerticalScrollDecorator {}
    }
}
