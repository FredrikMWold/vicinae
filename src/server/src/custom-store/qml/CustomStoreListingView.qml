import QtQuick
import QtQuick.Layouts

Item {
    id: root

    required property var host

    function moveUp() {
        listView.moveUp();
    }

    function moveDown() {
        listView.moveDown();
    }

    function moveSectionUp() {
        listView.moveSectionUp();
    }

    function moveSectionDown() {
        listView.moveSectionDown();
    }

    GenericListView {
        id: listView

        anchors.fill: parent
        listModel: root.host.listModel
        model: root.host.listModel
        autoWireModel: true
        emptyTitle: qsTr("No extensions")
        emptyDescription: qsTr("Refresh this store after adding top-level extension folders.")
        emptyIcon: Img.builtin("store").withFillColor(Theme.textMuted)

        delegate: Loader {
            id: delegateLoader

            required property int index
            required property bool isSection
            required property bool isSelectable
            required property string sectionName
            required property string title
            required property string subtitle
            required property string iconSource
            required property var itemAccessory
            required property bool isInstalled

            width: ListView.view.width
            sourceComponent: isSection ? sectionComponent : itemComponent

            Component {
                id: sectionComponent

                SectionHeader {
                    width: delegateLoader.width
                    text: delegateLoader.sectionName
                }

            }

            Component {
                id: itemComponent

                SelectableDelegate {
                    id: itemDelegate

                    width: delegateLoader.width
                    height: 50
                    selected: listView.currentIndex === delegateLoader.index
                    onClicked: listView.currentIndex = delegateLoader.index
                    onActivated: listView.itemActivated(delegateLoader.index)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 15

                        ViciImage {
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 30
                            source: delegateLoader.iconSource
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: delegateLoader.title
                                color: itemDelegate.selected ? Theme.listItemSelectionFg : Theme.foreground
                                font.pointSize: Theme.regularFontSize
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                Layout.fillWidth: true
                            }

                            Text {
                                text: delegateLoader.subtitle
                                color: Theme.textMuted
                                font.pointSize: Theme.smallerFontSize
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                Layout.fillWidth: true
                            }

                        }

                        ViciImage {
                            visible: delegateLoader.isInstalled
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            source: Img.builtin("check-circle").withFillColor(Theme.toastSuccess)
                        }

                    }

                }

            }

        }

    }

}
