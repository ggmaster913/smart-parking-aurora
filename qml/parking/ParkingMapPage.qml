// SPDX-License-Identifier: MIT
//
// Карта зон парковки на MFW (подключение по официальному MapTemplate/MfwMap).
// Подложка — растровые тайлы OpenStreetMap без ключа доступа, с атрибуцией.
// Маркер открывает карточку зоны: адрес, тариф, часы работы, свободные места.
import QtQuick 2.0
import Sailfish.Silica 1.0
import QtPositioning 5.2
import MfwMap 1.0
import "ParkingFormat.js" as F
import "../design"

Page {
    id: page

    objectName: "parkingMapPage"
    allowedOrientations: Orientation.All

    // Страница подтверждения бронирования (задаёт интегратор). Получает
    // свойства zoneId и spaceId.
    property url bookingPageUrl

    property var selectedZone: ({})
    // id объекта карты (MfwMapObject.id) -> id зоны. Маркер своей позиции сюда не входит.
    property var pinZones: ({})
    property bool hasSelection: selectedZone.id !== undefined
    property string mapError: ""

    readonly property string iconFree: Qt.resolvedUrl("icons/pin-free.svg")
    readonly property string iconFull: Qt.resolvedUrl("icons/pin-full.svg")
    readonly property string iconMe: Qt.resolvedUrl("icons/pin-me.svg")
    readonly property string iconDemo: Qt.resolvedUrl("icons/pin-demo.svg")

    function zoneById(zoneId) {
        var zones = parkingService.zones;
        for (var i = 0; i < zones.length; ++i) {
            if (zones[i].id === zoneId)
                return zones[i];
        }
        return null;
    }

    function selectZone(zone) {
        selectedZone = zone ? zone : ({});
    }

    function rebuildPins() {
        if (!map)
            return;
        map.clearMapObjects();
        var zones = parkingService.zones;
        var mapping = {};
        for (var i = 0; i < zones.length; ++i) {
            var pin = pinComponent.createObject(page);
            map.addMapObject(pin);
            pin.locationAnchor = MfwPinMapObject.Bottom;
            pin.source = zones[i].freeSpaces > 0 ? page.iconFree : page.iconFull;
            pin.location = QtPositioning.coordinate(zones[i].latitude, zones[i].longitude);
            mapping[pin.id] = zones[i].id;
        }
        pinZones = mapping;
        if (parkingService.hasPosition && positionPin.latitude !== undefined) {
            var me = pinComponent.createObject(page);
            map.addMapObject(me);
            me.locationAnchor = MfwPinMapObject.Center;
            me.source = parkingService.positionIsDemo ? page.iconDemo : page.iconMe;
            me.location = QtPositioning.coordinate(positionPin.latitude, positionPin.longitude);
        }
        if (hasSelection) {
            var fresh = zoneById(selectedZone.id);
            selectZone(fresh);
        }
    }

    QtObject {
        id: positionPin

        property var latitude
        property var longitude
    }

    Component {
        id: pinComponent

        MfwPinMapObject {}
    }

    PositionTracker {
        id: tracker

        onPositionApplied: {
            var first = positionPin.latitude === undefined;
            positionPin.latitude = latitude;
            positionPin.longitude = longitude;
            if (first || isDemo)
                map.center = QtPositioning.coordinate(latitude, longitude);
            page.rebuildPins();
        }
    }

    Connections {
        target: parkingService
        onZonesChanged: page.rebuildPins()
    }

    // Доступность мест перечитывается при возврате на страницу и в приложение,
    // а не только по новым координатам.
    Connections {
        target: Qt.application
        onStateChanged: {
            if (Qt.application.state === Qt.ApplicationActive)
                parkingService.refresh();
        }
    }

    onStatusChanged: {
        if (status === PageStatus.Active)
            parkingService.refresh();
    }

    PageHeader {
        id: header

        objectName: "pageHeader"
        title: "Парковки рядом"
        description: parkingService.positionIsDemo ? "Демо-местоположение" : ""
    }

    MfwMap {
        id: map

        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        center: QtPositioning.coordinate(F.demoLatitude, F.demoLongitude)
        zoomLevel: 13.0
        // Явные границы для кнопок масштаба: растровые тайлы OSM есть до 19 уровня.
        minimumZoomLevel: 3.0
        maximumZoomLevel: 19.0
        metersPerPixelTolerance: 0.1
        pixelRatio: Theme.pixelRatio
        cacheDatabaseStoreSettings: true
        styleJson: F.osmStyleJson

        onGestureInProgressChanged: {
            page.forwardNavigation = !gestureInProgress;
            page.backNavigation = !gestureInProgress;
        }

        // Зона определяется только по id нажатого маркера; нажатие на свою позицию
        // или неизвестный объект ничего не выбирает.
        onMapObjectClicked: {
            var zone = page.pinZones.hasOwnProperty(id) ? page.zoneById(page.pinZones[id]) : null;
            if (zone)
                page.selectZone(zone);
        }
        onError: page.mapError = message

        Component.onCompleted: {
            addImagePath(page.iconFree, page.iconFree, 40, 52);
            addImagePath(page.iconFull, page.iconFull, 40, 52);
            addImagePath(page.iconMe, page.iconMe, 32, 32);
            addImagePath(page.iconDemo, page.iconDemo, 32, 32);
            page.rebuildPins();
        }

        // Обязательная атрибуция данных OpenStreetMap.
        Label {
            id: osmLabel

            anchors {
                right: parent.right
                bottom: parent.bottom
                margins: Theme.paddingSmall
            }
            text: F.osmAttribution
            font.pixelSize: Theme.fontSizeTiny
            color: "#202124"

            Rectangle {
                anchors.fill: parent
                anchors.margins: -Theme.paddingSmall / 2
                z: -1
                color: "#ccffffff"
            }
        }
    }

    // Кнопки масштаба дублируют жест щипка (свойство MfwMap.zoomLevel, границы —
    // minimumZoomLevel/maximumZoomLevel карты).
    Column {
        id: zoomControls

        anchors {
            right: parent.right
            verticalCenter: map.verticalCenter
            rightMargin: Theme.paddingMedium
        }
        spacing: Theme.paddingMedium
        z: 1

        Repeater {
            model: [
                { "label": "+", "step": 1, "name": "zoomInButton" },
                { "label": "−", "step": -1, "name": "zoomOutButton" }
            ]

            Rectangle {
                objectName: modelData.name
                width: Theme.itemSizeSmall
                height: Theme.itemSizeSmall
                radius: width / 2
                color: Theme.rgba(Theme.highlightDimmerColor, 0.9)
                opacity: zoomArea.enabled ? 1.0 : 0.4

                Label {
                    anchors.centerIn: parent
                    text: modelData.label
                    font.pixelSize: Theme.fontSizeLarge
                }

                MouseArea {
                    id: zoomArea

                    anchors.fill: parent
                    enabled: modelData.step > 0 ? map.zoomLevel < map.maximumZoomLevel
                                                : map.zoomLevel > map.minimumZoomLevel
                    onClicked: {
                        var level = Math.round(map.zoomLevel) + modelData.step;
                        level = Math.max(map.minimumZoomLevel, Math.min(map.maximumZoomLevel, level));
                        map.zoomLevel = level;
                    }
                }
            }
        }
    }

    // Состояние геопозиции и ошибки подложки поверх карты.
    Column {
        id: statusPanel

        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            margins: Theme.paddingMedium
        }
        spacing: Theme.paddingSmall
        // Не зависит от hasPosition: после потери сигнала координата остаётся,
        // но пользователь должен видеть, что она устарела.
        visible: tracker.status !== "ok" || tracker.useDemo || page.mapError.length > 0

        Rectangle {
            width: parent.width
            height: statusColumn.height + 2 * Theme.paddingMedium
            radius: Theme.paddingSmall
            color: Theme.rgba(Theme.highlightDimmerColor, 0.9)

            Column {
                id: statusColumn

                x: Theme.paddingMedium
                y: Theme.paddingMedium
                width: parent.width - 2 * Theme.paddingMedium
                spacing: Theme.paddingSmall

                Label {
                    width: parent.width
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeSmall
                    text: tracker.statusText
                }
                Label {
                    width: parent.width
                    wrapMode: Text.Wrap
                    visible: page.mapError.length > 0
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: "Подложка карты недоступна: " + page.mapError
                          + ". Список парковок работает без неё."
                }
                // Короткие подписи (DESIGN.md): кнопка не шире панели на узком экране.
                Button {
                    visible: !tracker.useDemo && tracker.status !== "ok"
                    width: Math.min(Theme.buttonWidthLarge, parent.width)
                    preferredWidth: width
                    text: "Демо-местоположение"
                    onClicked: tracker.enableDemo()
                }
                Button {
                    visible: tracker.useDemo && tracker.hasRealFix
                    width: Math.min(Theme.buttonWidthLarge, parent.width)
                    preferredWidth: width
                    text: "Моя геопозиция"
                    onClicked: tracker.useRealPosition()
                }
            }
        }
    }

    // Карточка выбранной зоны (#19): название, адрес, крупная цена, места и одно действие.
    // Только оформление: выбор зоны, переход и данные — прежние.
    Rectangle {
        id: zoneCard

        // Последняя выбранная зона: содержимое не пропадает во время исчезновения карточки.
        property var zone: ({})
        readonly property bool shown: page.hasSelection
        readonly property int free: zone.freeSpaces !== undefined ? zone.freeSpaces : 0

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: Theme.paddingMedium
            // Над атрибуцией OpenStreetMap, чтобы не перекрывать её.
            bottomMargin: osmLabel.height + 3 * Theme.paddingSmall
        }
        height: cardColumn.height + 2 * Theme.paddingLarge
        radius: Theme.paddingMedium
        color: AppStyle.overlayCardColor
        border.color: AppStyle.hairlineColor
        border.width: 1
        enabled: shown
        opacity: shown ? 1 : 0
        visible: opacity > 0

        transform: Translate {
            y: zoneCard.shown ? 0 : Theme.paddingLarge * 2
            Behavior on y { NumberAnimation { duration: AppStyle.motionCard; easing.type: Easing.OutCubic } }
        }
        Behavior on opacity { NumberAnimation { duration: AppStyle.motionCard } }

        // Короткий акцент при выборе другой зоны.
        SequentialAnimation {
            id: selectAccent
            NumberAnimation { target: zoneCard; property: "scale"; to: 1.02; duration: AppStyle.motionAccent / 2 }
            NumberAnimation { target: zoneCard; property: "scale"; to: 1.0; duration: AppStyle.motionAccent / 2 }
        }

        Connections {
            target: page
            onSelectedZoneChanged: {
                if (!page.hasSelection)
                    return
                var changed = zoneCard.zone.id !== page.selectedZone.id
                zoneCard.zone = page.selectedZone
                if (changed && AppStyle.motionEnabled && zoneCard.opacity === 1)
                    selectAccent.restart()
            }
        }

        Column {
            id: cardColumn

            x: Theme.paddingLarge
            y: Theme.paddingLarge
            width: parent.width - 2 * Theme.paddingLarge
            spacing: Theme.paddingSmall

            Item {
                width: parent.width
                height: Math.max(titleColumn.height, closeButton.height)

                Column {
                    id: titleColumn

                    width: parent.width - closeButton.width - Theme.paddingSmall
                    spacing: Theme.paddingSmall / 2

                    Label {
                        width: parent.width
                        wrapMode: Text.Wrap
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeLarge
                        font.family: Theme.fontFamilyHeading
                        text: zoneCard.zone.name || ""
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.Wrap
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeSmall
                        text: zoneCard.zone.address || ""
                    }
                }

                IconButton {
                    id: closeButton

                    anchors.right: parent.right
                    icon.source: "image://theme/icon-m-clear"
                    onClicked: page.selectZone(null)
                }
            }

            // Главное число — цена; расстояние вторично.
            Item {
                width: parent.width
                height: Math.max(priceLabel.height, distanceLabel.height)

                PriceLabel {
                    id: priceLabel

                    width: parent.width - distanceLabel.width - Theme.paddingMedium
                    amountKopecks: zoneCard.zone.rateKopecksPerHour !== undefined ? zoneCard.zone.rateKopecksPerHour : 0
                    suffix: "/ ч"
                    font.pixelSize: Theme.fontSizeExtraLarge
                }
                Label {
                    id: distanceLabel

                    anchors {
                        right: parent.right
                        baseline: priceLabel.baseline
                    }
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeSmall
                    text: zoneCard.zone.distanceMeters >= 0
                          ? AppStyle.formatDistance(zoneCard.zone.distanceMeters) + " по прямой" : ""
                }
            }

            // Статус мест: цвет всегда вместе с подписью.
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeMedium
                color: zoneCard.free > 3 ? AppStyle.accentColor
                                         : zoneCard.free > 0 ? AppStyle.warningColor : Theme.secondaryColor
                text: AppStyle.freeSpacesText(zoneCard.free, zoneCard.zone.capacity)
            }
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: zoneCard.zone.workingHours || ""
            }

            Item { width: 1; height: Theme.paddingSmall }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(Theme.buttonWidthLarge, parent.width)
                preferredWidth: width
                text: "Выбрать место"
                onClicked: pageStack.push(Qt.resolvedUrl("ZoneDetailsPage.qml"), {
                                              "zoneId": page.selectedZone.id,
                                              "bookingPageUrl": page.bookingPageUrl
                                          })
            }
        }
    }

    // Плотная подложка кнопки списка: на светлых тайлах OSM прозрачная кнопка Silica
    // почти не читалась (снимок e2e-02 в #28). Только оформление.
    Rectangle {
        anchors.fill: listButton
        anchors.margins: -Theme.paddingSmall / 2
        radius: height / 2
        color: AppStyle.overlayCardColor
        border.color: AppStyle.hairlineColor
        border.width: 1
        visible: listButton.visible
    }

    Button {
        id: listButton

        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: osmLabel.height + 3 * Theme.paddingLarge
        }
        visible: !page.hasSelection
        text: "Список парковок (" + parkingService.zones.length + ")"
        onClicked: pageStack.push(Qt.resolvedUrl("ZoneListPage.qml"), {
                                      "tracker": tracker,
                                      "bookingPageUrl": page.bookingPageUrl
                                  })
    }

}
