kdeconnect_add_plugin(kdeconnect_clipboard SOURCES clipboardplugin.cpp clipboardlistener.cpp)

target_link_libraries(kdeconnect_clipboard kdeconnectcore
    Qt::DBus
    KF6::GuiAddons
    KF6::I18n
    KF6::Notifications
    ${kdeconnect_clipboard_WL_LINK_LIBS}
)

install(FILES "kdeconnect_clipboard_config.qml" DESTINATION ${KDE_INSTALL_DATADIR}/kdeconnect)
