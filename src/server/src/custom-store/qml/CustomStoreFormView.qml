import QtQuick

Item {
    id: root

    required property var host

    FormView {
        id: formView

        anchors.fill: parent
        Component.onCompleted: Qt.callLater(formView.focusFirst)

        FormField {
            label: qsTr("Name")
            error: root.host.nameError

            FormTextInput {
                text: root.host.name
                placeholder: qsTr("Personal Extensions")
                hasError: root.host.nameError !== ""
                onTextEdited: root.host.name = text
            }

        }

        FormField {
            label: qsTr("GitHub Repository")
            error: root.host.urlError

            FormTextInput {
                text: root.host.url
                placeholder: "https://github.com/owner/repository"
                hasError: root.host.urlError !== ""
                onTextEdited: root.host.url = text
            }

        }

        FormField {
            label: qsTr("Branch")
            error: root.host.branchError

            FormTextInput {
                text: root.host.branch
                placeholder: "main"
                hasError: root.host.branchError !== ""
                onTextEdited: root.host.branch = text
            }

        }

    }

}
