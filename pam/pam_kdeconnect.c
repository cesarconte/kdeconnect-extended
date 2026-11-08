/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define PAM_SM_AUTH
#include <security/pam_ext.h>
#include <security/pam_modules.h>

#define KDECONNECT_DEFAULT_TIMEOUT 25

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    (void)pamh;
    (void)flags;
    (void)argc;
    (void)argv;
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    (void)flags;
    (void)argc;
    (void)argv;

    const char *username = NULL;
    if (pam_get_user(pamh, &username, NULL) != PAM_SUCCESS || !username || !*username) {
        return PAM_IGNORE;
    }

    // Resolver UID del usuario invocador
    struct passwd *pw = getpwnam(username);
    uid_t uid = pw ? pw->pw_uid : getuid();

    // Si se ejecuta mediante sudo, SUDO_UID contiene el UID real del usuario en la sesión
    const char *sudo_uid_env = getenv("SUDO_UID");
    if (sudo_uid_env && *sudo_uid_env) {
        uid_t suid = (uid_t)atoi(sudo_uid_env);
        if (suid >= 1000) {
            uid = suid;
        }
    }

    char sock_path[512];
    snprintf(sock_path, sizeof(sock_path), "/run/user/%u/kdeconnect_auth.sock", (unsigned int)uid);

    // Si el socket del daemon no existe, salir inmediatamente sin demora (fallback a contraseña)
    if (access(sock_path, F_OK) != 0) {
        return PAM_IGNORE;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return PAM_IGNORE;
    }

    // Tiempo de espera inicial para conectar con el daemon local
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return PAM_IGNORE;
    }

    const char *service = "sudo";
    const void *service_item = NULL;
    if (pam_get_item(pamh, PAM_SERVICE, &service_item) == PAM_SUCCESS && service_item) {
        service = (const char *)service_item;
    }

    char req[512];
    snprintf(req, sizeof(req), "{\"action\":\"authenticate\",\"user\":\"%s\",\"service\":\"%s\"}\n", username, service);
    if (write(fd, req, strlen(req)) <= 0) {
        close(fd);
        return PAM_IGNORE;
    }

    // Configurar tiempo de espera ampliado para la confirmación del usuario en su teléfono
    tv.tv_sec = KDECONNECT_DEFAULT_TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

    char buf[1024];
    int authenticated = 0;
    int prompt_shown = 0;

    while (1) {
        memset(buf, 0, sizeof(buf));
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            break;
        }

        if (strstr(buf, "\"status\":\"prompt\"")) {
            if (!prompt_shown) {
                pam_prompt(pamh, PAM_TEXT_INFO, NULL, "\n📱 [KDE Connect] Esperando confirmación biométrica en tu teléfono móvil...\n");
                prompt_shown = 1;
            }
            continue;
        }

        if (strstr(buf, "\"status\":\"success\"")) {
            pam_prompt(pamh, PAM_TEXT_INFO, NULL, "✓ Autenticado biométricamente mediante KDE Connect.\n");
            authenticated = 1;
            break;
        }

        if (strstr(buf, "\"status\":\"unavailable\"")) {
            close(fd);
            return PAM_IGNORE;
        }

        if (strstr(buf, "\"status\":\"rejected\"") || strstr(buf, "\"status\":\"timeout\"")) {
            pam_prompt(pamh, PAM_ERROR_MSG, NULL, "✗ Confirmación biométrica cancelada o expirada.\n");
            authenticated = 0;
            break;
        }
    }

    close(fd);
    return authenticated ? PAM_SUCCESS : PAM_AUTH_ERR;
}
