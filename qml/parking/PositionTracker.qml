// SPDX-License-Identifier: MIT
//
// Получение координат через Qt Positioning (разрешение Location в .desktop)
// и передача их в parkingService. При запрете/ошибке GPS пользователь может
// явно выбрать демо-местоположение; оно помечается как демо.
import QtQuick 2.0
import QtPositioning 5.2
import "ParkingFormat.js" as F

Item {
    id: tracker

    // waiting — ждём координаты; ok — есть реальная позиция;
    // denied — нет разрешения; unavailable — источник недоступен или не ответил.
    property string status: "waiting"
    property bool useDemo: false
    property alias active: source.active
    readonly property bool hasRealFix: status === "ok"

    readonly property string statusText: {
        if (useDemo)
            return "Используется демо-местоположение (не ваше реальное)";
        switch (status) {
        case "ok": return "Местоположение определено";
        case "denied": return "Нет доступа к геопозиции. Разрешите доступ или выберите демо-местоположение";
        case "unavailable": return "Геопозиция недоступна. Можно выбрать демо-местоположение";
        default: return "Определяем местоположение…";
        }
    }

    signal positionApplied(real latitude, real longitude, bool isDemo)

    function enableDemo() {
        var result = parkingService.setPosition(F.demoLatitude, F.demoLongitude, true);
        if (result.ok) {
            useDemo = true;
            positionApplied(F.demoLatitude, F.demoLongitude, true);
        }
        return result;
    }

    // Вернуться к реальной позиции, если она уже получена.
    function useRealPosition() {
        useDemo = false;
        applyFix();
    }

    function applyFix() {
        var position = source.position;
        if (!position.latitudeValid || !position.longitudeValid)
            return;
        var coordinate = position.coordinate;
        status = "ok";
        timeout.stop();
        if (useDemo)
            return;
        var result = parkingService.setPosition(coordinate.latitude, coordinate.longitude, false);
        if (result.ok)
            positionApplied(coordinate.latitude, coordinate.longitude, false);
    }

    PositionSource {
        id: source

        active: true
        updateInterval: 5000

        onPositionChanged: tracker.applyFix()
        onSourceErrorChanged: {
            if (sourceError === PositionSource.AccessError) {
                tracker.status = "denied";
            } else if (sourceError !== PositionSource.NoError && tracker.status !== "ok") {
                tracker.status = "unavailable";
            }
        }
    }

    Timer {
        id: timeout

        interval: 15000
        running: true
        onTriggered: {
            if (tracker.status === "waiting")
                tracker.status = "unavailable";
        }
    }

    Component.onCompleted: {
        if (!source.valid)
            status = "unavailable";
    }
}
