// SPDX-License-Identifier: MIT
// Главный экран после демо-входа: активная парковка, поиск, история и профиль.
// Только оформление и переходы; данные берутся из сервисов. Демо-режим подписан.
import QtQuick 2.0
import Sailfish.Silica 1.0
import "../design"
import "../booking/BookingUtils.js" as Utils

Page {
    id: page

    objectName: "mainPage"
    allowedOrientations: Orientation.All

    readonly property var activeSession: typeof bookingService !== "undefined"
                                         ? bookingService.activeSession : ({})
    readonly property bool hasActive: !!activeSession && !!activeSession.id
    readonly property var profile: typeof accountService !== "undefined" ? accountService.profile : ({})
    readonly property var vehicles: typeof accountService !== "undefined" ? accountService.vehicles : []
    readonly property var history: typeof historyService !== "undefined" ? historyService : null
    readonly property var parking: typeof parkingService !== "undefined" ? parkingService : null
    readonly property url confirmationUrl: Qt.resolvedUrl("../booking/ConfirmationPage.qml")

    property real nowUtc: Utils.nowUtcSeconds()
    readonly property int remaining: hasActive ? Utils.remainingSeconds(activeSession.expiresAtUtc, nowUtc) : 0

    function openMap() {
        pageStack.push(Qt.resolvedUrl("../parking/ParkingMapPage.qml"),
                       { "bookingPageUrl": page.confirmationUrl })
    }

    // Обратный отсчёт только пока экран виден и есть активная парковка.
    Timer {
        interval: 1000
        repeat: true
        running: page.hasActive && page.status === PageStatus.Active
                 && Qt.application.state === Qt.ApplicationActive
        triggeredOnStart: true
        onTriggered: page.nowUtc = Utils.nowUtcSeconds()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: "О приложении"
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
        }

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingLarge

            // Шапка: знак приложения и приветствие.
            Item {
                width: parent.width
                height: Theme.itemSizeHuge

                Rectangle {
                    id: badge

                    x: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.itemSizeMedium
                    height: width
                    radius: Theme.paddingMedium
                    color: AppStyle.accentColor

                    Label {
                        anchors.centerIn: parent
                        text: "P"
                        color: Theme.highlightDimmerColor
                        font.pixelSize: Theme.fontSizeExtraLarge
                        font.bold: true
                    }
                }

                Column {
                    anchors {
                        left: badge.right
                        leftMargin: Theme.paddingLarge
                        right: parent.right
                        rightMargin: Theme.horizontalPageMargin
                        verticalCenter: parent.verticalCenter
                    }

                    Label {
                        width: parent.width
                        text: "Умная парковка"
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeLarge
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        objectName: "greeting"
                        width: parent.width
                        text: "Здравствуйте" + (page.profile.displayName
                                                ? ", " + page.profile.displayName
                                                : (page.profile.contact ? ", " + page.profile.contact : ""))
                        color: Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeSmall
                        truncationMode: TruncationMode.Fade
                    }
                }
            }

            // Активная парковка: крупный остаток времени.
            BackgroundItem {
                objectName: "activeParkingCard"
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: activeColumn.height + 2 * Theme.paddingLarge
                visible: page.hasActive
                onClicked: pageStack.push(Qt.resolvedUrl("../booking/ActiveParkingPage.qml"))

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.paddingMedium
                    color: AppStyle.cardColor
                    border.color: AppStyle.timeStateColor(page.remaining)
                    border.width: 2
                }

                Column {
                    id: activeColumn

                    x: Theme.paddingLarge
                    y: Theme.paddingLarge
                    width: parent.width - 2 * Theme.paddingLarge
                    spacing: Theme.paddingSmall

                    Label {
                        text: "Идёт парковка"
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    Label {
                        text: AppStyle.formatRemaining(page.remaining)
                        color: AppStyle.timeStateColor(page.remaining)
                        font.pixelSize: Theme.fontSizeHuge
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.Wrap
                        color: Theme.primaryColor
                        font.pixelSize: Theme.fontSizeSmall
                        text: page.hasActive
                              ? Utils.zoneName(page.parking, page.activeSession.zoneId)
                                + " · до " + AppStyle.formatLocalTime(page.activeSession.expiresAtUtc)
                              : ""
                    }
                }
            }

            // Основное действие.
            BackgroundItem {
                objectName: "findParkingButton"
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: Theme.itemSizeExtraLarge
                onClicked: page.openMap()

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.paddingMedium
                    color: Theme.rgba(AppStyle.accentColor, 0.25)
                    border.color: AppStyle.accentColor
                    border.width: 1
                }

                Image {
                    id: findIcon

                    x: Theme.paddingLarge
                    anchors.verticalCenter: parent.verticalCenter
                    source: "image://theme/icon-m-location?" + Theme.highlightColor
                }
                Column {
                    anchors {
                        left: findIcon.right
                        leftMargin: Theme.paddingLarge
                        right: findArrow.left
                        verticalCenter: parent.verticalCenter
                    }

                    Label {
                        text: "Найти парковку"
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeLarge
                    }
                    Label {
                        width: parent.width
                        text: page.parking ? "Зон рядом: " + page.parking.zones.length + " · карта и список"
                                           : "Карта и список зон"
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                        truncationMode: TruncationMode.Fade
                    }
                }
                Image {
                    id: findArrow

                    anchors {
                        right: parent.right
                        rightMargin: Theme.paddingMedium
                        verticalCenter: parent.verticalCenter
                    }
                    source: "image://theme/icon-m-right?" + Theme.highlightColor
                }
            }

            // Второстепенные разделы.
            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium

                Repeater {
                    model: [
                        {
                            "name": "historyButton",
                            "icon": "image://theme/icon-m-history",
                            "title": "История",
                            "subtitle": page.history && page.history.totalSessions > 0
                                        ? AppStyle.formatKopecks(page.history.totalKopecks) + " · "
                                          + page.history.totalSessions + " шт."
                                        : "Пока пусто",
                            "url": "../history/HistoryPage.qml",
                            "props": {}
                        },
                        {
                            "name": "profileButton",
                            "icon": "image://theme/icon-m-car",
                            "title": "Профиль",
                            "subtitle": page.vehicles.length > 0 ? page.vehicles[0].plate
                                                                 : "Добавьте автомобиль",
                            "url": "../account/ProfilePage.qml",
                            "props": { "signInPage": "" }
                        }
                    ]

                    BackgroundItem {
                        objectName: modelData.name
                        width: (parent.width - Theme.paddingMedium) / 2
                        height: Theme.itemSizeExtraLarge + Theme.paddingLarge
                        onClicked: pageStack.push(Qt.resolvedUrl(modelData.url), modelData.props)

                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.paddingMedium
                            color: AppStyle.cardColor
                        }

                        Column {
                            x: Theme.paddingMedium
                            width: parent.width - 2 * Theme.paddingMedium
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.paddingSmall

                            Image {
                                source: modelData.icon + "?" + Theme.highlightColor
                            }
                            Label {
                                width: parent.width
                                text: modelData.title
                                color: Theme.primaryColor
                                font.pixelSize: Theme.fontSizeMedium
                                truncationMode: TruncationMode.Fade
                            }
                            Label {
                                width: parent.width
                                text: modelData.subtitle
                                color: Theme.secondaryColor
                                font.pixelSize: Theme.fontSizeExtraSmall
                                truncationMode: TruncationMode.Fade
                            }
                        }
                    }
                }
            }

            Label {
                objectName: "demoNotice"
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: "Демо-режим: парковки вымышлены, оплата только симулируется, "
                      + "реквизиты карты не запрашиваются."
            }
        }

        VerticalScrollDecorator {}
    }
}
