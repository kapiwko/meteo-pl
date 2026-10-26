pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtPositioning
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    objectName: "locationEditor"
    property string locationName: "Warszawa"
    property real latitude: 52.2297
    property real longitude: 21.0122
    property string model: "um4_60"
    property bool showMap: false
    property string positionError: ""
    property bool positionRequested: false
    readonly property bool valid: locationName.trim().length > 0 && LocationRules.valid(latitude, longitude, model)
        && (!manual.checked || (latField.acceptableInput && lonField.acceptableInput))
    readonly property bool outsideModel: model !== "gfs" && !LocationRules.valid(latitude, longitude, model)
    readonly property bool savedPlace: locationName.trim().length > 0 && UserPreferences.places.some(place => LocationRules.samePlace(place.latitude, place.longitude, root.latitude, root.longitude))
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
            root.selectPlace({name: I18n.i18n("My location"), latitude: position.coordinate.latitude, longitude: position.coordinate.longitude});
            root.positionRequested = false;
        }
        onSourceErrorChanged: {
            if (!root.positionRequested || sourceError === PositionSource.NoError) return;
            root.positionRequested = false;
            root.positionError = sourceError === PositionSource.UpdateTimeoutError
                ? I18n.i18n("Location lookup timed out. You can choose a point on the map.")
                : I18n.i18n("Could not determine your location. Check the location service or choose a point on the map.");
        }
    }

    GroupBox {
        title: I18n.i18n("Find a place")
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
                    placeholderText: I18n.i18n("Town or village name…")
                    Accessible.name: I18n.i18n("Search for a place")
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
            CheckBox { text: I18n.i18n("Search outside Poland too"); checked: !placeSearch.polandOnly; onToggled: placeSearch.polandOnly = !checked }
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
                text: I18n.i18n("No places found. Refine the name or choose a point on the map.")
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            Kirigami.InlineMessage { visible: placeSearch.error.length > 0; text: placeSearch.error; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
            Flow {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                Button { text: I18n.i18n("Choose on map"); icon.name: "map-globe"; checkable: true; checked: root.showMap; onToggled: root.showMap = checked }
                Button {
                    text: root.positionRequested ? I18n.i18n("Locating…") : I18n.i18n("Use my location")
                    icon.name: "find-location"
                    enabled: !root.positionRequested
                    onClicked: {
                        root.positionError = "";
                        if (positionSource.valid) { root.positionRequested = true; positionSource.update(15000); }
                        else root.positionError = I18n.i18n("The location service is unavailable. Choose a place or a point on the map.");
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
                    onCoordinateSelected: (latitude, longitude) => root.selectPlace({name: I18n.i18n("Point on map"), latitude: Number(latitude.toFixed(6)), longitude: Number(longitude.toFixed(6))})
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
        title: I18n.i18n("My places")
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
                highlighted: LocationRules.samePlace(modelData.latitude, modelData.longitude, root.latitude, root.longitude)
                onClicked: root.selectPlace(modelData)
                contentItem: RowLayout {
                    Label { text: savedPlace.modelData.name; Layout.fillWidth: true; elide: Text.ElideRight }
                    ToolButton {
                        icon.name: "edit-delete"
                        onClicked: {
                            const selected = LocationRules.samePlace(savedPlace.modelData.latitude, savedPlace.modelData.longitude, root.latitude, root.longitude);
                            UserPreferences.removePlace(savedPlace.modelData.latitude, savedPlace.modelData.longitude);
                            if (selected) {
                                const first = UserPreferences.places[0];
                                if (first) root.selectPlace(first);
                                else root.resetDraft("", 0, 0, root.model);
                            }
                        }
                        ToolTip.text: I18n.i18n("Remove place: ") + savedPlace.modelData.name
                        ToolTip.visible: hovered
                        Accessible.name: ToolTip.text
                    }
                }
            }
        }
    }
    GroupBox {
        title: I18n.i18n("Forecast")
        Layout.fillWidth: true
        padding: Kirigami.Units.largeSpacing
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            Kirigami.FormLayout {
                Layout.fillWidth: true
                Label {
                    Kirigami.FormData.label: I18n.i18n("Place:")
                    Layout.fillWidth: true
                    text: root.locationName.length ? root.locationName : I18n.i18n("Choose a place or a point on the map")
                    font.bold: root.locationName.length > 0
                    wrapMode: Text.WordWrap
                }
                ComboBox {
                    objectName: "locationModelSelector"
                    Kirigami.FormData.label: I18n.i18n("Model:")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Accessible.name: I18n.i18n("Forecast model")
                    model: [I18n.i18n("UM4 · 60 hours"), I18n.i18n("UM4 · 120 hours"), I18n.i18n("GFS · 240 hours")]
                    currentIndex: ["um4_60", "um4_120", "gfs"].indexOf(root.model)
                    onActivated: index => root.model = ["um4_60", "um4_120", "gfs"][index]
                }
            }
            Label {
                Layout.fillWidth: true
                text: root.model === "gfs" ? I18n.i18n("GFS: worldwide forecast.") : I18n.i18n("UM4: detailed forecasts for Poland and Central Europe.")
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                wrapMode: Text.WordWrap
            }
            Kirigami.InlineMessage { Layout.fillWidth: true; visible: root.outsideModel; text: I18n.i18n("This point is outside the UM4 area. Choose the GFS model."); type: Kirigami.MessageType.Warning }
        }
    }
    ToolButton {
        id: manual
        objectName: "manualLocationToggle"
        text: I18n.i18n("Custom name and coordinates")
        icon.name: checked ? "arrow-down" : "arrow-right"
        checkable: true
    }
    Kirigami.FormLayout {
        visible: manual.checked
        Layout.fillWidth: true
        TextField {
            objectName: "customLocationName"
            Kirigami.FormData.label: I18n.i18n("Name:")
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.locationName
            maximumLength: 100
            onTextEdited: root.locationName = text
        }
        TextField {
            id: latField
            objectName: "manualLatitude"
            Kirigami.FormData.label: I18n.i18n("Latitude (°N):")
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Binding on text { value: root.latitude.toFixed(6); when: !latField.activeFocus; restoreMode: Binding.RestoreNone }
            validator: RegularExpressionValidator { regularExpression: /-?\d{1,2}([.,]\d{1,6})?/ }
            onTextEdited: if (acceptableInput) root.latitude = Number(text.replace(",", "."));
        }
        TextField {
            id: lonField
            objectName: "manualLongitude"
            Kirigami.FormData.label: I18n.i18n("Longitude (°E):")
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Binding on text { value: root.longitude.toFixed(6); when: !lonField.activeFocus; restoreMode: Binding.RestoreNone }
            validator: RegularExpressionValidator { regularExpression: /-?\d{1,3}([.,]\d{1,6})?/ }
            onTextEdited: if (acceptableInput) root.longitude = Number(text.replace(",", "."));
        }
    }
}
