find_package(LayerShellQt QUIET)

kdeconnect_add_plugin(kdeconnect_spatialflick
    SOURCES
        spatialflickplugin.cpp
        edgeportaloverlay.cpp
)

target_link_libraries(kdeconnect_spatialflick
    kdeconnectcore
    kdeconnectdbusinterfaces
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    KF6::I18n
    KF6::Notifications
)

if(TARGET LayerShellQt::Interface)
    target_link_libraries(kdeconnect_spatialflick LayerShellQt::Interface)
    target_compile_definitions(kdeconnect_spatialflick PRIVATE -DHAVE_LAYERSHELLQT)
endif()

install(FILES "kdeconnect_spatialflick_config.qml" DESTINATION ${KDE_INSTALL_DATADIR}/kdeconnect)
