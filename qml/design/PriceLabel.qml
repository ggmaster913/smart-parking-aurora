import QtQuick 2.0
import Sailfish.Silica 1.0

// Сумма в копейках в рублях. Контракт: docs/DESIGN.md, раздел 5.
Label {
    // real, а не int: QML int 32-битный, а суммы по контракту — целые 0..2^53−1.
    property real amountKopecks: 0
    property string suffix: ""
    property string freeText: qsTr("Бесплатно")

    readonly property string formattedAmount: (amountKopecks === 0 && freeText.length > 0)
                                              ? freeText
                                              : AppStyle.formatKopecks(amountKopecks)

    text: suffix.length > 0 && !(amountKopecks === 0 && freeText.length > 0)
          ? formattedAmount + " " + suffix
          : formattedAmount
    color: AppStyle.accentColor
    font.pixelSize: Theme.fontSizeMedium
    wrapMode: Text.Wrap
}
