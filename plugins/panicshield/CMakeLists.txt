kdeconnect_add_plugin(kdeconnect_panicshield SOURCES panicshieldplugin.cpp)

target_link_libraries(kdeconnect_panicshield
    kdeconnectcore
    kdeconnectdbusinterfaces
    Qt::Core
    Qt::DBus
    KF6::I18n
    KF6::Notifications
    KF6::PulseAudioQt
)

install(FILES "kdeconnect_panicshield_config.qml" DESTINATION ${KDE_INSTALL_DATADIR}/kdeconnect)
