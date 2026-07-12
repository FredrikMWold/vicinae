import QtQuick
import QtQuick.Layouts

Item {
    id: root

    required property var host

    FormView {
        id: formView

        anchors.fill: parent
        Component.onCompleted: Qt.callLater(formView.focusFirst)

        Repeater {
            model: root.host.arguments

            delegate: FormField {
                id: argumentField

                required property int index
                required property var modelData
                readonly property var argument: modelData || ({
                })

                label: argument.label || argument.name || "Argument"
                error: argument.error || ""

                FormTextInput {
                    text: argumentField.argument.value || ""
                    placeholder: argumentField.argument.placeholder || argumentField.label
                    hasError: argumentField.error !== ""
                    onTextEdited: root.host.setArgumentValue(argumentField.index, text)
                    onAccepted: root.host.submit()
                }

            }

        }

    }

}
