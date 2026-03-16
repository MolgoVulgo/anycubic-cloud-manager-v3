import QtQuick 2.15
import "../components/Theme.js" as Theme
import "../components"

AppDialogFrame {
    id: root
    title: qsTr("Print Config")
    subtitle: qsTr("Advanced flags")
    minimumWidth: 620
    maximumWidth: 760

    property bool liftCompensation: false
    property bool autoResinCheck: true

    signal liftCompensationToggled(bool checked)
    signal autoResinCheckToggled(bool checked)
    signal closeRequested()

    AppCheckBox {
        text: qsTr("Lift compensation")
        checked: root.liftCompensation
        onToggled: root.liftCompensationToggled(checked)
    }

    Text {
        text: qsTr("Adds extra stabilization on Z lifts.")
        color: Theme.fgSecondary
        font.pixelSize: Theme.fontCaptionPx
        opacity: 0.9
    }

    AppCheckBox {
        text: qsTr("Auto resin check")
        checked: root.autoResinCheck
        onToggled: root.autoResinCheckToggled(checked)
    }

    Text {
        text: qsTr("Best-effort pre-check before sending order.")
        color: Theme.fgSecondary
        font.pixelSize: Theme.fontCaptionPx
        opacity: 0.9
    }

    footerTrailingData: [
        AppButton {
            text: qsTr("Close")
            variant: "secondary"
            onClicked: root.closeRequested()
        }
    ]
}
