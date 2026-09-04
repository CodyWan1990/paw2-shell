import QtQuick
import "../services" as Services

/// Spec-driven settings editor shared by shell and compositor settings.
Item {
    id: root

    property var spec: ({})
    property var value: undefined
    property var choices: spec.choices ?? []
    property int labelWidth: 110
    signal valueEdited(var value)

    // ─── keyboard: forward the host's row cursor to whichever editor the
    // Loader built. A section header or a note is not a stop, so it reports
    // none; a `preset` row showing its custom field, and the workspace layout
    // list, are several, and those come from the loaded item itself.
    property int cursorIndex: -1
    readonly property bool navigable: !spec.section && spec.type !== "note"
    readonly property int selectableCount: {
        if (!navigable || !editor.item)
            return 0;
        return editor.item.selectableCount !== undefined
            ? editor.item.selectableCount : 1;
    }

    onCursorIndexChanged: paintCursor()
    function paintCursor() {
        const item = editor.item;
        if (!item)
            return;
        if (item.selectableCount !== undefined)
            item.cursorIndex = cursorIndex;
        else if (item.cursor !== undefined)
            item.cursor = cursorIndex === 0;
    }

    function activateAt(i) {
        const item = editor.item;
        if (!item)
            return;
        if (item.activateAt !== undefined)
            item.activateAt(i);
        else if (item.keyActivate !== undefined)
            item.keyActivate();
    }

    function adjustAt(i, delta) {
        const item = editor.item;
        if (!item)
            return;
        if (item.adjustAt !== undefined)
            item.adjustAt(i, delta);
        else if (item.keyAdjust !== undefined)
            item.keyAdjust(delta);
    }

    implicitHeight: editor.item
        ? (editor.item.implicitHeight > 0 ? editor.item.implicitHeight : editor.item.height)
        : 0
    height: implicitHeight

    function choiceLabel(choice) {
        return typeof choice === "object" ? choice.label : choice;
    }
    function choiceValue(choice) {
        return typeof choice === "object" ? choice.value : choice;
    }
    function labelForValue(current) {
        for (const choice of choices)
            if (choiceValue(choice) === current)
                return choiceLabel(choice);
        return current ?? "";
    }
    function valueForLabel(label) {
        for (const choice of choices)
            if (choiceLabel(choice) === label)
                return choiceValue(choice);
        return label;
    }

    Loader {
        id: editor
        width: parent.width
        height: root.height
        // A rebuilt editor starts with no cursor; re-apply whatever the host
        // had set, or the highlight vanishes when a row changes shape.
        onLoaded: root.paintCursor()
        sourceComponent: {
            if (root.spec.section) return sectionEditor;
            switch (root.spec.type) {
            case "bool": return boolEditor;
            case "int":
            case "float": return sliderEditor;
            case "enum":
            case "choice":
            case "palette": return dropdownEditor;
            case "preset": return presetEditor;
            case "note": return noteEditor;
            default: return textEditor;
            }
        }
    }

    Component {
        id: sectionEditor
        Item {
            width: parent.width
            implicitHeight: 34

            SectionHeader {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: root.spec.section
            }
        }
    }

    Component {
        id: boolEditor
        SegmentedControl {
            width: parent.width
            labelWidth: root.labelWidth
            label: root.spec.label
            model: [{ value: "true", label: "On" }, { value: "false", label: "Off" }]
            valueRole: "value"
            textRole: "label"
            currentValue: root.value ? "true" : "false"
            onSelected: value => root.valueEdited(value === "true")
        }
    }

    Component {
        id: sliderEditor
        StyledSlider {
            readonly property real unit: root.spec.scale ?? 1
            width: parent.width
            labelWidth: root.labelWidth
            label: root.spec.label
            suffix: root.spec.suffix ?? ""
            from: root.spec.from
            to: root.spec.to
            stepSize: root.spec.step ?? 1
            decimals: root.spec.decimals ?? 0
            value: root.value / unit
            onMoved: movedValue => root.valueEdited(root.spec.type === "int"
                ? Math.round(movedValue) * unit
                : Math.round(movedValue / stepSize) * stepSize * unit)
        }
    }

    Component {
        id: dropdownEditor
        StyledDropdown {
            width: parent.width
            labelWidth: root.labelWidth
            label: root.spec.label
            filterable: root.spec.filterable === true
            model: root.choices.map(choice => root.choiceLabel(choice))
            currentValue: root.labelForValue(root.value)
            onSelected: label => root.valueEdited(root.valueForLabel(label))
        }
    }

    Component {
        id: presetEditor
        Item {
            id: preset
            readonly property bool allowCustom: root.spec.allowCustom !== false
            readonly property bool hasMatch: root.choices.some(
                choice => root.choiceValue(choice) === root.value)
            property bool customOpen: allowCustom && !hasMatch
            width: parent.width
            implicitHeight: dropdown.implicitHeight
                + (customOpen ? customField.height + 6 : 0)
                + (customOpen && root.spec.hint ? hint.implicitHeight + 2 : 0)

            // Two stops once the custom field is showing: the preset dropdown,
            // then the free-text value it revealed.
            property int cursorIndex: -1
            readonly property int selectableCount: customOpen ? 2 : 1
            onCursorIndexChanged: {
                dropdown.cursor = cursorIndex === 0;
                customField.cursor = cursorIndex === 1;
            }
            function activateAt(i) {
                if (i === 0) dropdown.keyActivate();
                else customField.keyActivate();
            }
            function adjustAt(i, delta) {
                if (i === 0) dropdown.keyAdjust(delta);
            }

            StyledDropdown {
                id: dropdown
                width: parent.width
                labelWidth: root.labelWidth
                label: root.spec.label
                // A detected list can be long (every installed icon theme), so
                // the same filter the plain dropdown offers.
                filterable: root.spec.filterable === true || root.choices.length > 8
                model: root.choices.map(choice => root.choiceLabel(choice))
                    .concat(preset.allowCustom ? ["Custom…"] : [])
                currentValue: preset.customOpen ? "Custom…" : root.labelForValue(root.value)
                onSelected: label => {
                    if (label === "Custom…") {
                        preset.customOpen = true;
                        return;
                    }
                    preset.customOpen = false;
                    root.valueEdited(root.valueForLabel(label));
                }
            }

            SettingField {
                id: customField
                anchors.top: dropdown.bottom
                anchors.topMargin: 6
                anchors.left: parent.left
                anchors.leftMargin: root.labelWidth + 16
                anchors.right: parent.right
                visible: preset.customOpen
                Component.onCompleted: text = root.value ?? ""
                onEdited: value => root.valueEdited(value)
            }

            StyledText {
                id: hint
                anchors.top: customField.bottom
                anchors.topMargin: 2
                anchors.left: customField.left
                anchors.right: parent.right
                visible: preset.customOpen && !!root.spec.hint
                text: root.spec.hint ?? ""
                color: Services.Colors.color8
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }

    Component {
        id: textEditor
        LabeledSettingRow {
            width: parent.width
            labelWidth: root.labelWidth
            label: root.spec.label

            SettingField {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                Component.onCompleted: text = root.spec.type === "csv"
                    ? (root.value || []).join(", ") : (root.value ?? "")
                onEdited: value => root.valueEdited(root.spec.type === "csv"
                    ? value.split(",").map(part => part.trim()).filter(part => part.length > 0)
                    : value)
            }
        }
    }

    Component {
        id: noteEditor
        StyledText {
            width: parent.width
            text: root.spec.text ?? ""
            color: Services.Colors.color8
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
