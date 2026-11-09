kdeconnect_add_plugin(kdeconnect_spatialpresence SOURCES spatialpresenceplugin.cpp)

target_link_libraries(kdeconnect_spatialpresence
    kdeconnectcore
    kdeconnectdbusinterfaces
    Qt::Core
    Qt::DBus
    KF6::I18n
    KF6::Notifications
)

install(FILES "kdeconnect_spatialpresence_config.qml" DESTINATION ${KDE_INSTALL_DATADIR}/kdeconnect)
