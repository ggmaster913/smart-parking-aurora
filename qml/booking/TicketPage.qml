import QtQuick 2.0
import Sailfish.Silica 1.0
import "../design"
import "BookingUtils.js" as Utils

// Электронный DEMO-талон (#5). Показывает только данные, пришедшие из BookingService.
Page {
    id: page

    property var session: ({})

    signal activeParkingRequested()
    signal backRequested()

    readonly property var parking: typeof parkingService !== "undefined" ? parkingService : null
    readonly property bool valid: Utils.hasSession(session)

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
                title: qsTr("Талон")
            }

            StatusPanel {
                visible: !page.valid
                isError: true
                title: qsTr("Талон недоступен")
                message: qsTr("Нет данных о парковке. Оформите парковку заново.")
                retryText: qsTr("Назад")
                onRetryRequested: page.backRequested()
            }

            Rectangle {
                visible: page.valid
                width: parent.width
                height: ticket.height + 2 * Theme.paddingLarge
                radius: Theme.paddingMedium
                color: AppStyle.cardColor

                Column {
                    id: ticket
                    x: Theme.paddingLarge
                    y: Theme.paddingLarge
                    width: parent.width - 2 * x
                    spacing: Theme.paddingMedium

                    Label {
                        width: parent.width
                        text: page.valid ? page.session.ticketNumber : ""
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeLarge
                        font.family: Theme.fontFamilyHeading
                        wrapMode: Text.WrapAnywhere
                    }

                    Label {
                        width: parent.width
                        text: qsTr("Демонстрационный талон. Не является платёжным документом.")
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                        wrapMode: Text.Wrap
                    }

                    DetailRow {
                        label: qsTr("Автомобиль")
                        value: page.valid ? page.session.plate : ""
                    }

                    DetailRow {
                        label: qsTr("Парковка")
                        value: page.valid ? Utils.zoneName(page.parking, page.session.zoneId) : ""
                    }

                    DetailRow {
                        label: qsTr("Место")
                        value: page.valid
                               ? Utils.spaceLabel(page.parking, page.session.zoneId, page.session.spaceId)
                               : ""
                    }

                    DetailRow {
                        label: qsTr("Начало")
                        value: page.valid ? AppStyle.formatLocalTime(page.session.startsAtUtc) : ""
                    }

                    DetailRow {
                        label: qsTr("Действует до")
                        value: page.valid ? AppStyle.formatLocalTime(page.session.expiresAtUtc) : ""
                        valueColor: AppStyle.accentColor
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.paddingSmall / 2

                        Label {
                            text: page.valid && page.session.paymentIsDemo === true
                                  ? qsTr("Оплачено (демо)") : qsTr("Оплачено")
                            color: Theme.secondaryHighlightColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                        }

                        PriceLabel {
                            width: parent.width
                            amountKopecks: page.valid ? page.session.amountKopecks : 0
                            freeText: ""
                        }
                    }
                }
            }

            Button {
                visible: page.valid
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(Theme.buttonWidthLarge, parent.width)
                preferredWidth: width
                text: qsTr("К активной парковке")
                onClicked: page.activeParkingRequested()
            }
        }

        VerticalScrollDecorator {}
    }
}
