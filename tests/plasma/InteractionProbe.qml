pragma ComponentBehavior: Bound
import QtQuick
import pl.meteo.desktop
import pl.meteo.testfixtures

QtObject {
    id: probe
    required property var applet
    required property WeatherModel weather
    required property var configuration
    required property var actions
    required property string appletIcon
    property int phase: 0
    property int ticks: 0
    property bool sawLoading: false
    property bool sawLocationLoading: false
    readonly property int expectedSectionCount: {
        const revision = weather.dataRevision;
        return weather.configuredCharts(UserPreferences.chartLayoutForProfile(configuration.chartProfile)).length;
    }
    property Timer poll: Timer { interval: 50; running: true; repeat: true; onTriggered: probe.tick() }
    Component.onCompleted: {
        FixtureDriver.prepare();
        if (FixtureDriver.scenario !== "empty") {
            UserPreferences.savePlace("Warszawa", 52.2297, 21.0122);
            UserPreferences.savePlace("Gdańsk", 54.352, 18.6466);
        }
        if (FixtureDriver.scenario === "cached") FixtureDriver.seedCache(weather);
    }
    function find(item, name) {
        if (!item) return null;
        if (item.objectName === name) return item;
        for (const child of item.children ?? []) {
            const found = find(child, name);
            if (found) return found;
        }
        return null;
    }
    function require(condition, message) { if (!condition) throw new Error(message); }
    function action(name) { return Array.from(actions).find(item => item.objectName === name); }
    function finish(callback) {
        poll.stop();
        const frame = applet.fullRepresentationItem;
        require(frame && frame.collapseMarginsHint && frame.footer, "Missing native Plasma frame");
        const pin = find(frame, "pinPopupButton");
        const pinAction = action("pinPopupAction");
        // plasmawindowed forces the initial native window to stay open.
        // Exercise both transitions after that host initialization.
        require(pin && pinAction && !configuration.pinned && !pin.checked, "Missing initial popup pin state");
        pin.clicked();
        require(configuration.pinned && pin.checked && !applet.hideOnWindowDeactivate, "Pin does not keep the native popup open");
        pinAction.trigger();
        require(!configuration.pinned && !pin.checked && applet.hideOnWindowDeactivate, "Context action does not unpin the popup");
        require(FixtureDriver.capture(frame), "Could not capture native container");
        if (callback) callback();
        FixtureDriver.report(true, "Native container, state and actions verified");
    }
    function tick() {
        try {
            if (++ticks > 220) throw new Error("Timed out waiting for widget state, phase " + phase);
            const frame = applet.fullRepresentationItem;
            if (!frame) return;
            const sections = find(frame, "widgetSections");
            if (!sections) return;
            const refresh = action("refreshForecastAction");
            require(refresh && action("openForecastAction"), "Missing native context actions");
            const scenario = FixtureDriver.scenario;
            if (!weather.hasData) require(appletIcon === "pl.meteo.desktop", "Applet fallback is not a theme icon name");
            if (scenario === "empty") {
                require(!weather.locationName.length && !weather.busy && sections.count === 0, "Empty widget starts requests or shows charts");
                finish(); return;
            }
            if (scenario === "loading") {
                if (weather.busy && !weather.hasData) {
                    const indicator = find(frame, "forecastLoadingIndicator");
                    require(indicator && indicator.running && !refresh.enabled, "Loading indicator or disabled refresh is missing");
                    sawLoading = true;
                }
                if (weather.hasData) { require(sawLoading, "Loading state was skipped"); finish(); }
                return;
            }
            if (scenario === "failed") {
                if (!weather.error.length) return;
                require(!weather.hasData && sections.count === 0, "Failed request produced data");
                if (phase === 0) {
                    require(refresh.enabled, "Cannot retry after network error");
                    phase = 1; refresh.trigger();
                } else if (!weather.busy) finish();
                return;
            }
            if (scenario === "cached") {
                if (!weather.error.length || !weather.hasData || sections.count !== expectedSectionCount) return;
                require(weather.usingCache, "Network error discarded cached data");
                const warning = find(frame, "forecastErrorMessage");
                require(warning && warning.visible, "Offline cache has no visible status");
                const button = find(frame, "openMeteogramButton");
                finish(() => button.clicked()); return;
            }
            if (phase === 3 && weather.busy && !weather.hasData) {
                const indicator = find(frame, "forecastLoadingIndicator");
                const icon = find(frame, "forecastWeatherIcon");
                require(indicator && indicator.running && icon && !icon.visible, "Location change shows an empty weather icon instead of a loader");
                sawLocationLoading = true;
            }
            if (!weather.hasData || weather.busy || sections.count !== expectedSectionCount) return;
            const temperature = find(frame, "widgetSection-temperature");
            const precipitation = find(frame, "widgetSection-precipitation");
            if (!temperature || !precipitation) return;
            if (phase === 0) {
                require(temperature.expanded && precipitation.expanded, "Incorrect default section state");
                phase = 1;
                temperature.collapse(); precipitation.collapse();
                FixtureDriver.keyClick(precipitation, Qt.Key_Space);
            } else if (phase === 1 && ticks > 12 && temperature.height < 100) {
                require(!temperature.expanded && precipitation.expanded, "Keyboard or independent collapse failed");
                require(configuration.collapsedCharts.indexOf("temperature") >= 0, "Collapsed section was not saved");
                configuration.placeOrder = ["54.352000,18.646600", "52.229700,21.012200"];
                phase = 2;
            } else if (phase === 2) {
                const selector = find(frame, "headerLocationSelector");
                if (!selector || selector.count !== 2 || selector.model[0].name !== "Gdańsk") return;
                phase = 3; selector.activated(0);
            } else if (phase === 3 && weather.locationName === "Gdańsk") {
                require(sawLocationLoading, "Location change did not show its loading state");
                require(!temperature.expanded, "Section state lost on location change");
                action("choosePlaceAction").trigger();
                phase = 4;
            } else if (phase === 4) {
                const selector = find(frame, "headerLocationSelector");
                require(selector.popup.visible, "Choose location action did not open selector");
                selector.popup.close();
                finish(() => action("openForecastAction").trigger());
            }
        } catch (error) {
            poll.stop(); FixtureDriver.report(false, String(error));
        }
    }
}
