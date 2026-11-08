pragma Singleton
import QtQuick 2.0
import Sailfish.Silica 1.0

// Общее оформление «Умной парковки». Контракт описан в docs/DESIGN.md, раздел 5.
// Цвета и размеры берутся из Theme; собственный только акцент.
QtObject {
    id: style

    readonly property color accentBase: "#1E9BB0"
    readonly property color accentColor: Theme.highlightFromColor(accentBase, Theme.colorScheme)
    readonly property color cardColor: Theme.rgba(Theme.highlightBackgroundColor, 0.12)
    // Плотная подложка панелей поверх карты: читаемость текста над тайлами.
    readonly property color overlayCardColor: Theme.rgba(Theme.highlightDimmerColor, 0.94)
    readonly property color hairlineColor: Theme.rgba(Theme.primaryColor, 0.2)

    // Статусы времени (#19): янтарный — мало времени, красный — истекло/ошибка.
    // Цвет всегда дублируется подписью.
    readonly property color warningBase: "#E0A030"
    readonly property color warningColor: Theme.highlightFromColor(warningBase, Theme.colorScheme)
    readonly property color dangerColor: Theme.errorColor
    // Порог «мало времени» для активной парковки, секунды.
    readonly property int lowTimeSeconds: 15 * 60

    // Движение (#19). Значения — проектные, не норматив SDK. Выключатель без экрана настроек.
    property bool motionEnabled: true
    readonly property int motionAccent: motionEnabled ? 150 : 0   // акцент выбора зоны/места
    readonly property int motionCard: motionEnabled ? 200 : 0     // нижняя карточка: opacity + сдвиг
    readonly property int motionTicket: motionEnabled ? 220 : 0   // вход талона
    readonly property int motionProgress: motionEnabled ? 250 : 0 // шкала времени между секундами

    // Доля оставшегося времени 0..1 для шкалы; без корректного интервала — 0.
    function remainingFraction(startsAtUtc, expiresAtUtc, nowUtc) {
        if (!_isWholeNumber(startsAtUtc) || !_isWholeNumber(expiresAtUtc) || expiresAtUtc <= startsAtUtc)
            return 0
        var left = Math.max(0, expiresAtUtc - nowUtc)
        return Math.min(1, left / (expiresAtUtc - startsAtUtc))
    }

    // Цвет состояния времени: акцент, янтарный при остатке < lowTimeSeconds, красный при 0.
    function timeStateColor(remainingSeconds) {
        if (!(remainingSeconds > 0))
            return dangerColor
        return remainingSeconds < lowTimeSeconds ? warningColor : accentColor
    }

    function formatDistance(meters) {
        if (typeof meters !== "number" || !isFinite(meters) || meters < 0)
            return ""
        if (Math.round(meters) < 1000)
            return Math.round(meters) + nbsp + qsTr("м")
        return (Math.round(meters / 100) / 10).toString().replace(".", ",") + nbsp + qsTr("км")
    }

    readonly property string currencySign: "₽"
    // Наибольшая сумма, точно представимая числом QML/JSON: 2^53 − 1 коп. (контракт #14).
    readonly property real maxKopecks: 9007199254740991
    readonly property string nbsp: " "

    function _isWholeNumber(value) {
        return typeof value === "number" && isFinite(value) && Math.floor(value) === value
    }

    function _pad2(value) {
        return value < 10 ? "0" + value : "" + value
    }

    function _groupThousands(value) {
        var digits = "" + value
        var result = ""
        while (digits.length > 3) {
            result = nbsp + digits.slice(digits.length - 3) + result
            digits = digits.slice(0, digits.length - 3)
        }
        return digits + result
    }

    // 13500 -> "135 ₽", 13505 -> "135,05 ₽", 1234567 -> "12 345,67 ₽".
    // Допустимы только целые 0..2^53−1; дробные, отрицательные, NaN/Infinity и больше предела -> "—".
    function formatKopecks(kopecks) {
        if (!_isWholeNumber(kopecks) || kopecks < 0 || kopecks > maxKopecks)
            return "—"
        var rubles = Math.floor(kopecks / 100)
        var rest = kopecks % 100
        var text = _groupThousands(rubles)
        if (rest > 0)
            text += "," + _pad2(rest)
        return text + nbsp + currencySign
    }

    // 45 -> "45 мин", 60 -> "1 ч", 90 -> "1 ч 30 мин".
    function formatDuration(minutes) {
        if (!_isWholeNumber(minutes) || minutes < 0)
            return "—"
        var hours = Math.floor(minutes / 60)
        var rest = minutes % 60
        if (hours === 0)
            return rest + nbsp + qsTr("мин")
        if (rest === 0)
            return hours + nbsp + qsTr("ч")
        return hours + nbsp + qsTr("ч") + " " + rest + nbsp + qsTr("мин")
    }

    // Остаток в секундах: 3899 -> "1:04:59", 309 -> "05:09", отрицательное -> "0:00".
    function formatRemaining(seconds) {
        var total = (typeof seconds === "number" && isFinite(seconds)) ? Math.max(0, Math.floor(seconds)) : 0
        if (total === 0)
            return "0:00"
        var hours = Math.floor(total / 3600)
        var minutes = Math.floor((total % 3600) / 60)
        var secs = total % 60
        if (hours > 0)
            return hours + ":" + _pad2(minutes) + ":" + _pad2(secs)
        return _pad2(minutes) + ":" + _pad2(secs)
    }

    // Unix-время UTC в секундах -> локальное "ЧЧ:ММ" или "d MMMM, ЧЧ:ММ", если не сегодня.
    function formatLocalTime(utcSeconds) {
        if (!_isWholeNumber(utcSeconds) || utcSeconds <= 0)
            return "—"
        var date = new Date(utcSeconds * 1000)
        var now = new Date()
        var sameDay = date.getFullYear() === now.getFullYear()
                && date.getMonth() === now.getMonth()
                && date.getDate() === now.getDate()
        return Qt.formatDateTime(date, sameDay ? "HH:mm" : "d MMMM, HH:mm")
    }

    function _plural(count, one, few, many) {
        var mod10 = count % 10
        var mod100 = count % 100
        if (mod10 === 1 && mod100 !== 11)
            return one
        if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14))
            return few
        return many
    }

    // «Свободно 12 из 40», «Осталось 2 места», «Мест нет».
    function freeSpacesText(freeSpaces, capacity) {
        if (!_isWholeNumber(freeSpaces) || freeSpaces <= 0)
            return qsTr("Мест нет")
        if (freeSpaces <= 3)
            return qsTr("Осталось") + " " + freeSpaces + nbsp
                    + _plural(freeSpaces, qsTr("место"), qsTr("места"), qsTr("мест"))
        if (_isWholeNumber(capacity) && capacity >= freeSpaces)
            return qsTr("Свободно") + " " + freeSpaces + " " + qsTr("из") + " " + capacity
        return qsTr("Свободно") + " " + freeSpaces
    }
}
