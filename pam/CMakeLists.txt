find_library(PAM_LIBRARY pam REQUIRED)

add_library(pam_kdeconnect MODULE
    pam_kdeconnect.c
)

target_link_libraries(pam_kdeconnect
    PRIVATE
        ${PAM_LIBRARY}
)

set_target_properties(pam_kdeconnect PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pam_kdeconnect"
)

install(TARGETS pam_kdeconnect DESTINATION ${KDE_INSTALL_LIBDIR}/security)
