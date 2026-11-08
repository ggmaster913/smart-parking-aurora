// Host-only navigation recorder. Does not emulate Silica rendering or target APIs.
import QtQuick 2.0
Item {
    property url cover
    property int allowedOrientations
    property int defaultAllowedOrientations: 0
    property QtObject pageStack: QtObject {
        property int depth: 1
        property string lastUrl: ""
        property var currentPage: null
        function replaceAbove(page, url, properties) {
            lastUrl = url.toString()
            return null
        }
        function push(url, properties, action) {
            lastUrl = url.toString()
            return null
        }
        function find(predicate) { return null }
        function pop() {}
    }
}
