pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtPositioning
import QtCore
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    objectName: "locationEditor"
    property string locationName: ""
    property real latitude: NaN
    property real longitude: NaN
    property string editingId: ""
    property bool hasPoint: false
    property real mapLatitude: 52.2297
    property real mapLongitude: 21.0122
    property bool showMap: false
    property int mapZoom: 7
    property string positionError: ""
    property bool positionRequested: false
    readonly property var duplicatePlace: hasPoint ? UserPreferences.places.find(place => place.id !== editingId && LocationRules.samePlace(place.latitude, place.longitude, latitude, longitude)) || null : null
    readonly property bool valid: hasPoint && locationName.trim().length > 0 && !duplicatePlace
        && !placeSearch.busy && !positionRequested && !geocoder.busy && LocationRules.valid(latitude, longitude, "gfs")
        && (!manual.checked || (latField.acceptableInput && lonField.acceptableInput))
    onVisibleChanged: if (!visible) { positionRequested = false; placeSearch.query = ""; geocoder.cancel(); }
    spacing: Kirigami.Units.largeSpacing


    function resetDraft(name, lat, lon) {
        geocoder.cancel();
        hasPoint = name.trim().length > 0 && LocationRules.valid(lat, lon, "gfs");
        locationName = name;
        latitude = hasPoint ? lat : NaN; longitude = hasPoint ? lon : NaN;
        mapLatitude = Number.isFinite(lat) ? lat : 52.2297;
        mapLongitude = Number.isFinite(lon) ? lon : 21.0122;
        searchField.text = ""; placeSearch.query = ""; positionError = "";
        positionRequested = false; showMap = false; mapZoom = 7; manual.checked = false;
    }
    function selectPlace(place) {
        geocoder.cancel();
        positionRequested = false;
        locationName = place.name;
        latitude = place.latitude; longitude = place.longitude;
        hasPoint = true;
        mapLatitude = latitude; mapLongitude = longitude;
        searchField.text = place.name; placeSearch.query = "";
        positionError = "";
    }
    function selectLocatedPosition(lat, lon) {
        Qt.inputMethod.hide();
        selectPlace({name: I18n.i18n("My location"), latitude: lat, longitude: lon});
        mapZoom = 12;
        showMap = true;
        geocoder.lookup(lat, lon);
    }
    function requestPosition() {
        Qt.inputMethod.hide();
        positionError = "";
        if (locationPermission.status === Qt.PermissionStatus.Denied) {
            positionError = I18n.i18n("Location access was denied. Allow location access for Meteo in the system settings, then try again.");
            return;
        }
        positionRequested = true;
        if (locationPermission.status === Qt.PermissionStatus.Undetermined) locationPermission.request();
    }
    PlaceSearchModel { id: placeSearch }
    ReverseGeocoder {
        id: geocoder
        onResolved: name => { root.locationName = name; searchField.text = name; }
    }
    LocationPermission {
        id: locationPermission
        accuracy: LocationPermission.Approximate
        availability: LocationPermission.WhenInUse
        onStatusChanged: if (root.positionRequested && status === Qt.PermissionStatus.Denied) {
            root.positionRequested = false;
            root.positionError = I18n.i18n("Location access was denied. Allow location access for Meteo in the system settings, then try again.");
        }
    }
    Loader {
        active: root.positionRequested && locationPermission.status === Qt.PermissionStatus.Granted
        visible: false
        onLoaded: {
            if (!root.positionRequested) return;
            const source = item as PositionSource;
            if (source && source.valid) source.update(15000);
            else {
                root.positionRequested = false;
                root.positionError = I18n.i18n("The location service is unavailable. Choose a place or a point on the map.");
            }
        }
        sourceComponent: PositionSource {
            active: false
            onPositionChanged: {
                if (!root.positionRequested || !position.coordinate.isValid) return;
                root.selectLocatedPosition(position.coordinate.latitude, position.coordinate.longitude);
            }
            onSourceErrorChanged: {
                if (!root.positionRequested || sourceError === PositionSource.NoError) return;
                root.positionRequested = false;
                root.positionError = sourceError === PositionSource.UpdateTimeoutError
                    ? I18n.i18n("Location lookup timed out. You can choose a point on the map.")
                    : I18n.i18n("Could not determine your location. Check the location service or choose a point on the map.");
            }
        }
    }

    SettingsSection {
        title: I18n.i18n("Find a place")
        Layout.fillWidth: true
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
                    function updateQuery() {
                        geocoder.cancel();
                        root.hasPoint = false;
                        // Predictive keyboards (including Samsung Keyboard) can
                        // keep the entire place name in the IME preedit buffer.
                        placeSearch.query = text.slice(0, cursorPosition) + preeditText + text.slice(cursorPosition);
                    }
                    onTextChanged: if (!text.length) placeSearch.query = ""
                    onTextEdited: updateQuery()
                    onPreeditTextChanged: if (activeFocus && (preeditText.length > 0 || !root.hasPoint)) updateQuery()
                    onAccepted: {
                        const queryText = text.slice(0, cursorPosition) + preeditText + text.slice(cursorPosition);
                        Qt.inputMethod.commit();
                        if (!queryText.trim().length) { placeSearch.query = ""; return; }
                        if (placeSearch.results.length && placeSearch.query.trim() === queryText.trim()) root.selectPlace(placeSearch.results[0]);
                        else { placeSearch.query = queryText; placeSearch.search(); }
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
            Pane {
                Layout.fillWidth: true
                visible: placeSearch.results.length > 0
                padding: 0
                Kirigami.Theme.inherit: false
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                contentItem: ColumnLayout {
                    spacing: 0
                    Repeater {
                        model: placeSearch.results
                        delegate: ItemDelegate {
                            id: resultDelegate
                            required property var modelData
                            Layout.fillWidth: true
                            onClicked: root.selectPlace(modelData)
                            contentItem: ColumnLayout {
                                spacing: Kirigami.Units.smallSpacing / 2
                                Label { text: resultDelegate.modelData.name; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                                Label { text: resultDelegate.modelData.description; opacity: 0.7; font: Kirigami.Theme.smallFont; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            }
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
                    objectName: "useMyLocationButton"
                    text: root.positionRequested ? I18n.i18n("Locating…") : I18n.i18n("Use my location")
                    icon.name: "find-location"
                    enabled: !root.positionRequested
                    onClicked: root.requestPosition()
                }
            }
            Kirigami.InlineMessage { visible: root.positionError.length > 0; text: root.positionError; type: Kirigami.MessageType.Information; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                visible: root.positionRequested || geocoder.busy
                BusyIndicator { running: parent.visible; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                Label { Layout.fillWidth: true; text: root.positionRequested ? I18n.i18n("Determining your location…") : I18n.i18n("Finding the place name…"); wrapMode: Text.WordWrap }
            }
            Kirigami.InlineMessage { visible: geocoder.error.length > 0; text: geocoder.error; type: Kirigami.MessageType.Information; Layout.fillWidth: true }
            Loader {
                id: locationMap
                Layout.fillWidth: true
                Layout.preferredHeight: active ? 280 : 0
                active: root.showMap
                visible: active
                sourceComponent: TileMap {
                    objectName: "locatedPointMap"
                    zoom: root.mapZoom
                    latitude: root.mapLatitude; longitude: root.mapLongitude
                    markerLatitude: root.hasPoint ? root.latitude : NaN; markerLongitude: root.hasPoint ? root.longitude : NaN
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
        function onMapLatitudeChanged() { const map = locationMap.item as TileMap; if (map) map.latitude = root.mapLatitude; }
        function onMapLongitudeChanged() { const map = locationMap.item as TileMap; if (map) map.longitude = root.mapLongitude; }
    }
    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: root.duplicatePlace !== null
        text: I18n.i18n("This place is already in your list. Choose it from My places.")
        type: Kirigami.MessageType.Information
    }
    Kirigami.FormLayout {
        id: nameForm
        Layout.fillWidth: true
        visible: root.hasPoint || root.editingId.length > 0 || manual.checked
        TextField {
            objectName: "customLocationName"
            Kirigami.FormData.label: I18n.i18n("Name:")
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: Math.min(root.width, Kirigami.Units.gridUnit * 18)
            text: root.locationName
            maximumLength: 100
            onTextEdited: { geocoder.cancel(); root.locationName = text; }
        }
    }
    ToolButton {
        id: manual
        objectName: "manualLocationToggle"
        text: I18n.i18n("Coordinates")
        icon.name: checked ? "arrow-down" : "arrow-right"
        checkable: true
    }
    Kirigami.FormLayout {
        twinFormLayouts: [nameForm]
        visible: manual.checked
        Layout.fillWidth: true
        TextField {
            id: latField
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            objectName: "manualLatitude"
            Kirigami.FormData.label: I18n.i18n("Latitude (°N):")
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: Math.min(root.width, Kirigami.Units.gridUnit * 18)
            Binding on text { value: Number.isFinite(root.latitude) ? root.latitude.toFixed(6) : ""; when: !latField.activeFocus; restoreMode: Binding.RestoreNone }
            validator: RegularExpressionValidator { regularExpression: /-?\d{1,2}([.,]\d{1,6})?/ }
            onTextEdited: { root.latitude = acceptableInput ? Number(text.replace(",", ".")) : NaN; root.hasPoint = Number.isFinite(root.latitude) && Number.isFinite(root.longitude); }
        }
        TextField {
            id: lonField
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            objectName: "manualLongitude"
            Kirigami.FormData.label: I18n.i18n("Longitude (°E):")
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: Math.min(root.width, Kirigami.Units.gridUnit * 18)
            Binding on text { value: Number.isFinite(root.longitude) ? root.longitude.toFixed(6) : ""; when: !lonField.activeFocus; restoreMode: Binding.RestoreNone }
            validator: RegularExpressionValidator { regularExpression: /-?\d{1,3}([.,]\d{1,6})?/ }
            onTextEdited: { root.longitude = acceptableInput ? Number(text.replace(",", ".")) : NaN; root.hasPoint = Number.isFinite(root.latitude) && Number.isFinite(root.longitude); }
        }
    }
}
