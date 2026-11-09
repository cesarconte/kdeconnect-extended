kdeconnect_add_plugin(kdeconnect_smartducking
    SOURCES
        smartduckingplugin.cpp
)

target_link_libraries(kdeconnect_smartducking
    kdeconnectcore
    kdeconnectdbusinterfaces
    KF6::PulseAudioQt
    KF6::I18n
    KF6::Notifications
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
)

install(FILES "kdeconnect_smartducking_config.qml" DESTINATION ${KDE_INSTALL_DATADIR}/kdeconnect)
