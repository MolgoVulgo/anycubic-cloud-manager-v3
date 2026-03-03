import QtQuick 2.15
import QtQuick.Controls 2.15

Frame {
    property string stage: "idle"
    property int percent: 0

    Column {
        spacing: 8
        Text { text: stage }
        ProgressBar { from: 0; to: 100; value: percent }
    }
}
