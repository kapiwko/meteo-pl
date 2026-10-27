pragma ComponentBehavior: Bound
import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.InlineMessage {
    id: root
    objectName: "profileUpdateNotice"
    property string profileId: UserPreferences.profile
    readonly property var currentProfile: UserPreferences.profiles.find(profile => profile.id === root.profileId) || UserPreferences.profiles[0] || {}
    visible: currentProfile.updateAvailable === true
    type: Kirigami.MessageType.Information
    text: I18n.i18n("A newer version of this preset is available (revision %1). Updating replaces your changes and keeps an old copy.").arg(currentProfile.availableRevision || 1)
    actions: [Kirigami.Action {
        text: I18n.i18n("Update and keep old copy")
        icon.name: "view-refresh"
        onTriggered: UserPreferences.updateProfile(root.currentProfile.id || root.profileId)
    }]
}
