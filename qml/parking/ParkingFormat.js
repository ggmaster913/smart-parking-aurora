// SPDX-License-Identifier: MIT
.pragma library

// Демонстрационная точка (центр Москвы). В интерфейсе всегда помечается как демо
// и не выдаётся за реальное местоположение устройства.
var demoLatitude = 55.7558;
var demoLongitude = 37.6173;

// Подложка: растровые тайлы OpenStreetMap без ключа доступа.
// Требуется атрибуция «© участники OpenStreetMap» (см. third_party/mfw/README.md).
var osmStyleJson = '{"version":8,"sources":{"osm":{"type":"raster","tiles":'
        + '["https://tile.openstreetmap.org/{z}/{x}/{y}.png"],"tileSize":256}},'
        + '"layers":[{"id":"osm","type":"raster","source":"osm","minzoom":0,"maxzoom":19}]}';
var osmAttribution = "© участники OpenStreetMap";

function groupDigits(value) {
    var text = String(value);
    var result = "";
    while (text.length > 3) {
        result = " " + text.slice(text.length - 3) + result;
        text = text.slice(0, text.length - 3);
    }
    return text + result;
}

// Целые копейки в строку рублей: 18000 -> «180 ₽», 4550 -> «45,50 ₽».
function rubles(kopecks) {
    var value = Math.round(Number(kopecks));
    if (!isFinite(value))
        return "—";
    var sign = value < 0 ? "−" : "";
    value = Math.abs(value);
    var rub = Math.floor(value / 100);
    var kop = value % 100;
    var text = sign + groupDigits(rub);
    if (kop !== 0)
        text += "," + (kop < 10 ? "0" : "") + kop;
    return text + " ₽";
}

function rate(kopecksPerHour) {
    if (Number(kopecksPerHour) === 0)
        return "Бесплатно";
    return rubles(kopecksPerHour) + "/ч";
}

// Расстояние по прямой; -1 означает «позиция неизвестна».
function distance(meters) {
    var value = Number(meters);
    if (!isFinite(value) || value < 0)
        return "—";
    if (value < 1000)
        return Math.round(value) + " м";
    var km = Math.round(value / 100) / 10;
    return String(km).replace(".", ",") + " км";
}

function freeSpaces(free, capacity) {
    if (Number(free) <= 0)
        return "Мест нет";
    return "Свободно " + free + " из " + capacity;
}
