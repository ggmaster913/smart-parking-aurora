import QtQuick 2.0
import Sailfish.Silica 1.0
import "../design"
import "BookingUtils.js" as Utils

// Подтверждение брони с симуляцией оплаты (#5). Контракт BookingService — docs/CONTRACTS.md, #9.
// Карта передаёт zoneId и spaceId. Машину и длительность пользователь выбирает здесь,
// либо страница использует переданные vehicleId/durationMinutes, если они допустимы.
Page {
    id: page

    property string zoneId
    property string spaceId
    property string vehicleId
    property int durationMinutes: Utils.defaultDurationMinutes

    signal ticketReady(var session)
    signal activeParkingRequested()
    signal vehicleRequired()
    signal backRequested()

    readonly property var booking: typeof bookingService !== "undefined" ? bookingService : null
    readonly property var parking: typeof parkingService !== "undefined" ? parkingService : null
    readonly property var account: typeof accountService !== "undefined" ? accountService : null

    readonly property var vehicles: account !== null && account.vehicles ? account.vehicles : []
    readonly property int vehicleCount: vehicles.length
    readonly property bool vehicleValid: Utils.vehicleIndex(vehicles, vehicleId) >= 0
    readonly property bool durationValid: Utils.isValidDuration(durationMinutes)
    property bool vehicleDropped: false
    property string vehicleNotice: ""

    // Ключ создаётся при первом нажатии «Оплатить» и живёт, пока не изменились параметры.
    property string operationId: ""
    property bool submitting: false
    property bool completed: false
    property var quoteResult: ({})
    property var bookError: null

    readonly property bool quoteOk: !!quoteResult && quoteResult.ok === true
    readonly property bool canSubmit: booking !== null && account !== null
                                      && zoneId.length > 0 && spaceId.length > 0
                                      && vehicleValid && durationValid && quoteOk
                                      && !submitting && !completed

    function requote() {
        if (booking === null) {
            quoteResult = { ok: false, errorCode: "STORAGE_ERROR",
                            message: qsTr("Сервис бронирования недоступен."), data: {} }
            return
        }
        quoteResult = booking.quote(zoneId, durationMinutes)
    }

    function resetOperation() {
        operationId = ""        // другие параметры — новая операция
        bookError = null
    }

    // Согласует выбранную машину со списком профиля: сохраняет допустимый выбор,
    // единственную машину выбирает сама, удалённую сбрасывает.
    function syncVehicle() {
        if (submitting || completed)
            return
        var next = Utils.chooseVehicleId(vehicles, vehicleId)
        if (next === vehicleId)
            return
        vehicleDropped = vehicleId.length > 0 && next.length === 0
        vehicleId = next
        resetOperation()
    }

    function selectVehicle(id) {
        if (submitting || completed || id === vehicleId)
            return
        vehicleId = id
        vehicleDropped = false
        vehicleNotice = ""
        resetOperation()
    }

    function setDuration(minutes) {
        if (submitting || completed)
            return
        var next = Utils.normalizeDuration(minutes)
        if (next === durationMinutes)
            return
        durationMinutes = next
        resetOperation()
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
        var error = result || { ok: false, errorCode: "STORAGE_ERROR",
                                message: qsTr("Не удалось выполнить бронирование."), data: {} }
        if (error.errorCode === "IDEMPOTENCY_CONFLICT")
            operationId = ""
        if (error.errorCode === "NOT_FOUND" || error.errorCode === "NOT_OWNER") {
            // NOT_FOUND бывает и для машины, и для места: сверяем список машин заново.
            var before = vehicleId
            if (account !== null)
                account.refresh()
            syncVehicle()
            if (vehicleId !== before || !vehicleValid) {
                // Ошибка касалась машины: показываем новый выбор, оплата — только по новому нажатию.
                vehicleNotice = qsTr("Выбранный автомобиль больше недоступен. Проверьте выбор и оплатите снова.")
                bookError = null
                return
            }
        }
        bookError = error
    }

    function errorActionText(code) {
        if (code === "SPACE_UNAVAILABLE")
            return qsTr("Другое место")
        if (code === "NOT_FOUND")
            return vehicleValid ? qsTr("Другое место") : ""
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

    onVehiclesChanged: syncVehicle()

    Component.onCompleted: {
        durationMinutes = Utils.normalizeDuration(durationMinutes)
        syncVehicle()
        requote()
    }

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

            // ---- Где: зона, адрес и место одной карточкой (#19) ----
            Rectangle {
                width: parent.width
                height: whereColumn.height + 2 * Theme.paddingLarge
                radius: Theme.paddingMedium
                color: AppStyle.cardColor
                border.color: AppStyle.hairlineColor
                border.width: 1

                Column {
                    id: whereColumn

                    x: Theme.paddingLarge
                    y: Theme.paddingLarge
                    width: parent.width - 2 * x
                    spacing: Theme.paddingSmall

                    Label {
                        text: qsTr("Где")
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                    Label {
                        width: parent.width
                        text: Utils.zoneName(page.parking, page.zoneId)
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeLarge
                        font.family: Theme.fontFamilyHeading
                        wrapMode: Text.Wrap
                    }
                    Label {
                        width: parent.width
                        visible: text.length > 0
                        text: Utils.zoneAddress(page.parking, page.zoneId)
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeSmall
                        wrapMode: Text.Wrap
                    }
                    Label {
                        width: parent.width
                        text: qsTr("Место") + " " + Utils.spaceLabel(page.parking, page.zoneId, page.spaceId)
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeMedium
                        wrapMode: Text.Wrap
                    }
                }
            }

            // ---- Автомобиль ----
            StatusPanel {
                visible: page.account === null
                isError: true
                title: qsTr("Профиль недоступен")
                message: qsTr("Не удалось получить список автомобилей. Оплата недоступна.")
                retryText: ""
            }

            StatusPanel {
                visible: page.account !== null && page.vehicleCount === 0
                title: qsTr("Нет автомобиля")
                message: qsTr("Добавьте автомобиль, чтобы оформить парковку.")
                retryText: qsTr("Добавить автомобиль")
                onRetryRequested: page.vehicleRequired()
            }

            ComboBox {
                visible: page.account !== null && page.vehicleCount > 0
                x: -Theme.horizontalPageMargin
                width: page.width
                label: qsTr("Автомобиль")
                enabled: !page.submitting && !page.completed
                currentIndex: Utils.vehicleIndex(page.vehicles, page.vehicleId)
                value: page.vehicleValid ? Utils.vehiclePlate(page.account, page.vehicleId)
                                         : qsTr("Выберите")
                menu: ContextMenu {
                    Repeater {
                        model: page.vehicles
                        MenuItem {
                            text: modelData.brand ? modelData.plate + " · " + modelData.brand
                                                  : modelData.plate
                            onClicked: page.selectVehicle(modelData.id)
                        }
                    }
                }
            }

            Label {
                visible: page.account !== null && page.vehicleCount > 0
                         && (!page.vehicleValid || page.vehicleNotice.length > 0)
                width: parent.width
                text: page.vehicleNotice.length > 0
                      ? page.vehicleNotice
                      : page.vehicleDropped
                        ? qsTr("Выбранный автомобиль больше недоступен. Выберите другой.")
                        : qsTr("Выберите автомобиль, чтобы оплатить.")
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.Wrap
            }

            // ---- Длительность: шаг 15 минут, 15 мин – 24 ч ----
            Column {
                width: parent.width
                spacing: Theme.paddingSmall

                Label {
                    text: qsTr("На сколько")
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }

                Row {
                    width: parent.width
                    spacing: Theme.paddingSmall

                    Button {
                        id: minusButton
                        width: Math.min(Theme.buttonWidthExtraSmall, (parent.width - 2 * parent.spacing) / 3)
                        preferredWidth: width
                        text: qsTr("−15 мин")
                        enabled: !page.submitting && !page.completed
                                 && page.durationMinutes > Utils.minDurationMinutes
                        onClicked: page.setDuration(Utils.stepDuration(page.durationMinutes, -1))
                    }

                    Label {
                        width: parent.width - 2 * minusButton.width - 2 * parent.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        horizontalAlignment: Text.AlignHCenter
                        text: AppStyle.formatDuration(page.durationMinutes)
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeMedium
                        wrapMode: Text.Wrap
                    }

                    Button {
                        width: minusButton.width
                        preferredWidth: width
                        text: qsTr("+15 мин")
                        enabled: !page.submitting && !page.completed
                                 && page.durationMinutes < Utils.maxDurationMinutes
                        onClicked: page.setDuration(Utils.stepDuration(page.durationMinutes, 1))
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
                    text: qsTr("Итого к оплате")
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeSmall
                }

                PriceLabel {
                    width: parent.width
                    amountKopecks: page.quoteOk ? page.quoteResult.data.amountKopecks : 0
                    freeText: ""
                    font.pixelSize: Theme.fontSizeExtraLarge
                }

                // Краткое резюме выбора под итогом: время и машина.
                Label {
                    width: parent.width
                    text: AppStyle.formatDuration(page.durationMinutes)
                          + (page.vehicleValid ? " · " + Utils.vehiclePlate(page.account, page.vehicleId) : "")
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.Wrap
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

            // Невидимый образец: сумма в кнопке, только если подпись помещается целиком.
            Label {
                id: payProbe
                visible: false
                font.pixelSize: Theme.fontSizeMedium
                text: page.quoteOk
                      ? qsTr("Оплатить") + " " + AppStyle.formatKopecks(page.quoteResult.data.amountKopecks) + " " + qsTr("(демо)")
                      : ""
            }

            Button {
                id: payButton
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(Theme.buttonWidthLarge, parent.width)
                preferredWidth: width
                text: page.quoteOk && payProbe.implicitWidth + 2 * Theme.paddingLarge <= payButton.width
                      ? payProbe.text : qsTr("Оплатить (демо)")
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
