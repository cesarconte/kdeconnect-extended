kdeconnect_add_plugin(kdeconnect_handoff
    SOURCES
        handoffplugin.cpp
)

target_link_libraries(kdeconnect_handoff
    kdeconnectcore
    Qt::Core
    Qt::DBus
    Qt::Gui
    KF6::I18n
    KF6::Notifications
    KF6::KIOWidgets
)
