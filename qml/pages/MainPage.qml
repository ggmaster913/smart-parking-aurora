// SPDX-License-Identifier: MIT
// Главный экран после демо-входа (#28): переходы к карте, активной парковке, истории
// и профилю. Демо-режим и симуляция оплаты подписаны.
import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    objectName: "mainPage"
    allowedOrientations: Orientation.All

    readonly property var activeSession: typeof bookingService !== "undefined"
                                         ? bookingService.activeSession : ({})
    readonly property bool hasActive: !!activeSession && !!activeSession.id
    readonly property url confirmationUrl: Qt.resolvedUrl("../booking/ConfirmationPage.qml")

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

            PageHeader {
                objectName: "pageHeader"
                title: "Умная парковка"
                description: accountService.profile.displayName || accountService.profile.contact || ""
            }

            Label {
                objectName: "demoNotice"
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: "Демо-режим: парковки вымышлены, оплата только симулируется, "
                      + "реквизиты карты не запрашиваются."
            }

            Button {
                objectName: "activeParkingButton"
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.hasActive
                text: "Активная парковка"
                onClicked: pageStack.push(Qt.resolvedUrl("../booking/ActiveParkingPage.qml"))
            }

            Button {
                objectName: "findParkingButton"
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Найти парковку"
                onClicked: pageStack.push(Qt.resolvedUrl("../parking/ParkingMapPage.qml"),
                                          { "bookingPageUrl": page.confirmationUrl })
            }

            Button {
                objectName: "historyButton"
                anchors.horizontalCenter: parent.horizontalCenter
                text: "История и расходы"
                onClicked: pageStack.push(Qt.resolvedUrl("../history/HistoryPage.qml"))
            }

            Button {
                objectName: "profileButton"
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Профиль и автомобили"
                onClicked: pageStack.push(Qt.resolvedUrl("../account/ProfilePage.qml"),
                                          { "signInPage": "" })
            }
        }

        VerticalScrollDecorator {}
    }
}
