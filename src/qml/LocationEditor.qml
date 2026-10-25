import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    property alias locationName: nameField.text
    property real latitude: 52.2297
    property real longitude: 21.0122
    property string model: "um4_60"
    readonly property var cities: [
        { name: "Warszawa", lat: 52.2297, lon: 21.0122 },
        { name: "Kielce", lat: 50.8661, lon: 20.6286 },
        { name: "Kraków", lat: 50.0647, lon: 19.9450 },
        { name: "Gdańsk", lat: 54.3520, lon: 18.6466 },
        { name: "Wrocław", lat: 51.1079, lon: 17.0385 },
        { name: "Poznań", lat: 52.4064, lon: 16.9252 },
        { name: "Łódź", lat: 51.7592, lon: 19.4560 },
        { name: "Zakopane", lat: 49.2992, lon: 19.9496 }
    ]
    readonly property bool valid: nameField.text.trim().length > 0 && latField.acceptableInput && lonField.acceptableInput
        && latitude >= -90 && latitude <= 90 && longitude >= -180 && longitude <= 180
        && (model === "gfs" || (latitude >= 44.5 && latitude <= 66.482 && longitude >= 8.21 && longitude <= 30.176))
    spacing: Kirigami.Units.largeSpacing

    function numberFrom(text) { return Number(text.trim().replace(",", ".")); }

    Label { text: "Miasto" }
    ComboBox {
        Layout.fillWidth: true
        model: root.cities
        textRole: "name"
        currentIndex: -1
        displayText: currentIndex < 0 ? "Wybierz miasto…" : currentText
        onActivated: index => {
            const city = root.cities[index];
            root.locationName = city.name;
            root.latitude = city.lat;
            root.longitude = city.lon;
        }
    }
    Label { text: "Nazwa lokalizacji" }
    TextField { id: nameField; Layout.fillWidth: true; placeholderText: "Np. Dom, Kielce, szlak…"; maximumLength: 100 }
    Label { text: "Szerokość geograficzna (°N)" }
    TextField {
        id: latField
        Layout.fillWidth: true
        text: String(root.latitude)
        validator: RegularExpressionValidator { regularExpression: /-?\d{1,2}([.,]\d{1,6})?/ }
        onTextEdited: if (acceptableInput) root.latitude = root.numberFrom(text)
    }
    Label { text: "Długość geograficzna (°E)" }
    TextField {
        id: lonField
        Layout.fillWidth: true
        text: String(root.longitude)
        validator: RegularExpressionValidator { regularExpression: /-?\d{1,3}([.,]\d{1,6})?/ }
        onTextEdited: if (acceptableInput) root.longitude = root.numberFrom(text)
    }
    Label { text: "Model prognozy" }
    ComboBox {
        Layout.fillWidth: true
        model: ["UM4 · 60 godzin", "UM4 · 120 godzin", "GFS · 240 godzin"]
        currentIndex: ["um4_60", "um4_120", "gfs"].indexOf(root.model)
        onActivated: index => root.model = ["um4_60", "um4_120", "gfs"][index]
    }
    Label {
        Layout.fillWidth: true
        text: root.valid ? "Możesz podać współrzędne dowolnego punktu. GFS obejmuje cały świat." : "Podaj nazwę i poprawne współrzędne. Dla lokalizacji poza obszarem UM4 wybierz GFS."
        wrapMode: Text.WordWrap
        opacity: 0.7
    }
}
