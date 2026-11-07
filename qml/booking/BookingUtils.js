.pragma library

// Вспомогательные функции экранов бронирования (#5). Бизнес-логики здесь нет:
// цены, проверки и запись выполняет BookingService (#9).
// Сервисы передаются аргументами, потому что .pragma library не видит контекст QML.

function _hex(count) {
    var text = ""
    for (var i = 0; i < count; ++i)
        text += Math.floor(Math.random() * 16).toString(16)
    return text
}

// Ключ операции в форме UUID v4. Создаётся один раз на подтверждение/продление
// и переиспользуется при повторе той же операции.
function newOperationId() {
    var variant = (8 + Math.floor(Math.random() * 4)).toString(16)
    return _hex(8) + "-" + _hex(4) + "-4" + _hex(3) + "-" + variant + _hex(3) + "-" + _hex(12)
}

function nowUtcSeconds() {
    return Math.floor(Date.now() / 1000)
}

function remainingSeconds(expiresAtUtc, nowUtc) {
    if (typeof expiresAtUtc !== "number" || !isFinite(expiresAtUtc))
        return 0
    return Math.max(0, Math.floor(expiresAtUtc - nowUtc))
}

function hasSession(session) {
    return !!session && typeof session === "object"
            && typeof session.id === "string" && session.id.length > 0
}

function _isResultOk(result) {
    return !!result && result.ok === true && !!result.data
}

function zoneName(parkingService, zoneId) {
    if (!parkingService || !zoneId)
        return zoneId || ""
    var result = parkingService.zoneDetails(zoneId)
    if (_isResultOk(result) && result.data.name)
        return result.data.name
    return zoneId
}

function zoneAddress(parkingService, zoneId) {
    if (!parkingService || !zoneId)
        return ""
    var result = parkingService.zoneDetails(zoneId)
    return _isResultOk(result) && result.data.address ? result.data.address : ""
}

function spaceLabel(parkingService, zoneId, spaceId) {
    if (!parkingService || !zoneId || !spaceId)
        return spaceId || ""
    var list = parkingService.spaces(zoneId) || []
    for (var i = 0; i < list.length; ++i) {
        if (list[i] && list[i].id === spaceId)
            return list[i].label || spaceId
    }
    return spaceId
}

function vehiclePlate(accountService, vehicleId) {
    if (!accountService || !vehicleId)
        return vehicleId || ""
    var list = accountService.vehicles || []
    for (var i = 0; i < list.length; ++i) {
        if (list[i] && list[i].id === vehicleId)
            return list[i].plate || vehicleId
    }
    return vehicleId
}

function _validVehicles(vehicles) {
    var result = []
    var list = vehicles || []
    for (var i = 0; i < list.length; ++i) {
        var item = list[i]
        if (item && typeof item.id === "string" && item.id.length > 0)
            result.push(item)
    }
    return result
}

// Индекс машины с данным ID в списке или -1. Пустой ID никогда не найден.
function vehicleIndex(vehicles, vehicleId) {
    if (typeof vehicleId !== "string" || vehicleId.length === 0)
        return -1
    var list = _validVehicles(vehicles)
    for (var i = 0; i < list.length; ++i) {
        if (list[i].id === vehicleId)
            return i
    }
    return -1
}

// Выбор машины для подтверждения: сохраняет переданный ID, если машина есть в списке;
// единственную машину выбирает сам; при нескольких или ни одной возвращает "" (выбор за пользователем).
function chooseVehicleId(vehicles, currentId) {
    var list = _validVehicles(vehicles)
    if (vehicleIndex(list, currentId) >= 0)
        return currentId
    if (list.length === 1)
        return list[0].id
    return ""
}

var minDurationMinutes = 15
var maxDurationMinutes = 1440
var durationStepMinutes = 15
var defaultDurationMinutes = 60

// Переданная длительность используется, только если она допустима; иначе — 60 минут.
function normalizeDuration(minutes) {
    return isValidDuration(minutes) ? minutes : defaultDurationMinutes
}

// Шаг длительности: ±15 минут с ограничением 15–1440.
function stepDuration(minutes, steps) {
    var next = normalizeDuration(minutes) + steps * durationStepMinutes
    return Math.max(minDurationMinutes, Math.min(maxDurationMinutes, next))
}

// Варианты продления (минуты), все в пределах 15–1440 и кратны 15.
var extensionOptions = [15, 30, 60, 120]

function indexOfOption(options, minutes) {
    for (var i = 0; i < options.length; ++i) {
        if (options[i] === minutes)
            return i
    }
    return -1
}

function isValidDuration(minutes) {
    return typeof minutes === "number" && Math.floor(minutes) === minutes
            && minutes >= 15 && minutes <= 1440 && minutes % 15 === 0
}
