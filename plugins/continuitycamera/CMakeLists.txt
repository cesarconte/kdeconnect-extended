find_package(PkgConfig REQUIRED)
pkg_check_modules(PipeWire REQUIRED IMPORTED_TARGET libpipewire-0.3)

kdeconnect_add_plugin(kdeconnect_continuitycamera
    SOURCES
        continuitycameraplugin.cpp
        continuitycameraengine.cpp
)

target_link_libraries(kdeconnect_continuitycamera
    kdeconnectcore
    Qt::Core
    Qt::Gui
    Qt::DBus
    PkgConfig::PipeWire
)
