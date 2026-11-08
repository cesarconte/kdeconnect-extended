kdeconnect_add_plugin(kdeconnect_biometricauth
    SOURCES
        biometricauthplugin.cpp
)

target_link_libraries(kdeconnect_biometricauth
    kdeconnectcore
    Qt::Core
    Qt::DBus
    Qt::Network
    KF6::I18n
    KF6::Notifications
)
