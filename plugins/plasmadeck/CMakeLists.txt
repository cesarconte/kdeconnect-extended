kdeconnect_add_plugin(kdeconnect_plasmadeck
    SOURCES
        plasmadeckplugin.cpp
        deckserver.cpp
)

target_link_libraries(kdeconnect_plasmadeck
    kdeconnectcore
    kdeconnectdbusinterfaces
    KF6::I18n
    KF6::Notifications
    KF6::WindowSystem
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Network
)

install(FILES "kdeconnect_plasmadeck_config.qml" DESTINATION ${KDE_INSTALL_DATADIR}/kdeconnect)
