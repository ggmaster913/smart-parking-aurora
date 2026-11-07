// SPDX-License-Identifier: MIT
// Демо-вход по email или телефону. SMS/письмо не отправляется: код показан на экране.
// Требует контекстное свойство accountService (docs/CONTRACTS.md).
import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    // Страница после успешного входа; интегратор может переопределить маршрут.
    property url nextPage: Qt.resolvedUrl("ProfilePage.qml")
    property string errorText: ""

    signal signedIn()

    function submit() {
        var result = accountService.demoSignIn(contactField.text, codeField.text)
        if (!result.ok) {
            errorText = result.message
            return
        }
        errorText = ""
        page.signedIn()
        if (nextPage.toString().length > 0)
            pageStack.replace(nextPage)
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: "Вход"
                description: "Демо-режим"
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: "Демо-вход: SMS и письма не отправляются. Введите email или телефон "
                      + "и код 123456. Владение контактом не проверяется."
            }

            TextField {
                id: contactField
                width: parent.width
                label: "Email или телефон"
                placeholderText: "name@example.ru или +7 999 123-45-67"
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: codeField.focus = true
            }

            TextField {
                id: codeField
                width: parent.width
                label: "Демо-код (123456)"
                placeholderText: "Код из демо-подсказки: 123456"
                inputMethodHints: Qt.ImhDigitsOnly
                EnterKey.enabled: contactField.text.length > 0 && text.length > 0
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: page.submit()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: page.errorText.length > 0
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: page.errorText
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Войти (демо)"
                enabled: contactField.text.length > 0 && codeField.text.length > 0
                onClicked: page.submit()
            }
        }

        VerticalScrollDecorator {}
    }
}
