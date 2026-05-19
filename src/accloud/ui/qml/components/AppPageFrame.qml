import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Rectangle {
    id: root

    property string sectionTitle: ""
    property string sectionSubtitle: ""
    property bool showSectionHeader: sectionTitle.length > 0 || sectionSubtitle.length > 0
    property bool embeddedInTabsContainer: false
    default property alias contentData: contentColumn.data

    radius: embeddedInTabsContainer ? 0 : Theme.radiusDialog
    color: Theme.bgSurface
    border.width: embeddedInTabsContainer ? 0 : Theme.borderWidth
    border.color: Theme.borderDefault

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingPage
        spacing: Theme.gapSection

        SectionHeader {
            visible: root.showSectionHeader
            Layout.fillWidth: true
            title: root.sectionTitle
            subtitle: root.sectionSubtitle
        }

        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.gapSection
        }
    }
}
