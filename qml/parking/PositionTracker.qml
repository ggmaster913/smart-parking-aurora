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

    // waiting — ждём первые координаты; ok — свежая реальная позиция;
    // lost — позиция была, но сигнал пропал или устарел (координата сохранена);
    // denied — нет разрешения; unavailable — источник недоступен или не ответил.
    property string status: "waiting"
    property bool useDemo: false
    property bool hadFix: false
    // Последняя реальная координата (для возврата из демо-режима).
    property real lastLatitude: 0
    property real lastLongitude: 0
    property alias active: source.active
    readonly property bool hasRealFix: status === "ok"

    // Без новых координат дольше этого срока позиция считается устаревшей.
    property int staleAfterMs: 30000

    readonly property string statusText: {
        if (useDemo)
            return "Используется демо-местоположение (не ваше реальное)";
        switch (status) {
        case "ok": return "Местоположение определено";
        case "lost": return "Сигнал геопозиции потерян. Расстояния показаны от последнего известного положения";
        case "denied": return hadFix
                              ? "Доступ к геопозиции отозван. Расстояния показаны от последнего известного положения"
                              : "Нет доступа к геопозиции. Разрешите доступ или выберите демо-местоположение";
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

    // Вернуться к последней реальной позиции.
    function useRealPosition() {
        if (!hadFix)
            return;
        useDemo = false;
        var result = parkingService.setPosition(lastLatitude, lastLongitude, false);
        if (result.ok)
            positionApplied(lastLatitude, lastLongitude, false);
    }

    function markProblem(newStatus) {
        // После успешного fix любая ошибка источника — потеря сигнала,
        // кроме явного запрета доступа.
        if (newStatus === "denied")
            status = "denied";
        else
            status = hadFix ? "lost" : newStatus;
        staleTimer.stop();
    }

    function applyFix() {
        var position = source.position;
        if (!position.latitudeValid || !position.longitudeValid)
            return;
        handleFix(position.coordinate.latitude, position.coordinate.longitude);
    }

    // Новая реальная координата (отдельно от PositionSource — для проверок).
    // В демо-режиме реальная позиция только запоминается как свежая и не
    // заменяет демо-точку в parkingService.
    function handleFix(latitude, longitude) {
        if (!isFinite(latitude) || !isFinite(longitude)
                || latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180)
            return;
        status = "ok";
        hadFix = true;
        lastLatitude = latitude;
        lastLongitude = longitude;
        startTimeout.stop();
        staleTimer.restart();
        if (useDemo)
            return;
        var result = parkingService.setPosition(latitude, longitude, false);
        if (result.ok)
            positionApplied(latitude, longitude, false);
    }

    PositionSource {
        id: source

        active: true
        updateInterval: 5000

        onPositionChanged: tracker.applyFix()
        onSourceErrorChanged: {
            if (sourceError === PositionSource.AccessError)
                tracker.markProblem("denied");
            else if (sourceError !== PositionSource.NoError)
                tracker.markProblem("unavailable");
        }
        onValidChanged: {
            if (!valid)
                tracker.markProblem("unavailable");
        }
    }

    // Первые координаты не пришли.
    Timer {
        id: startTimeout

        interval: 15000
        running: true
        onTriggered: {
            if (tracker.status === "waiting")
                tracker.status = "unavailable";
        }
    }

    // Координаты перестали приходить без явной ошибки источника.
    Timer {
        id: staleTimer

        interval: tracker.staleAfterMs
        onTriggered: {
            if (tracker.status === "ok")
                tracker.status = "lost";
        }
    }

    Component.onCompleted: {
        if (!source.valid)
            status = "unavailable";
    }
}
