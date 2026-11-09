/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QStringList>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <vector>

static QString detectRiskLevel(const QString &command)
{
    const QString cmd = command.toLower();
    if (cmd.contains(QLatin1String("dd ")) || cmd.contains(QLatin1String("mkfs")) || cmd.contains(QLatin1String("fdisk"))
        || cmd.contains(QLatin1String("parted")) || cmd.contains(QLatin1String("wipefs")) || cmd.contains(QLatin1String("rm -rf"))
        || cmd.contains(QLatin1String("shred")) || cmd.contains(QLatin1String("format"))) {
        return QStringLiteral("critical");
    }
    if (cmd.contains(QLatin1String("pacman -r")) || cmd.contains(QLatin1String("apt remove")) || cmd.contains(QLatin1String("systemctl stop"))
        || cmd.contains(QLatin1String("reboot")) || cmd.contains(QLatin1String("poweroff")) || cmd.contains(QLatin1String("shutdown"))) {
        return QStringLiteral("high");
    }
    return QStringLiteral("medium");
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("kdeconnect-dualkey"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Dual-Key Authorization: Exige aprobación biométrica en tu teléfono antes de ejecutar un comando crítico."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption riskOption({QStringLiteral("r"), QStringLiteral("risk")},
                                  QStringLiteral("Nivel de riesgo forzado (low, medium, high, critical). Por defecto se calcula automáticamente."),
                                  QStringLiteral("nivel"));
    parser.addOption(riskOption);

    QCommandLineOption timeoutOption({QStringLiteral("t"), QStringLiteral("timeout")},
                                     QStringLiteral("Tiempo máximo de espera en segundos (por defecto: 25)."),
                                     QStringLiteral("segundos"),
                                     QStringLiteral("25"));
    parser.addOption(timeoutOption);

    QCommandLineOption dryRunOption(
        QStringLiteral("dry-run"),
        QStringLiteral("Solicita autorización pero no ejecuta el comando (código de salida 0 si fue aprobado, 1 si fue rechazado)."));
    parser.addOption(dryRunOption);

    QCommandLineOption allowOfflineOption(QStringLiteral("allow-offline"),
                                          QStringLiteral("Permite continuar si el teléfono no está accesible (no recomendado para máxima seguridad)."));
    parser.addOption(allowOfflineOption);

    parser.process(app);

    const QStringList positionalArgs = parser.positionalArguments();
    if (positionalArgs.isEmpty()) {
        std::cerr << "\033[1;31mError:\033[0m No se especificó ningún comando para ejecutar.\n"
                  << "Uso: kdeconnect-dualkey [opciones] <comando> [argumentos...]\n\n"
                  << "Ejemplos de uso:\n"
                  << "  kdeconnect-dualkey sudo dd if=arch.iso of=/dev/sdb bs=4M status=progress\n"
                  << "  kdeconnect-dualkey --risk critical sudo wipefs -a /dev/nvme0n1\n"
                  << "  kdeconnect-dualkey --dry-run \"Despliegue a producción\"\n";
        return 1;
    }

    const QString fullCommand = positionalArgs.join(QLatin1Char(' '));
    QString risk = parser.value(riskOption).toLower();
    if (risk.isEmpty()) {
        risk = detectRiskLevel(fullCommand);
    }

    const int timeoutSec = parser.value(timeoutOption).toInt();
    const bool isDryRun = parser.isSet(dryRunOption);
    const bool allowOffline = parser.isSet(allowOfflineOption);

    // Localizar socket del demonio
    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) {
        runtimeDir = QStringLiteral("/run/user/%1").arg(getuid());
    }
    QString socketPath = runtimeDir + QStringLiteral("/kdeconnect_auth.sock");
    if (!QFileInfo::exists(socketPath)) {
        socketPath = QStringLiteral("/tmp/kdeconnect_auth.sock");
    }

    QLocalSocket socket;
    socket.connectToServer(socketPath);
    if (!socket.waitForConnected(2000)) {
        if (allowOffline) {
            std::cerr << "\033[1;33m⚠️  [Dual-Key] Demonio de KDE Connect no accesible. Continuando por flag --allow-offline...\033[0m\n";
        } else {
            std::cerr << "\033[1;31m❌ [Dual-Key] Error:\033[0m No se puede conectar con el socket de autenticación de KDE Connect en:\n"
                      << "   " << socketPath.toStdString() << "\n"
                      << "   Asegúrate de que 'kdeconnectd' está activo y tu teléfono está emparejado.\n"
                      << "   (Usa --allow-offline si deseas omitir esta verificación en caso de emergencia).\n";
            return 1;
        }
    } else {
        // Enviar solicitud JSON al daemon
        QJsonObject req;
        req[QStringLiteral("action")] = QStringLiteral("dualkey");
        req[QStringLiteral("user")] = QString::fromLocal8Bit(qgetenv("USER"));
        req[QStringLiteral("service")] = QStringLiteral("dualkey-cli");
        req[QStringLiteral("command")] = fullCommand;
        req[QStringLiteral("riskLevel")] = risk;

        const QByteArray reqBytes = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
        socket.write(reqBytes);
        socket.flush();
        socket.waitForBytesWritten(1000);

        // Visualización en terminal
        std::cout << "\033[1;36m🔑 [Dual-Key Authorization]\033[0m\n"
                  << "   ──────────────────────────────────────────────────────────\n"
                  << "   \033[1mComando\033[0m : " << fullCommand.toStdString() << "\n"
                  << "   \033[1mRiesgo \033[0m : ";
        if (risk == QLatin1String("critical")) {
            std::cout << "\033[1;31m🔴 CRÍTICO\033[0m\n";
        } else if (risk == QLatin1String("high")) {
            std::cout << "\033[1;33m🟠 ALTO\033[0m\n";
        } else {
            std::cout << "\033[1;32m🟡 MEDIO\033[0m\n";
        }
        std::cout << "   \033[1mEstado \033[0m : 📱 Solicitud enviada a tu teléfono móvil. Esperando confirmación...\n"
                  << "   ──────────────────────────────────────────────────────────\n"
                  << std::flush;

        bool approved = false;
        bool done = false;
        const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + (timeoutSec * 1000);

        while (!done && QDateTime::currentMSecsSinceEpoch() < deadline) {
            const int remainingMs = qMax<int>(100, deadline - QDateTime::currentMSecsSinceEpoch());
            if (!socket.waitForReadyRead(qMin(1000, remainingMs))) {
                if (socket.state() != QLocalSocket::ConnectedState) {
                    break;
                }
                continue;
            }

            while (socket.canReadLine()) {
                const QByteArray line = socket.readLine().trimmed();
                const QJsonDocument doc = QJsonDocument::fromJson(line);
                if (!doc.isObject()) {
                    continue;
                }

                const QJsonObject resp = doc.object();
                const QString status = resp.value(QStringLiteral("status")).toString();

                if (status == QLatin1String("prompt")) {
                    continue;
                } else if (status == QLatin1String("success")) {
                    approved = true;
                    done = true;
                    break;
                } else if (status == QLatin1String("rejected")) {
                    approved = false;
                    done = true;
                    std::cerr << "\033[1;31m❌ [Dual-Key] Autorización RECHAZADA desde el dispositivo móvil.\033[0m\n"
                              << "Operación abortada por seguridad.\n";
                    return 1;
                } else if (status == QLatin1String("timeout")) {
                    approved = false;
                    done = true;
                    std::cerr << "\033[1;31m⌛ [Dual-Key] Tiempo de espera agotado sin respuesta biométrica.\033[0m\n"
                              << "Operación abortada por seguridad.\n";
                    return 124;
                } else if (status == QLatin1String("unavailable")) {
                    if (allowOffline) {
                        std::cerr << "\033[1;33m⚠️  [Dual-Key] Teléfono no accesible. Continuando por flag --allow-offline...\033[0m\n";
                        approved = true;
                        done = true;
                        break;
                    } else {
                        std::cerr << "\033[1;31m⚠️  [Dual-Key] Dispositivo móvil no accesible o desconectado.\033[0m\n"
                                  << "Operación abortada. Usa --allow-offline para forzar ejecución local.\n";
                        return 1;
                    }
                }
            }
        }

        if (!done && !approved) {
            std::cerr << "\033[1;31m⌛ [Dual-Key] Tiempo de espera límite (" << timeoutSec << "s) agotado.\033[0m\n";
            return 124;
        }

        socket.disconnectFromServer();
    }

    std::cout << "\033[1;32m✅ [Dual-Key] Autorización concedida con éxito.\033[0m\n";

    if (isDryRun) {
        std::cout << "ℹ️  Modo --dry-run: Comando no ejecutado.\n";
        return 0;
    }

    std::cout << "🚀 Ejecutando comando...\n\n" << std::flush;

    // Ejecutar comando preservando TTY, stdin/stdout y señales
    std::vector<std::string> argsStorage;
    std::vector<char *> cArgs;
    argsStorage.reserve(positionalArgs.size());
    cArgs.reserve(positionalArgs.size() + 1);

    for (const QString &arg : positionalArgs) {
        argsStorage.push_back(arg.toStdString());
    }
    for (const auto &str : argsStorage) {
        cArgs.push_back(const_cast<char *>(str.c_str()));
    }
    cArgs.push_back(nullptr);

    execvp(cArgs[0], cArgs.data());

    std::cerr << "\033[1;31mError al ejecutar '" << positionalArgs.first().toStdString() << "': \033[0m" << strerror(errno) << "\n";
    return 127;
}
