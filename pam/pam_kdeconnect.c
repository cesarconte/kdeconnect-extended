/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <dirent.h>
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

static void print_msg(pam_handle_t *pamh, int is_err, const char *msg)
{
    FILE *tty = fopen("/dev/tty", "w");
    if (tty) {
        fprintf(tty, "%s", msg);
        fflush(tty);
        fclose(tty);
    } else {
        pam_prompt(pamh, is_err ? PAM_ERROR_MSG : PAM_TEXT_INFO, NULL, "%s", msg);
    }
}

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
    const void *ruser_item = NULL;
    pam_get_item(pamh, PAM_RUSER, &ruser_item);

    const char *actual_user = (ruser_item && *(const char *)ruser_item && strcmp((const char *)ruser_item, "root") != 0) ? (const char *)ruser_item : NULL;

    if (!actual_user) {
        const char *sudo_user = getenv("SUDO_USER");
        if (sudo_user && *sudo_user && strcmp(sudo_user, "root") != 0) {
            actual_user = sudo_user;
        }
    }

    if (!actual_user) {
        const char *user_env = getenv("USER");
        if (user_env && *user_env && strcmp(user_env, "root") != 0) {
            actual_user = user_env;
        }
    }

    if (!actual_user) {
        const char *login = getlogin();
        if (login && *login && strcmp(login, "root") != 0) {
            actual_user = login;
        }
    }

    if (!actual_user) {
        if (pam_get_user(pamh, &username, NULL) == PAM_SUCCESS && username && *username) {
            actual_user = username;
        }
    }

    uid_t uid = 0;
    if (actual_user) {
        struct passwd *pw = getpwnam(actual_user);
        if (pw) {
            uid = pw->pw_uid;
        }
    }

    char sock_path[512] = {0};
    if (uid >= 1000) {
        snprintf(sock_path, sizeof(sock_path), "/run/user/%u/kdeconnect_auth.sock", (unsigned int)uid);
    }

    // Si no se encuentra para el UID deducido, escanear /run/user/ en busca del socket de KDE Connect
    if (sock_path[0] == '\0' || access(sock_path, F_OK) != 0) {
        DIR *dir = opendir("/run/user");
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] >= '1' && entry->d_name[0] <= '9') {
                    char candidate[512];
                    snprintf(candidate, sizeof(candidate), "/run/user/%s/kdeconnect_auth.sock", entry->d_name);
                    if (access(candidate, F_OK) == 0) {
                        strncpy(sock_path, candidate, sizeof(sock_path) - 1);
                        uid = (uid_t)atoi(entry->d_name);
                        struct passwd *upw = getpwuid(uid);
                        if (upw) {
                            actual_user = upw->pw_name;
                        }
                        break;
                    }
                }
            }
            closedir(dir);
        }
    }

    // Si el socket del daemon no existe, salir inmediatamente sin demora (fallback a contraseña)
    if (sock_path[0] == '\0' || access(sock_path, F_OK) != 0) {
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
    snprintf(req, sizeof(req), "{\"action\":\"authenticate\",\"user\":\"%s\",\"service\":\"%s\"}\n", actual_user ? actual_user : "user", service);
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
                print_msg(pamh, 0, "\n📱 [KDE Connect] Esperando confirmación en tu teléfono móvil...\n");
                prompt_shown = 1;
            }
            continue;
        }

        if (strstr(buf, "\"status\":\"success\"")) {
            print_msg(pamh, 0, "✓ Autenticado mediante KDE Connect.\n");
            authenticated = 1;
            break;
        }

        if (strstr(buf, "\"status\":\"unavailable\"")) {
            close(fd);
            return PAM_IGNORE;
        }

        if (strstr(buf, "\"status\":\"rejected\"") || strstr(buf, "\"status\":\"timeout\"")) {
            print_msg(pamh, 1, "✗ Confirmación cancelada o expirada.\n");
            authenticated = 0;
            break;
        }
    }

    close(fd);
    return authenticated ? PAM_SUCCESS : PAM_AUTH_ERR;
}
