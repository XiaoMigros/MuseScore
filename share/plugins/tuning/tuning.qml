// Apply a choice of tempraments and tunings.
// Copyright (C) 2018-2019  Bill Hails
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Muse.UiComponents 1.0 as MU
import Muse.Ui 1.0

import MuseScore 3.0

MuseScore {
    version: "3.0.5"
    title: "Tuning"
    description: "Apply various temperaments and tunings"
    pluginType: "dialog"
    categoryCode: "playback"
    thumbnailName: "modal_tuning.png"
    id: root
    requiresScore: false

    readonly property int incrementalControlWidth: 60
    readonly property int defaultSpacing: 12

    property var history: 0
    property int undoIndex: -1
    property int undoLength: -1

    // set true if customisations are made to the tuning
    property var modified: false

    /**
     * See http://leware.net/temper/temper.htm and specifically http://leware.net/temper/cents.htm
     *
     * I've taken the liberty of adding the Bach/Lehman temperament http://www.larips.com which was
     * my original motivation for doing this.
     *
     * These values are in cents. One cent is defined as 100th of an equal tempered semitone.
     * Each row is ordered in the cycle of fifths, so C, G, D, A, E, B, F#, C#, G#/Ab, Eb, Bb, F;
     * and the values are offsets from the equal tempered value.
     *
     * However for tunings who's default root note is not C, the values are pre-rotated so that applying the
     * root note rotation will put the first value of the sequence at the root note.
     */
    property var equal: {
        'offsets': [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
        'root': 0,
        'pure': 0,
        'name': "equal",
        'displayName': qsTr("Equal")
    }
    property var pythagorean: {
        'offsets': [-6.0, -4.0, -2.0, 0.0, 2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 14.0, 16.0],
        'root': 9,
        'pure': 3,
        'name': "pythagorean",
        'displayName': qsTr("Pythagorean")
    }
    property var aaron: {
        'offsets': [10.5, 7.0, 3.5, 0.0, -3.5, -7.0, -10.5, -14.0, -17.5, -21.0, -24.5, -28.0],
        'root': 9,
        'pure': 3,
        'name': "aaron",
        'displayName': qsTr("Aaron")
    }
    property var silberman: {
        'offsets': [5.0, 3.3, 1.7, 0.0, -1.7, -3.3, -5.0, -6.7, -8.3, -10.0, -11.7, -13.3],
        'root': 9,
        'pure': 3,
        'name': "silberman",
        'displayName': qsTr("Silberman")
    }
    property var salinas: {
        'offsets': [16.0, 10.7, 5.3, 0.0, -5.3, -10.7, -16.0, -21.3, -26.7, -32.0, -37.3, -42.7],
        'root': 9,
        'pure': 3,
        'name': "salinas",
        'displayName': qsTr("Salinas")
    }
    property var kirnberger: {
        'offsets': [0.0, -3.5, -7.0, -10.5, -14.0, -12.0, -10.0, -10.0, -8.0, -6.0, -4.0, -2.0],
        'root': 0,
        'pure': 0,
        'name': "kirnberger",
        'displayName': qsTr("Kirnberger")
    }
    property var vallotti: {
        'offsets': [0.0, -2.0, -4.0, -6.0, -8.0, -10.0, -8.0, -6.0, -4.0, -2.0, 0.0, 2.0],
        'root': 0,
        'pure': 0,
        'name': "vallotti",
        'displayName': qsTr("Vallotti")
    }
    property var werkmeister: {
        'offsets': [0.0, -4.0, -8.0, -12.0, -10.0, -8.0, -12.0, -10.0, -8.0, -6.0, -4.0, -2.0],
        'root': 0,
        'pure': 0,
        'name': "werkmeister",
        'displayName': qsTr("Werkmeister")
    }
    property var marpurg: {
        'offsets': [0.0, 2.0, 4.0, 6.0, 0.0, 2.0, 4.0, 6.0, 0.0, 2.0, 4.0, 6.0],
        'root': 0,
        'pure': 0,
        'name': "marpurg",
        'displayName': qsTr("Marpurg")
    }
    property var just: {
        'offsets': [0.0, 2.0, 4.0, -16.0, -14.0, -12.0, -10.0, -30.0, -28.0, 16.0, 18.0, -2.0],
        'root': 0,
        'pure': 0,
        'name': "just",
        'displayName': qsTr("Just")
    }
    property var meanSemitone: {
        'offsets': [0.0, -3.5, -7.0, -10.5, -14.0, 3.5, 0.0, -3.5, -7.0, -10.5, -14.0, -17.5],
        'root': 6,
        'pure': 6,
        'name': "meanSemitone",
        'displayName': qsTr("Mean semitone")
    }
    property var grammateus: {
        'offsets': [-2.0, 0.0, 2.0, 4.0, 6.0, 8.0, 10.0, 0.0, 2.0, 4.0, 6.0, 8.0],
        'root': 11,
        'pure': 1,
        'name': "grammateus",
        'displayName': qsTr("Grammateus")
    }
    property var french: {
        'offsets': [0.0, -2.5, -5.0, -7.5, -10.0, -12.5, -13.0, -13.0, -11.0, -6.0, -1.5, 2.5],
        'root': 0,
        'pure': 0,
        'name': "french",
        'displayName': qsTr("French")
    }
    property var french2: {
        'offsets': [0.0, -3.5, -7.0, -10.5, -14.0, -17.5, -18.2, -19.0, -17.0, -10.5, -3.5, 3.5],
        'root': 0,
        'pure': 0,
        'name': "french2",
        'displayName': qsTr("Tempérament Ordinaire")
    }
    property var rameau: {
        'offsets': [0.0, -3.5, -7.0, -10.5, -14.0, -17.5, -15.5, -13.5, -11.5, -2.0, 7.0, 3.5],
        'root': 0,
        'pure': 0,
        'name': "rameau",
        'displayName': qsTr("Rameau")
    }
    property var irrFr17e: {
        'offsets': [-8.0, -2.0, 3.0, 0.0, -3.0, -6.0, -9.0, -12.0, -15.0, -18.0, -21.0, -24.0],
        'root': 9,
        'pure': 3,
        'name': "irrFr17e",
        'displayName': qsTr("Irr Fr 17e")
    }
    property var bachLehman: {
        'offsets': [0.0, -2.0, -3.9, -5.9, -7.8, -5.9, -3.9, -2.0, -2.0, -2.0, -2.0, 2.0],
        'root': 0,
        'pure': 3,
        'name': "bachLehman",
        'displayName': qsTr("Bach/Lehman")
    }

    property var tuningModel: [equal, pythagorean, aaron, silberman, salinas, kirnberger, vallotti, werkmeister,
                                marpurg, just, meanSemitone, grammateus, french, french2, rameau, irrFr17e, bachLehman]
    readonly property var notesStringModel: ["C", "G", "D", "A", "E", "B", "F#", "C#", "G#", "Eb", "Bb", "F"]
    readonly property var pitchOffsets: [0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5] // convert from C, C#,... to C, G,...

    property var finalModel: [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]; //list of final offsets, in notesStringModel order

    property int currentTemperament: 0
    property int currentRoot: 0
    property int currentPureTone: 0
    property var currentTweak: 0.0
    signal finalTextUpdated(int index)
    onFinalTextUpdated: function (index) {
        finalValueRepeater.itemAt(index).control.currentValue = finalModel[index]
    }
    signal recalculated(var model)
    onRecalculated: function (model) {
        for (var i in finalModel) {
            finalValueRepeater.itemAt(i).control.currentValue = model[i]
        }
    }

    onRun: {
        var data = JSON.parse(options.data)
        if (data.hasOwnProperty('edited') && data.edited) {
            restoreSavedValues(data)
        }
    }

    function getHistory() {
        if (history == 0) {
            history = new commandHistory()
        }
        return history
    }

    function applyTemperament() {
        if (curScore.selection.elements.length == 0) {
            curScore.startCmd("Add tuning to score")
            cmd("select-all")
        } else {
            curScore.startCmd("Add tuning to selection")
        }
        var chordList = []
        for (var i in curScore.selection.elements) {
            var el = curScore.selection.elements[i]
            if (el.type == Element.NOTE && el.parent.type == Element.CHORD) {
                chordList.push(el.parent)
            }
        }
        var shouldAnnotate = annotateValue.checked
        var cursor = curScore.newCursor()
        for (var i in chordList) {
            var chord = chordList[i]
            for (var j in chord.notes) {
                var note = chord.notes[j]
                note.tuning = getFinalTuning(note.pitch)
            }
            if (shouldAnnotate) {
                var isGrace = chord.notes[0].noteType != NoteType.NORMAL
                cursor.track = chord.track
                cursor.rewindToTick(isGrace ? chord.parent.parent.tick : chord.parent.tick)
                function addText(noteIndex, placement) {
                    var note = chord.notes[noteIndex]
                    var text = newElement(Element.STAFF_TEXT);
                    text.text = '' + note.tuning
                    text.autoplace = true
                    text.fontSize = 7 // smaller
                    text.placement = placement
                    cursor.add(text)
                }
                if (cursor.voice == 0 || cursor.voice == 2) {
                    for (var index = 0; index < chord.notes.length; index++) {
                        addText(index, Placement.ABOVE)
                    }
                } else {
                    for (var index = chord.notes.length - 1; index >= 0; index--) {
                        addText(index, Placement.BELOW)
                    }
                }
            }
        }
        curScore.endCmd()
        return true
    }

    function error(errorMessage) {
        errorDialog.text = qsTr(errorMessage)
        errorDialog.open()
    }

    /**
     * map a note (pitch modulo 12) to a value in one of the above tables
     * then adjust for the choice of pure note and tweak.
     */
    function lookUp(note, table) {
        var i = ((note * 7) - currentRoot + 12) % 12;
        var offset = table.offsets[i];
        var j = (currentPureTone - currentRoot + 12) % 12;
        var pureNoteAdjustment = table.offsets[j];
        var finalOffset = offset - pureNoteAdjustment;
        var tweakFinalOffset = finalOffset + currentTweak;
        return tweakFinalOffset
    }

    /**
     * returns a function for use by recalculate()
     *
     * We use an abstract function here because recalculate can be passed
     * a different function, i.e. when restoring from a save file.
     */
    function getTuning() {
        return function(pitch) {
            return lookUp(pitch, tuningModel[currentTemperament]);
        }
    }

    function getFinalTuning(pitch) {
        return finalModel[pitchOffsets[pitch % 12]];
    }

    function recalculate(tuning) {
        var oldModel = finalModel
        getHistory().add(
            function () {
                finalModel = oldModel
                root.recalculated(oldModel)
            },
            function() {
                var newModel = []
                for (var i in finalModel) {
                    newModel[i] = tuning(pitchOffsets[i]).toFixed(1)
                }
                finalModel = newModel
                root.recalculated(newModel)
            },
            "final offsets"
        )
    }

    function setCurrentTemperament(temperament) {
        var oldTemperament = currentTemperament
        getHistory().add(
            function() {
                currentTemperament = oldTemperament
            },
            function() {
                currentTemperament = temperament
            },
            "current temperament"
        )
    }

    function lookupTemperament(temperamentName) {
        for (var i in tuningModel) {
            if (tuningModel[i].name == temperamentName) {
                return i
            }
        }
    }

    function setCurrentRoot(root) {
        var oldRoot = currentRoot
        getHistory().add(
            function () {
                currentRoot = oldRoot
            },
            function() {
                currentRoot = root
            },
            "current root"
        )
    }

    function setCurrentPureTone(pureTone) {
        var oldPureTone = currentPureTone
        getHistory().add(
            function () {
                currentPureTone = oldPureTone
            },
            function() {
                currentPureTone = pureTone
            },
            "current pure tone"
        )
    }

    function setCurrentTweak(tweak) {
        var oldTweak = currentTweak
        getHistory().add(
            function () {
                currentTweak = oldTweak
            },
            function () {
                currentTweak = tweak
            },
            "current tweak"
        )
    }

    function setModified(state) {
        var oldModified = root.modified
        getHistory().add(
            function () {
                root.modified = oldModified
            },
            function () {
                root.modified = state
            },
            "modified"
        )
    }

    function temperamentClicked(temperamentIndex) {
        getHistory().begin()
        setCurrentTemperament(temperamentIndex)
        setCurrentRoot(tuningModel[temperamentIndex].root)
        setCurrentPureTone(tuningModel[temperamentIndex].pure)
        setCurrentTweak(0.0)
        recalculate(getTuning())
        getHistory().end()
    }

    function rootNoteClicked(rootIndex) {
        getHistory().begin()
        setModified(true)
        setCurrentRoot(rootIndex)
        setCurrentPureTone(rootIndex)
        setCurrentTweak(0.0)
        recalculate(getTuning())
        getHistory().end()
    }

    function pureToneClicked(pureIndex) {
        getHistory().begin()
        setModified(true)
        setCurrentPureTone(pureIndex)
        setCurrentTweak(0.0)
        recalculate(getTuning())
        getHistory().end()
    }

    function tweaked(newValue) {
        getHistory().begin()
        setModified(true)
        setCurrentTweak(newValue)
        recalculate(getTuning())
        getHistory().end()
    }

    function editingFinishedFor(index, newValue, oldValue) {
        getHistory().begin()
        setModified(true)
        getHistory().add(
            function () {
                finalModel[index] = oldValue
                root.finalTextUpdated(index) // no property binding for objects
            },
            function () {
                finalModel[index] = newValue
                root.finalTextUpdated(index)
            },
            "edit ".concat(notesStringModel[index])
        )
        getHistory().end()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: defaultSpacing
        spacing: defaultSpacing
        RowLayout {
            spacing: defaultSpacing
            MU.StyledGroupBox {
                Layout.fillWidth: true
                Layout.minimumWidth: childrenRect.width
                Layout.fillHeight: true
                title: qsTr("Temperament")
                MU.StyledFlickable {
                    height: parent.height
                    width: contentWidth + scrollBar.thickness + 2 * scrollBar.padding
                    focus: true
                    contentWidth: contentItem.childrenRect.width// + 2 * mainColumn.x
                    contentHeight: contentItem.childrenRect.height //+ 2 * mainColumn.y
                    Keys.onUpPressed: scrollBar.decrease()
                    Keys.onDownPressed: scrollBar.increase()
                    ScrollBar.vertical: MU.StyledScrollBar {id: scrollBar}
                    ColumnLayout {
						spacing: 5
						width: childrenRect.width
                        Repeater {
                            model: tuningModel
                            MU.RoundedRadioButton {
								Layout.minimumWidth: implicitWidth
                                text: modelData.displayName
                                checked: index == currentTemperament
                                onToggled: {
                                    temperamentClicked(index)
                                }
                            }
                        }
                    }
                }
            }

            MU.StyledGroupBox {
                Layout.fillHeight: true
                title: "Advanced"
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: defaultSpacing
                    RowLayout {
                        MU.StyledGroupBox {
                            title: "Root Note"
                            Layout.fillWidth: true
                            GridLayout {
                                columns: 4
                                anchors.margins: defaultSpacing

                                Repeater {
                                    model: notesStringModel
                                    MU.RoundedRadioButton {
                                        text: modelData
                                        checked: currentRoot == index
                                        onToggled: {
                                            rootNoteClicked(index)
                                        }
                                    }
                                }
                            }
                        }

                        MU.StyledGroupBox {
                            title: "Pure Tone"
                            Layout.fillWidth: true
                            GridLayout {
                                columns: 4
                                anchors.margins: defaultSpacing

                                Repeater {
                                    model: notesStringModel
                                    MU.RoundedRadioButton {
                                        text: modelData
                                        checked: currentPureTone == index
                                        onToggled: {
                                            pureToneClicked(index)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    MU.StyledGroupBox {
                        title: "Final Offsets"
                        Layout.fillWidth: true
                        GridLayout {
                            columns: 4
                            anchors.margins: defaultSpacing

                            Repeater {
                                model: finalModel
                                id: finalValueRepeater

                                RowLayout {
                                    property alias control: finalValueControl
                                    MU.StyledTextLabel {
                                        text: notesStringModel[index]
                                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        Layout.minimumWidth: 20
                                    }
                                    MU.IncrementalPropertyControl {
                                        Layout.maximumWidth: incrementalControlWidth
                                        id: finalValueControl

                                        currentValue: modelData
                                        decimals: 1
                                        step: 0.1
                                        minValue: -99.9
                                        maxValue: 99.9

                                        onValueEdited: function(newValue) {
                                            if (currentValue !== newValue) {
                                                editingFinishedFor(index, newValue, currentValue)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        spacing: defaultSpacing
                        MU.StyledTextLabel {
                            text: qsTr("Global offset for all pitches")
                            Layout.alignment: Qt.AlignVCenter
                        }
                        MU.IncrementalPropertyControl {
                            Layout.preferredWidth: incrementalControlWidth

                            currentValue: currentTweak
                            decimals: 1
                            step: 0.1
                            minValue: -99.9
                            maxValue: 99.9
                            property var previousValue: 0.0

                            onValueEdited: function(newValue) {
                                if (currentValue !== newValue) {
                                    tweaked(newValue)
                                }
                            }
                        }
                    }
                    RowLayout {
                        anchors.topMargin: defaultSpacing
                        MU.FlatButton {
                            id: saveButton
                            text: qsTranslate("PrefsDialogBase", "Save")
                            enabled: modified
                            onClicked: {
                                options.data = JSON.stringify(formatCurrentValues())
                                modified = false // outside of undo/redo chain
                            }
                        }
                        MU.FlatButton {
                            id: loadButton
                            text: qsTranslate("PrefsDialogBase", "Reset")
                            property int resetIndex: -1
                            enabled: undoIndex != resetIndex && undoIndex > -1
                            onClicked: {
                                getHistory().begin()
                                setCurrentTemperament(0)
                                setCurrentRoot(0)
                                setCurrentPureTone(0)
                                setCurrentTweak(0.0)
                                recalculate(
                                    function(pitch) {
                                        return tuningModel[0].offsets[pitch % 12]
                                    }
                                )
                                getHistory().end()
                                resetIndex = undoIndex
                            }
                        }
                        MU.FlatButton {
                            id: undoButton
                            icon: IconCode.UNDO
                            enabled: undoIndex > -1
                            onClicked: {
                                getHistory().undo()
                            }
                        }
                        MU.FlatButton {
                            id: redoButton
                            icon: IconCode.REDO
                            enabled: undoIndex < undoLength
                            onClicked: {
                                getHistory().redo()
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            MU.CheckBox {
                Layout.fillWidth: true
                id: annotateValue
                text: qsTr("Annotate tunings in score")
                checked: false
                onClicked: {
                    checked = !checked
                }
            }
            MU.FlatButton {
                Layout.fillWidth: true
                text: qsTranslate("PrefsDialogBase", "Cancel")
                onClicked: {
                    if (modified) {
                        cancelQuitDialog.open()
                    } else {
                        quit()
                    }
                }
            }
            MU.FlatButton {
                Layout.fillWidth: true
                text: qsTranslate("PrefsDialogBase", "Apply")
                enabled: curScore
                accentButton: true
                onClicked: {
                    if (modified) {
                        applyQuitDialog.open()
                    } else {
                        applyTemperament()
                        quit()
                    }
                }
            }
        }
    }

    MessageDialog {
        id: errorDialog
        title: "Error"
        text: ""
        onAccepted: {
            errorDialog.close()
        }
    }

    MessageDialog {
        id: applyQuitDialog
        title: "Quit?"
        text: "Do you want to quit the plugin?"
        detailedText: "It looks like you have made customisations to this tuning, you could save them if you like.\r\n" +
            "Click OK to apply the changes anyway, or Cancel to return to the editor."
        standardButtons: [StandardButton.Ok, StandardButton.Cancel]
        onAccepted: {
            if (curScore) {
                applyTemperament()
            }
            quit()
        }
        onRejected: {
            close()
        }
    }

    MessageDialog {
        id: cancelQuitDialog
        title: "Quit?"
        text: "Do you want to quit the plugin?"
        signal openApply
        detailedText: "It looks like you have made customisations to this tuning, you could save them if you like.\r\n"
        standardButtons: [StandardButton.Ok, StandardButton.Cancel]
        onAccepted: {
            quit()
        }
        onRejected: {
            close()
        }
    }

    function formatCurrentValues() {
        var data = {
            edited: true,
            offsets: finalModel,
            temperament: tuningModel[currentTemperament].name,
            root: currentRoot,
            pure: currentPureTone,
            tweak: currentTweak
        };
        return(JSON.stringify(data))
    }

    function restoreSavedValues(data) {
        getHistory().begin()
        setCurrentTemperament(lookupTemperament(data.temperament))
        setCurrentRoot(data.root)
        setCurrentPureTone(data.pure)
        setCurrentTweak(data.tweak)
        recalculate(
            function(pitch) {
                return data.offsets[pitch % 12]
            }
        )
        getHistory().end()
    }

    Settings {
        id: options
        category: "Tuning Plugin"
        property var data: '{
            "edited": false
        }'
    }

    // Command pattern for undo/redo
    function commandHistory() {
        function Command(undo_fn, redo_fn, label) {
            this.undo = undo_fn
            this.redo = redo_fn
            this.label = label // for debugging
        }

        var history = []
        var index = -1
        var transaction = 0
        var maxHistory = 30

        function newHistory(commands) {
            if (index < maxHistory) {
                index++
                history = history.slice(0, index)
            } else {
                history = history.slice(1, index)
            }
            history.push(commands)
        }

        this.add = function(undo, redo, label) {
            var command = new Command(undo, redo, label)
            command.redo()
            if (transaction) {
                history[index].push(command)
            } else {
                newHistory([command])
            }
        }

        this.undo = function() {
            if (index > -1) {
                history[index].slice().reverse().forEach(
                    function(command) {
                        command.undo()
                    }
                )
                index--
            }
            undoIndex = index
        }

        this.redo = function() {
            if ((index + 1) < history.length) {
                index++
                history[index].forEach(
                    function(command) {
                        command.redo()
                    }
                )
            }
            undoIndex = index
        }

        this.begin = function() {
            if (transaction) {
                throw new Error("already in transaction")
            }
            newHistory([])
            transaction = 1
            undoIndex = index
        }

        this.end = function() {
            undoLength = index
            if (!transaction) {
                throw new Error("not in transaction")
            }
            transaction = 0
        }
    }
}
// vim: ft=javascript
