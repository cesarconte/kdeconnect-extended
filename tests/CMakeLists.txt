find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)

set(kdeconnect_libraries
    kdeconnectcore
    kdeconnectdbusinterfaces
    kdeconnectmodels
    kdeconnectsmshelper
    kdeconnectversion
    KF6::GuiAddons
    KF6::I18n
    KF6::KIOWidgets
    KF6::Notifications
    Qt::DBus
    Qt::Network
    KF6::People
    Qt::Qml
    Qt::Test
)

ecm_add_test(notificationstest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
ecm_add_test(pluginloadtest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
ecm_add_test(sendfiletest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
target_sources(sendfiletest PRIVATE testdevice.cpp)
ecm_add_test(smshelpertest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
ecm_add_test(lanratelimittest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
ecm_add_test(lanipv6test.cpp LINK_LIBRARIES ${kdeconnect_libraries})
ecm_add_test(clipboardttltest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
ecm_add_test(handofftest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
find_package(KF6Runner ${KF_MIN_VERSION} REQUIRED)
ecm_add_test(krunnermatchtest.cpp LINK_LIBRARIES ${kdeconnect_libraries} KF6::Runner)
ecm_add_test(proximityunlocktest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
if (WIN32 OR APPLE)
    ecm_add_test(mdnstest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
endif()

if(NOT APPLE AND NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PipeWire REQUIRED IMPORTED_TARGET libpipewire-0.3)
    ecm_add_test(continuitymictest.cpp 
        LINK_LIBRARIES 
            ${kdeconnect_libraries}
            PkgConfig::PipeWire
    )
    target_sources(continuitymictest PRIVATE ../plugins/continuitymic/continuitymicengine.cpp)
    target_include_directories(continuitymictest PRIVATE ../plugins/continuitymic)
endif()

# Do not touch the clipboard of the running session (and avoid a KSystemClipboard deadlock on exit under Wayland)
set_tests_properties(notificationstest pluginloadtest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
