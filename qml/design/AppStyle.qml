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

    readonly property string currencySign: "₽"
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
    function formatKopecks(kopecks) {
        if (!_isWholeNumber(kopecks) || kopecks < 0)
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
