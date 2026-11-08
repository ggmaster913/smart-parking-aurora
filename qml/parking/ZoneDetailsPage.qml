// SPDX-License-Identifier: MIT
//
// Карточка зоны и выбор места. Дальше в бронирование передаются только
// идентификаторы zoneId и spaceId (не индекс строки).
import QtQuick 2.0
import Sailfish.Silica 1.0
import "ParkingFormat.js" as F
import "../design"

Page {
    id: page

    objectName: "zoneDetailsPage"
    allowedOrientations: Orientation.All

    property string zoneId
    property url bookingPageUrl

    property var zone: ({})
    property var spaceList: []
    property string errorText: ""
    property string selectedSpaceId: ""
    property string selectedSpaceLabel: ""

    function reload() {
        var result = parkingService.zoneDetails(zoneId);
        if (!result.ok) {
            errorText = result.message;
            zone = ({});
            spaceList = [];
            return;
        }
        errorText = "";
        zone = result.data;
        spaceList = parkingService.spaces(zoneId);

        // Выбранное место могло стать недоступным после обновления.
        var stillAvailable = false;
        for (var i = 0; i < spaceList.length; ++i) {
            if (spaceList[i].id === selectedSpaceId && spaceList[i].available)
                stillAvailable = true;
        }
        if (!stillAvailable) {
            selectedSpaceId = "";
            selectedSpaceLabel = "";
        }
    }

    Connections {
        target: parkingService
        onZonesChanged: page.reload()
    }

    Component.onCompleted: reload()

    // При возврате на карточку доступность перечитывается из БД.
    onStatusChanged: {
        if (status === PageStatus.Active)
            parkingService.refresh();
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: "Обновить"
                onClicked: parkingService.refresh()
            }
        }

        Column {
            id: content

            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: page.zone.name !== undefined ? page.zone.name : "Парковка"
                description: "Демо-данные"
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: page.errorText.length > 0
                wrapMode: Text.Wrap
                color: Theme.errorColor
                text: page.errorText
            }

            Column {
                width: parent.width
                visible: page.errorText.length === 0

                DetailItem {
                    label: "Адрес"
                    value: page.zone.address !== undefined ? page.zone.address : ""
                }
                DetailItem {
                    label: "Тариф"
                    value: F.rate(page.zone.rateKopecksPerHour)
                }
                DetailItem {
                    label: "Часы работы"
                    value: page.zone.workingHours !== undefined ? page.zone.workingHours : ""
                }
                DetailItem {
                    label: "Места"
                    value: F.freeSpaces(page.zone.freeSpaces, page.zone.capacity)
                }
                DetailItem {
                    label: "Расстояние по прямой"
                    value: page.zone.distanceMeters >= 0
                           ? F.distance(page.zone.distanceMeters)
                           : "местоположение неизвестно"
                }
            }

            SectionHeader {
                visible: page.errorText.length === 0
                text: "Выберите место"
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: page.errorText.length === 0 && page.zone.freeSpaces === 0
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                text: "Свободных мест нет. Выберите другую парковку."
            }

            Grid {
                id: grid

                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                columns: Math.max(1, Math.floor(width / (Theme.itemSizeLarge + spacing)))
                spacing: Theme.paddingMedium
                visible: page.errorText.length === 0

                Repeater {
                    model: page.spaceList

                    BackgroundItem {
                        id: cell

                        width: Theme.itemSizeLarge
                        height: Theme.itemSizeLarge
                        enabled: modelData.available
                        highlighted: down || page.selectedSpaceId === modelData.id

                        onClicked: {
                            page.selectedSpaceId = modelData.id;
                            page.selectedSpaceLabel = modelData.label;
                            // Оформление (#19): один короткий акцент выбранной ячейки.
                            if (AppStyle.motionEnabled)
                                cellAccent.restart();
                        }

                        SequentialAnimation {
                            id: cellAccent
                            NumberAnimation { target: cell; property: "scale"; to: 1.06; duration: AppStyle.motionAccent / 2 }
                            NumberAnimation { target: cell; property: "scale"; to: 1.0; duration: AppStyle.motionAccent / 2 }
                        }

                        Rectangle {
                            readonly property bool selected: page.selectedSpaceId === modelData.id

                            anchors.fill: parent
                            radius: Theme.paddingSmall
                            color: selected ? AppStyle.cardColor : "transparent"
                            border.width: selected ? 3 : 2
                            border.color: modelData.available
                                          ? (selected ? AppStyle.accentColor : Theme.primaryColor)
                                          : Theme.secondaryColor
                            opacity: modelData.available ? 1.0 : 0.4
                        }
                        Label {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: modelData.available
                                   ? (cell.highlighted ? Theme.highlightColor : Theme.primaryColor)
                                   : Theme.secondaryColor
                        }
                        Label {
                            anchors {
                                bottom: parent.bottom
                                horizontalCenter: parent.horizontalCenter
                                bottomMargin: Theme.paddingSmall
                            }
                            visible: !modelData.available
                            font.pixelSize: Theme.fontSizeTiny
                            color: Theme.secondaryColor
                            text: "занято"
                        }
                    }
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.errorText.length === 0
                enabled: page.selectedSpaceId.length > 0 && page.bookingPageUrl.toString().length > 0
                text: page.selectedSpaceId.length > 0
                      ? "Продолжить с местом " + page.selectedSpaceLabel
                      : "Выберите свободное место"
                onClicked: pageStack.push(page.bookingPageUrl, {
                                              "zoneId": page.zoneId,
                                              "spaceId": page.selectedSpaceId
                                          })
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: page.bookingPageUrl.toString().length === 0
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: "Страница бронирования ещё не подключена в навигации."
            }
        }

        VerticalScrollDecorator {}
    }
}
