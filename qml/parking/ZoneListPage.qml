// SPDX-License-Identifier: MIT
//
// Список зон: ближайшие по прямому расстоянию, затем по ID (порядок задаёт
// ParkingService). Работает и без подложки карты — резервный режим.
import QtQuick 2.0
import Sailfish.Silica 1.0
import "ParkingFormat.js" as F

Page {
    id: page

    objectName: "zoneListPage"
    allowedOrientations: Orientation.All

    property var tracker: null
    property url bookingPageUrl

    function enableDemo() {
        if (tracker)
            tracker.enableDemo();
        else
            parkingService.setPosition(F.demoLatitude, F.demoLongitude, true);
    }

    SilicaListView {
        id: list

        anchors.fill: parent
        model: parkingService.zones

        PullDownMenu {
            MenuItem {
                text: "Обновить"
                onClicked: parkingService.refresh()
            }
            MenuItem {
                visible: !parkingService.positionIsDemo
                         && !(page.tracker && page.tracker.hasRealFix)
                text: "Использовать демо-местоположение"
                onClicked: page.enableDemo()
            }
        }

        header: Column {
            width: list.width

            PageHeader {
                title: "Парковки"
                description: parkingService.hasPosition
                             ? (parkingService.positionIsDemo
                                ? "Ближайшие к демо-местоположению"
                                : "Ближайшие к вам")
                             : "Местоположение неизвестно — порядок по названию"
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                visible: text.length > 0
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: page.tracker ? page.tracker.statusText
                                   : (parkingService.positionIsDemo
                                      ? "Используется демо-местоположение (не ваше реальное)" : "")
            }
            Item {
                width: 1
                height: Theme.paddingMedium
            }
        }

        delegate: ListItem {
            id: item

            contentHeight: column.height + 2 * Theme.paddingMedium
            opacity: modelData.freeSpaces > 0 ? 1.0 : 0.6

            onClicked: pageStack.push(Qt.resolvedUrl("ZoneDetailsPage.qml"), {
                                          "zoneId": modelData.id,
                                          "bookingPageUrl": page.bookingPageUrl
                                      })

            Column {
                id: column

                x: Theme.horizontalPageMargin
                y: Theme.paddingMedium
                width: parent.width - 2 * Theme.horizontalPageMargin

                Item {
                    width: parent.width
                    height: nameLabel.height

                    Label {
                        id: nameLabel

                        width: parent.width - distanceLabel.width - Theme.paddingMedium
                        truncationMode: TruncationMode.Fade
                        color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                        text: modelData.name
                    }
                    Label {
                        id: distanceLabel

                        anchors.right: parent.right
                        color: Theme.highlightColor
                        text: F.distance(modelData.distanceMeters)
                    }
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                    text: modelData.address
                }
                Label {
                    width: parent.width
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: modelData.freeSpaces > 0 ? Theme.primaryColor : Theme.secondaryColor
                    text: F.rate(modelData.rateKopecksPerHour) + " · " + modelData.workingHours
                          + " · " + F.freeSpaces(modelData.freeSpaces, modelData.capacity)
                }
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: "Парковки не загружены"
            hintText: "Потяните вниз, чтобы обновить"
        }

        VerticalScrollDecorator {}
    }
}
