// SPDX-License-Identifier: MIT
// Профиль демо-пользователя и его автомобили.
// Требует контекстное свойство accountService (docs/CONTRACTS.md).
import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    // Страница после выхода; интегратор может переопределить маршрут.
    property url signInPage: Qt.resolvedUrl("SignInPage.qml")
    property string profileMessage: ""
    property string vehicleMessage: ""

    signal signedOut()

    function saveName() {
        var result = accountService.saveProfile(nameField.text)
        profileMessage = result.ok ? "Имя сохранено" : result.message
    }

    function addVehicle() {
        var result = accountService.addVehicle(plateField.text, brandField.text)
        if (!result.ok) {
            vehicleMessage = result.message
            return
        }
        vehicleMessage = "Автомобиль " + result.data.vehicle.plate + " добавлен"
        plateField.text = ""
        brandField.text = ""
    }

    function signOut() {
        accountService.signOut()
        page.signedOut()
        if (signInPage.toString().length > 0)
            pageStack.replace(signInPage)
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: "Выйти"
                onClicked: page.signOut()
            }
        }

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: "Профиль"
                description: accountService.profile.contact || ""
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: "Демо-профиль хранится только на этом устройстве."
            }

            TextField {
                id: nameField
                width: parent.width
                label: "Имя"
                placeholderText: "Как к вам обращаться"
                text: accountService.profile.displayName || ""
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: page.saveName()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Сохранить имя"
                onClicked: page.saveName()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: page.profileMessage.length > 0
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: page.profileMessage
            }

            SectionHeader {
                text: "Мои автомобили"
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: accountService.vehicles.length === 0
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                text: "Автомобилей пока нет. Добавьте госномер и марку."
            }

            Repeater {
                model: accountService.vehicles

                delegate: Column {
                    x: Theme.horizontalPageMargin
                    width: column.width - 2 * Theme.horizontalPageMargin

                    Label {
                        width: parent.width
                        text: modelData.plate
                        color: Theme.primaryColor
                        font.pixelSize: Theme.fontSizeLarge
                    }
                    Label {
                        width: parent.width
                        text: modelData.brand
                        color: Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeSmall
                        truncationMode: TruncationMode.Fade
                    }
                }
            }

            SectionHeader {
                text: "Добавить автомобиль"
            }

            TextField {
                id: plateField
                width: parent.width
                label: "Госномер"
                placeholderText: "Например, А123ВС77"
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: brandField.focus = true
            }

            TextField {
                id: brandField
                width: parent.width
                label: "Марка"
                placeholderText: "Например, Lada Vesta"
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: page.addVehicle()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Добавить"
                enabled: plateField.text.length > 0 && brandField.text.length > 0
                onClicked: page.addVehicle()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: page.vehicleMessage.length > 0
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: page.vehicleMessage
            }
        }

        VerticalScrollDecorator {}
    }
}
