// SPDX-License-Identifier: MIT
// Ошибка запуска (#28): база данных или импорт каталога недоступны. Временная БД не
// подставляется; пользователь видит причину и может повторить попытку.
import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    objectName: "startupErrorPage"
    allowedOrientations: Orientation.All
    backNavigation: false

    Column {
        width: parent.width
        spacing: Theme.paddingLarge

        PageHeader {
            title: "Умная парковка"
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            wrapMode: Text.Wrap
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeLarge
            text: "Не удалось подготовить данные"
        }

        Label {
            objectName: "startupErrorText"
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            wrapMode: Text.Wrap
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
            text: appStartup.errorText
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Повторить"
            onClicked: appStartup.retry()
        }
    }
}
