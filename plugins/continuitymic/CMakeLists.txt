find_package(PkgConfig REQUIRED)
pkg_check_modules(PipeWire REQUIRED IMPORTED_TARGET libpipewire-0.3)

kdeconnect_add_plugin(kdeconnect_continuitymic
    SOURCES
        continuitymicplugin.cpp
        continuitymicengine.cpp
)

target_link_libraries(kdeconnect_continuitymic
    kdeconnectcore
    Qt::Core
    Qt::DBus
    PkgConfig::PipeWire
)
