add_definitions(-DTRANSLATION_DOMAIN="krunner_kdeconnect")

find_package(KF6Runner ${KF_MIN_VERSION} REQUIRED)

kcoreaddons_add_plugin(krunner_kdeconnect
    SOURCES kdeconnectrunner.cpp
    INSTALL_NAMESPACE "kf6/krunner"
)

target_link_libraries(krunner_kdeconnect
    KF6::Runner
    KF6::I18n
    kdeconnectcore
    kdeconnectdbusinterfaces
    Qt::DBus
    Qt::Core
)
