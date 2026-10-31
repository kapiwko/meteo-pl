pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

Dialog {
    id: root
    objectName: "welcomeDialog"
    property string selectedModel: "um4_60"
    property var pendingOptions: null
    signal completed(var options)
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(640, parent ? parent.width - 32 : 640)
    height: Math.min(520, parent ? parent.height - 32 : 520)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    title: wizard.title
    onAboutToShow: {
        wizard.step = 0;
        wizard.useCurrentLocation = true;
        wizard.selectedModel = selectedModel;
        wizard.selectedProfile = UserPreferences.profile;
        wizard.selectedTheme = UserPreferences.theme;
        wizard.selectedWindUnit = UserPreferences.windUnit;
    }
    onClosed: {
        if (!pendingOptions) return;
        const options = pendingOptions;
        pendingOptions = null;
        completed(options);
    }
    contentItem: WelcomeWizard {
        id: wizard
        mobileLayout: false
        showHeader: false
        onCompleted: options => { root.pendingOptions = options; root.accept(); }
        onSkipped: root.reject()
    }
}
