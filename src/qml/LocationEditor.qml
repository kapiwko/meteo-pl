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
        showMap = false; manual.checked = false;
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
        onSourceErrorChanged: {
            if (!root.positionRequested || sourceError === PositionSource.NoError) return;
            root.positionRequested = false;
            root.positionError = sourceError === PositionSource.UpdateTimeoutError
                ? "Upłynął czas ustalania pozycji. Możesz wybrać punkt z mapy."
                : "Nie udało się ustalić pozycji. Sprawdź usługę lokalizacji lub wybierz punkt z mapy.";
        }
    }

    GroupBox {
        title: "Znajdź miejsce"
        Layout.fillWidth: true
        padding: Kirigami.Units.largeSpacing
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            RowLayout {
                Layout.fillWidth: true
                Kirigami.SearchField {
                    id: searchField
                    objectName: "placeSearchField"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    placeholderText: "Nazwa miasta lub wsi…"
                    Accessible.name: "Wyszukaj miejscowość"
                    autoAccept: false
                    onTextChanged: if (!text.length) placeSearch.query = ""
                    onTextEdited: placeSearch.query = text
                    onAccepted: {
                        if (!text.length) { placeSearch.query = ""; return; }
                        if (placeSearch.results.length) root.selectPlace(placeSearch.results[0]); else placeSearch.search();
                    }
                }
                Item {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                    visible: placeSearch.busy
                    BusyIndicator { anchors.fill: parent; running: placeSearch.busy }
                }
            }
            CheckBox { text: "Szukaj także poza Polską"; checked: !placeSearch.polandOnly; onToggled: placeSearch.polandOnly = !checked }
            Frame {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(220, searchResults.contentHeight + padding * 2)
                visible: placeSearch.results.length > 0
                padding: 0
                contentItem: ListView {
                    id: searchResults
                    clip: true
                    model: placeSearch.results
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        id: resultDelegate
                        required property var modelData
                        width: ListView.view.width
                        onClicked: root.selectPlace(modelData)
                        contentItem: ColumnLayout {
                            spacing: Kirigami.Units.smallSpacing / 2
                            Label { text: resultDelegate.modelData.name; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: resultDelegate.modelData.description; color: Kirigami.Theme.disabledTextColor; font: Kirigami.Theme.smallFont; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        }
                    }
                }
            }
            Label {
                visible: placeSearch.query.length >= 2 && !placeSearch.busy && !placeSearch.results.length && !placeSearch.error
                text: "Nie znaleziono miejscowości. Doprecyzuj nazwę lub wybierz punkt z mapy."
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            Kirigami.InlineMessage { visible: placeSearch.error.length > 0; text: placeSearch.error; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
            Flow {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                Button { text: "Wybierz na mapie"; icon.name: "map-globe"; checkable: true; checked: root.showMap; onToggled: root.showMap = checked }
                Button {
                    text: root.positionRequested ? "Ustalanie pozycji…" : "Użyj mojej pozycji"
                    icon.name: "find-location"
                    enabled: !root.positionRequested
                    onClicked: {
                        root.positionError = "";
                        if (positionSource.valid) { root.positionRequested = true; positionSource.update(15000); }
                        else root.positionError = "Usługa lokalizacji jest niedostępna. Wybierz miejscowość lub punkt z mapy.";
                    }
                }
            }
            Kirigami.InlineMessage { visible: root.positionError.length > 0; text: root.positionError; type: Kirigami.MessageType.Information; Layout.fillWidth: true }
            Loader {
                id: locationMap
                Layout.fillWidth: true
                Layout.preferredHeight: active ? 280 : 0
                active: root.showMap
                visible: active
                sourceComponent: TileMap {
                    latitude: root.latitude; longitude: root.longitude
                    markerLatitude: root.latitude; markerLongitude: root.longitude
                    markerName: root.locationName
                    places: UserPreferences.places
                    onPlaceSelected: (name, latitude, longitude) => root.selectPlace({name: name, latitude: latitude, longitude: longitude})
                    onCoordinateSelected: (latitude, longitude) => root.selectPlace({name: "Punkt na mapie", latitude: Number(latitude.toFixed(6)), longitude: Number(longitude.toFixed(6))})
                }
            }
        }
    }
    Connections {
        target: root
        function onLatitudeChanged() { const map = locationMap.item as TileMap; if (map) map.latitude = root.latitude; }
        function onLongitudeChanged() { const map = locationMap.item as TileMap; if (map) map.longitude = root.longitude; }
    }
    GroupBox {
        title: "Moje miejsca"
        visible: UserPreferences.places.length > 0
        Layout.fillWidth: true
        padding: Kirigami.Units.smallSpacing
        contentItem: ListView {
            implicitHeight: Math.min(160, contentHeight)
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
    GroupBox {
        title: "Prognoza"
        Layout.fillWidth: true
        padding: Kirigami.Units.largeSpacing
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            Kirigami.FormLayout {
                Layout.fillWidth: true
                Label {
                    Kirigami.FormData.label: "Miejsce:"
                    Layout.fillWidth: true
                    text: root.locationName.length ? root.locationName : "Wybierz miejscowość lub punkt na mapie"
                    font.bold: root.locationName.length > 0
                    wrapMode: Text.WordWrap
                }
                ComboBox {
                    objectName: "locationModelSelector"
                    Kirigami.FormData.label: "Model:"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Accessible.name: "Model prognozy"
                    model: ["UM4 · 60 godzin", "UM4 · 120 godzin", "GFS · 240 godzin"]
                    currentIndex: ["um4_60", "um4_120", "gfs"].indexOf(root.model)
                    onActivated: index => root.model = ["um4_60", "um4_120", "gfs"][index]
                }
            }
            Label {
                Layout.fillWidth: true
                text: root.model === "gfs" ? "GFS: prognoza dla całego świata." : "UM4: szczegółowa prognoza dla Polski i Europy Środkowej."
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                wrapMode: Text.WordWrap
            }
            Kirigami.InlineMessage { Layout.fillWidth: true; visible: root.outsideModel; text: "Ten punkt leży poza obszarem UM4. Wybierz model GFS."; type: Kirigami.MessageType.Warning }
        }
    }
    ToolButton {
        id: manual
        objectName: "manualLocationToggle"
        text: "Własna nazwa i współrzędne"
        icon.name: checked ? "arrow-down" : "arrow-right"
        checkable: true
    }
    Kirigami.FormLayout {
        visible: manual.checked
        Layout.fillWidth: true
        TextField {
            objectName: "customLocationName"
            Kirigami.FormData.label: "Nazwa:"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.locationName
            maximumLength: 100
            onTextEdited: root.locationName = text
        }
        TextField {
            id: latField
            Kirigami.FormData.label: "Szerokość (°N):"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Binding on text { value: String(root.latitude); when: !latField.activeFocus; restoreMode: Binding.RestoreNone }
            validator: RegularExpressionValidator { regularExpression: /-?\d{1,2}([.,]\d{1,6})?/ }
            onTextEdited: if (acceptableInput) root.latitude = Number(text.replace(",", "."));
        }
        TextField {
            id: lonField
            Kirigami.FormData.label: "Długość (°E):"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Binding on text { value: String(root.longitude); when: !lonField.activeFocus; restoreMode: Binding.RestoreNone }
            validator: RegularExpressionValidator { regularExpression: /-?\d{1,3}([.,]\d{1,6})?/ }
            onTextEdited: if (acceptableInput) root.longitude = Number(text.replace(",", "."));
        }
    }
}
