// SPDX-License-Identifier: MIT
// Корень и общая навигация SmartParking (#28). Страницы модулей только испускают сигналы;
// переходы между ними выполняет маршрутизатор ниже, чтобы стек не копил дубликаты.
import QtQuick 2.0
import Sailfish.Silica 1.0

ApplicationWindow {
    id: app

    objectName: "applicationWindow"
    cover: Qt.resolvedUrl("cover/DefaultCoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    readonly property url mainUrl: Qt.resolvedUrl("pages/MainPage.qml")
    readonly property url startupErrorUrl: Qt.resolvedUrl("pages/StartupErrorPage.qml")
    readonly property url signInUrl: Qt.resolvedUrl("account/SignInPage.qml")
    readonly property url profileUrl: Qt.resolvedUrl("account/ProfilePage.qml")
    readonly property url mapUrl: Qt.resolvedUrl("parking/ParkingMapPage.qml")
    readonly property url confirmationUrl: Qt.resolvedUrl("booking/ConfirmationPage.qml")
    readonly property url ticketUrl: Qt.resolvedUrl("booking/TicketPage.qml")
    readonly property url activeUrl: Qt.resolvedUrl("booking/ActiveParkingPage.qml")

    function mainPage() {
        return pageStack.find(function (page) { return page.objectName === "mainPage" })
    }

    // Заменяет весь стек одной страницей; при пустом стеке — обычный push.
    function resetTo(url, properties) {
        if (pageStack.depth === 0)
            pageStack.push(url, properties || {}, PageStackAction.Immediate)
        else
            pageStack.replaceAbove(null, url, properties || {})
    }

    // Поверх главной страницы (или вместо всего стека, если её нет).
    function showAboveMain(url, properties) {
        var main = mainPage()
        if (main)
            return pageStack.replaceAbove(main, url, properties || {})
        resetTo(mainUrl)
        return pageStack.push(url, properties || {})
    }

    function showStart() {
        if (!appStartup.ready)
            resetTo(startupErrorUrl)
        else if (accountService.signedIn)
            resetTo(mainUrl)
        else
            resetTo(signInUrl, { "nextPage": "" })
    }

    function locateZone(zoneId) {
        var map = showAboveMain(mapUrl, { "bookingPageUrl": confirmationUrl })
        if (map && map.focusZone)
            map.focusZone(zoneId)
    }

    Component.onCompleted: showStart()

    Connections {
        target: appStartup
        // This signal is emitted only after all service context properties exist.
        onServicesReady: app.showStart()
    }

    // Единый маршрутизатор сигналов текущей страницы.
    Connections {
        target: pageStack.currentPage
        ignoreUnknownSignals: true

        onSignedIn: app.resetTo(app.mainUrl)
        onSignedOut: app.resetTo(app.signInUrl, { "nextPage": "" })
        onTicketReady: app.showAboveMain(app.ticketUrl, { "session": session })
        onActiveParkingRequested: app.showAboveMain(app.activeUrl)
        onVehicleRequired: pageStack.push(app.profileUrl, { "signInPage": "" })
        onBackRequested: pageStack.pop()
        onLocateParkingRequested: app.locateZone(zoneId)
    }
}
