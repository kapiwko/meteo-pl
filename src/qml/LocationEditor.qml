pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtPositioning
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    property string locationName: "Warszawa"
    property real latitude: 52.2297
    property real longitude: 21.0122
    property string model: "um4_60"
    property bool showMap: false
    property string positionError: ""
    property bool positionRequested: false
    readonly property bool valid: locationName.trim().length > 0 && latField.acceptableInput && lonField.acceptableInput
        && latitude >= -90 && latitude <= 90 && longitude >= -180 && longitude <= 180
        && (model === "gfs" || (latitude >= 44.5 && latitude <= 66.482 && longitude >= 8.21 && longitude <= 30.176))
    readonly property bool outsideModel: model !== "gfs" && !(latitude >= 44.5 && latitude <= 66.482 && longitude >= 8.21 && longitude <= 30.176)
    spacing: Kirigami.Units.largeSpacing

    function resetDraft(name, lat, lon, forecastModel) {
        locationName = name; latitude = lat; longitude = lon; model = forecastModel;
        searchField.text = ""; placeSearch.query = ""; positionError = "";
    }
    function selectPlace(place) {
        locationName = place.name;
        latitude = place.latitude; longitude = place.longitude;
        searchField.text = place.name; placeSearch.query = "";
        positionError = "";
    }
    PlaceSearchModel { id: placeSearch }
    PositionSource {
        id: positionSource
        active: false
        onPositionChanged: {
            if (!position.coordinate.isValid) return;
            root.selectPlace({name: "Moja pozycja", latitude: position.coordinate.latitude, longitude: position.coordinate.longitude});
            root.positionRequested = false;
        }
        onSourceErrorChanged: if (root.positionRequested && sourceError !== PositionSource.NoError) root.positionError = sourceError === PositionSource.UpdateTimeoutError
            ? "Upłynął czas ustalania pozycji. Możesz wybrać punkt z mapy."
            : "Nie udało się ustalić pozycji. Sprawdź usługę lokalizacji lub wybierz punkt z mapy.";
    }

    Label { text: "Znajdź miejscowość"; font.bold: true }
    RowLayout {
        Layout.fillWidth: true
        TextField {
            id: searchField
            objectName: "placeSearchField"
            Layout.fillWidth: true
            placeholderText: "Wpisz nazwę miasta lub wsi…"
            selectByMouse: true
            onTextEdited: placeSearch.query = text
            onAccepted: if (placeSearch.results.length) root.selectPlace(placeSearch.results[0]); else placeSearch.search();
        }
        BusyIndicator { running: placeSearch.busy; visible: running; Layout.preferredWidth: 30; Layout.preferredHeight: 30 }
        ToolButton { icon.name: "edit-clear"; visible: searchField.text.length > 0; onClicked: { searchField.clear(); placeSearch.query = ""; searchField.forceActiveFocus(); } }
    }
    CheckBox { text: "Szukaj także poza Polską"; checked: !placeSearch.polandOnly; onToggled: placeSearch.polandOnly = !checked }
    Pane {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(260, searchResults.contentHeight + padding * 2)
        visible: placeSearch.results.length > 0
        padding: 2
        ListView {
            id: searchResults
            anchors.fill: parent
            clip: true
            model: placeSearch.results
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: resultDelegate
                required property var modelData
                width: ListView.view.width
                onClicked: root.selectPlace(modelData)
                contentItem: ColumnLayout {
                    spacing: 2
                    Label { text: resultDelegate.modelData.name; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: resultDelegate.modelData.description; opacity: 0.65; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                }
            }
        }
    }
    Label { visible: placeSearch.query.length >= 2 && !placeSearch.busy && !placeSearch.results.length && !placeSearch.error; text: "Nie znaleziono miejscowości. Doprecyzuj nazwę lub wybierz punkt z mapy."; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    Kirigami.InlineMessage { visible: placeSearch.error.length > 0; text: placeSearch.error; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
    GroupBox {
        title: "Moje miejsca"
        visible: UserPreferences.places.length > 0
        Layout.fillWidth: true
        padding: Kirigami.Units.smallSpacing
        contentItem: ListView {
            implicitHeight: Math.min(180, contentHeight)
            clip: true
            model: UserPreferences.places
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: savedPlace
                required property var modelData
                width: ListView.view.width
                highlighted: Math.abs(modelData.latitude-root.latitude) < 0.000001 && Math.abs(modelData.longitude-root.longitude) < 0.000001
                onClicked: root.selectPlace(modelData)
                contentItem: RowLayout {
                    Label { text: savedPlace.modelData.name; Layout.fillWidth: true; elide: Text.ElideRight }
                    ToolButton {
                        icon.name: "edit-delete"
                        onClicked: {
                            const selected = Math.abs(savedPlace.modelData.latitude-root.latitude) < 0.000001 && Math.abs(savedPlace.modelData.longitude-root.longitude) < 0.000001;
                            UserPreferences.removePlace(savedPlace.modelData.latitude, savedPlace.modelData.longitude);
                            if (selected) {
                                const first = UserPreferences.places[0];
                                if (first) root.selectPlace(first);
                                else root.resetDraft("", 0, 0, root.model);
                            }
                        }
                        ToolTip.text: "Usuń miejsce: " + savedPlace.modelData.name
                        ToolTip.visible: hovered
                        Accessible.name: ToolTip.text
                    }
                }
            }
        }
    }
    Frame {
        Layout.fillWidth: true
        padding: 12
        RowLayout {
            anchors.fill: parent
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: root.locationName; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { text: root.latitude.toFixed(4) + "°N, " + root.longitude.toFixed(4) + "°E"; opacity: 0.65 }
            }

        }
    }
    RowLayout {
        Layout.fillWidth: true
        Button { text: "Wybierz na mapie"; icon.name: "map-globe"; checkable: true; checked: root.showMap; onToggled: root.showMap = checked }
        Button { text: "Moja pozycja"; icon.name: "find-location"; onClicked: { root.positionError = ""; root.positionRequested = true; if (positionSource.valid) positionSource.update(15000); else root.positionError = "Usługa lokalizacji jest niedostępna. Wybierz miejscowość lub punkt z mapy."; } }
    }
    Kirigami.InlineMessage { visible: root.positionError.length > 0; text: root.positionError; type: Kirigami.MessageType.Information; Layout.fillWidth: true }
    Loader {
        id: locationMap
        Layout.fillWidth: true
        Layout.preferredHeight: active ? 300 : 0
        active: root.showMap
        sourceComponent: TileMap {
            latitude: root.latitude; longitude: root.longitude
            markerLatitude: root.latitude; markerLongitude: root.longitude
            onCoordinateSelected: (latitude, longitude) => root.selectPlace({name: "Punkt na mapie", latitude: Number(latitude.toFixed(6)), longitude: Number(longitude.toFixed(6))})
        }
    }
    Connections {
        target: root
        function onLatitudeChanged() { const map = locationMap.item as TileMap; if (map) map.latitude = root.latitude; }
        function onLongitudeChanged() { const map = locationMap.item as TileMap; if (map) map.longitude = root.longitude; }
    }
    CheckBox { id: manual; text: "Własna nazwa i współrzędne" }
    GridLayout {
        visible: manual.checked
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 12
        Label { text: "Nazwa" }
        TextField { Layout.fillWidth: true; text: root.locationName; maximumLength: 100; onTextEdited: root.locationName = text }
        Label { text: "Szerokość (°N)" }
        TextField {
            id: latField
            Layout.fillWidth: true
            Binding on text { value: String(root.latitude); when: !latField.activeFocus; restoreMode: Binding.RestoreNone }
            validator: RegularExpressionValidator { regularExpression: /-?\d{1,2}([.,]\d{1,6})?/ }
            onTextEdited: if (acceptableInput) root.latitude = Number(text.replace(",", "."));
        }
        Label { text: "Długość (°E)" }
        TextField {
            id: lonField
            Layout.fillWidth: true
            Binding on text { value: String(root.longitude); when: !lonField.activeFocus; restoreMode: Binding.RestoreNone }
            validator: RegularExpressionValidator { regularExpression: /-?\d{1,3}([.,]\d{1,6})?/ }
            onTextEdited: if (acceptableInput) root.longitude = Number(text.replace(",", "."));
        }
    }
    Label { text: "Model prognozy"; font.bold: true }
    ComboBox {
        Layout.fillWidth: true
        model: ["UM4 · 60 godzin · Europa Środkowa", "UM4 · 120 godzin · Europa Środkowa", "GFS · 240 godzin · cały świat"]
        currentIndex: ["um4_60", "um4_120", "gfs"].indexOf(root.model)
        onActivated: index => root.model = ["um4_60", "um4_120", "gfs"][index]
    }
    Kirigami.InlineMessage { Layout.fillWidth: true; visible: root.outsideModel; text: "Ten punkt leży poza obszarem UM4. Wybierz model GFS."; type: Kirigami.MessageType.Warning }
}
