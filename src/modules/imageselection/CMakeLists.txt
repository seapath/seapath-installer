include_directories(${PROJECT_BINARY_DIR}/src/libcalamaresui)
calamares_add_plugin(imageselection
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        Config.cpp
        ImageSelectionPage.cpp
        ImageSelectionViewStep.cpp
    UI
        ImageSelectionPage.ui
        SeapathFlavorSelection.ui
    LINK_PRIVATE_LIBRARIES
        ${qtname}::DBus
        ${qtname}::Xml
        z
    SHARED_LIB
)

find_package(${qtname} ${QT_VERSION} COMPONENTS Xml)
