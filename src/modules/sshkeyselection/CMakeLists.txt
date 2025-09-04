include_directories(${PROJECT_BINARY_DIR}/src/libcalamaresui)
calamares_add_plugin(sshkeyselection
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        Config.cpp
        SshKeySelectionPage.cpp
        SshKeySelectionViewStep.cpp
    UI
        SshKeySelectionPage.ui
    LINK_PRIVATE_LIBRARIES
        ${qtname}::DBus
    SHARED_LIB
)
