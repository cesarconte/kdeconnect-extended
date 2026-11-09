/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "edgeportaloverlay.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#ifdef HAVE_LAYERSHELLQT
#include <LayerShellQt/Window>
#endif

EdgePortalOverlay::EdgePortalOverlay(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAcceptDrops(true);

    m_widthAnim = new QPropertyAnimation(this, "portalWidth", this);
    m_widthAnim->setDuration(220);
    m_widthAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_flashAnim = new QPropertyAnimation(this, "flashOpacity", this);
    m_flashAnim->setDuration(450);
    m_flashAnim->setEasingCurve(QEasingCurve::OutQuad);

    m_waveAnim = new QPropertyAnimation(this, "waveProgress", this);
    m_waveAnim->setDuration(800);
    m_waveAnim->setStartValue(0.0);
    m_waveAnim->setEndValue(1.0);
    m_waveAnim->setLoopCount(-1);

    updateGeometryForScreen();
}

EdgePortalOverlay::~EdgePortalOverlay() = default;

void EdgePortalOverlay::setEdgePosition(EdgePosition pos)
{
    if (m_edgePosition != pos) {
        m_edgePosition = pos;
        updateGeometryForScreen();
        initLayerShell();
        update();
    }
}

EdgePortalOverlay::EdgePosition EdgePortalOverlay::edgePosition() const
{
    return m_edgePosition;
}

void EdgePortalOverlay::setDeviceName(const QString &name)
{
    if (m_deviceName != name) {
        m_deviceName = name;
        update();
    }
}

QString EdgePortalOverlay::deviceName() const
{
    return m_deviceName;
}

void EdgePortalOverlay::setDeviceIconName(const QString &iconName)
{
    if (m_deviceIconName != iconName) {
        m_deviceIconName = iconName;
        update();
    }
}

int EdgePortalOverlay::portalWidth() const
{
    return m_portalWidth;
}

void EdgePortalOverlay::setPortalWidth(int width)
{
    if (m_portalWidth != width) {
        m_portalWidth = width;
        updateGeometryForScreen();
        update();
    }
}

qreal EdgePortalOverlay::flashOpacity() const
{
    return m_flashOpacity;
}

void EdgePortalOverlay::setFlashOpacity(qreal opacity)
{
    if (!qFuzzyCompare(m_flashOpacity, opacity)) {
        m_flashOpacity = opacity;
        update();
    }
}

qreal EdgePortalOverlay::waveProgress() const
{
    return m_waveProgress;
}

void EdgePortalOverlay::setWaveProgress(qreal progress)
{
    if (!qFuzzyCompare(m_waveProgress, progress)) {
        m_waveProgress = progress;
        update();
    }
}

void EdgePortalOverlay::updateGeometryForScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    const QRect geo = screen->geometry();
    if (m_edgePosition == EdgeRight) {
        setGeometry(geo.right() - m_portalWidth + 1, geo.top(), m_portalWidth, geo.height());
    } else if (m_edgePosition == EdgeLeft) {
        setGeometry(geo.left(), geo.top(), m_portalWidth, geo.height());
    } else if (m_edgePosition == EdgeTop) {
        setGeometry(geo.left(), geo.top(), geo.width(), m_portalWidth);
    } else {
        setGeometry(geo.left(), geo.bottom() - m_portalWidth + 1, geo.width(), m_portalWidth);
    }
}

void EdgePortalOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    initLayerShell();
}

void EdgePortalOverlay::initLayerShell()
{
#ifdef HAVE_LAYERSHELLQT
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"), Qt::CaseInsensitive)) {
        if (windowHandle()) {
            auto *lWindow = LayerShellQt::Window::get(windowHandle());
            if (lWindow) {
                lWindow->setLayer(LayerShellQt::Window::LayerOverlay);
                if (m_edgePosition == EdgeRight) {
                    lWindow->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorRight | LayerShellQt::Window::AnchorTop
                                                                      | LayerShellQt::Window::AnchorBottom));
                } else if (m_edgePosition == EdgeLeft) {
                    lWindow->setAnchors(
                        LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom));
                } else if (m_edgePosition == EdgeTop) {
                    lWindow->setAnchors(
                        LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
                } else {
                    lWindow->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft
                                                                      | LayerShellQt::Window::AnchorRight));
                }
                lWindow->setExclusiveZone(0);
                lWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
                lWindow->setScope(QStringLiteral("kdeconnect-spatialflick"));
            }
        }
    }
#endif
}

void EdgePortalOverlay::animateOpen()
{
    m_widthAnim->stop();
    m_widthAnim->setStartValue(m_portalWidth);
    m_widthAnim->setEndValue(m_expandedWidth);
    m_widthAnim->start();

    if (m_waveAnim->state() != QAbstractAnimation::Running) {
        m_waveAnim->start();
    }
}

void EdgePortalOverlay::animateClose()
{
    m_widthAnim->stop();
    m_widthAnim->setStartValue(m_portalWidth);
    m_widthAnim->setEndValue(m_restWidth);
    m_widthAnim->start();

    m_waveAnim->stop();
}

void EdgePortalOverlay::showReceiveAnimation(const QString &fileName)
{
    m_isReceiving = true;
    m_receivedFileName = fileName;
    animateOpen();

    m_flashAnim->stop();
    m_flashAnim->setStartValue(0.95);
    m_flashAnim->setEndValue(0.0);
    m_flashAnim->start();

    // Cerrar después de 1.8 segundos
    QTimer::singleShot(1800, this, [this]() {
        m_isReceiving = false;
        animateClose();
    });
}

void EdgePortalOverlay::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        m_isDragActive = true;
        event->acceptProposedAction();
        animateOpen();
    }
}

void EdgePortalOverlay::dragMoveEvent(QDragMoveEvent *event)
{
    if (m_isDragActive) {
        event->acceptProposedAction();
    }
}

void EdgePortalOverlay::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event)
    m_isDragActive = false;
    animateClose();
}

void EdgePortalOverlay::dropEvent(QDropEvent *event)
{
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        const QList<QUrl> urls = event->mimeData()->urls();

        m_flashAnim->stop();
        m_flashAnim->setStartValue(1.0);
        m_flashAnim->setEndValue(0.0);
        m_flashAnim->start();

        Q_EMIT filesDropped(urls);

        m_isDragActive = false;
        QTimer::singleShot(250, this, [this]() {
            animateClose();
        });
    }
}

void EdgePortalOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect r = rect();

    // 1. Estado en reposo: indicador sutil de ubicación del teléfono
    if (m_portalWidth <= m_restWidth && !m_isReceiving) {
        const int dotHeight = 64;
        const int dotY = (r.height() - dotHeight) / 2;
        QRect dotRect;
        if (m_edgePosition == EdgeRight) {
            dotRect = QRect(r.right() - 4, dotY, 4, dotHeight);
        } else if (m_edgePosition == EdgeLeft) {
            dotRect = QRect(0, dotY, 4, dotHeight);
        } else if (m_edgePosition == EdgeTop) {
            dotRect = QRect((r.width() - dotHeight) / 2, 0, dotHeight, 4);
        } else {
            dotRect = QRect((r.width() - dotHeight) / 2, r.bottom() - 4, dotHeight, 4);
        }

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(56, 189, 248, 140));
        p.drawRoundedRect(dotRect, 2, 2);
        return;
    }

    // 2. Portal Expandido (Arrastre o Recepción)
    QPainterPath path;
    if (m_edgePosition == EdgeRight) {
        path.moveTo(r.topRight());
        path.lineTo(r.topLeft() + QPoint(28, 0));
        path.quadTo(r.left(), r.center().y(), r.bottomLeft().x() + 28, r.bottom());
        path.lineTo(r.bottomRight());
        path.closeSubpath();
    } else if (m_edgePosition == EdgeLeft) {
        path.moveTo(r.topLeft());
        path.lineTo(r.topRight() - QPoint(28, 0));
        path.quadTo(r.right(), r.center().y(), r.bottomRight().x() - 28, r.bottom());
        path.lineTo(r.bottomLeft());
        path.closeSubpath();
    } else {
        path.addRect(r);
    }

    // Fondo degradado
    QLinearGradient bgGrad(r.topLeft(), r.topRight());
    if (m_edgePosition == EdgeRight) {
        bgGrad.setColorAt(0.0, QColor(15, 23, 42, 220));
        bgGrad.setColorAt(0.5, QColor(30, 27, 75, 235));
        bgGrad.setColorAt(1.0, QColor(14, 165, 233, 245));
    } else {
        bgGrad.setColorAt(0.0, QColor(14, 165, 233, 245));
        bgGrad.setColorAt(0.5, QColor(30, 27, 75, 235));
        bgGrad.setColorAt(1.0, QColor(15, 23, 42, 220));
    }

    p.fillPath(path, bgGrad);

    // Borde brillante de energía cian
    p.setPen(QPen(QColor(56, 189, 248, 220), 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Ondas cinéticas del portal
    if (m_waveProgress > 0.0) {
        p.save();
        p.setClipPath(path);
        const int centerY = r.center().y();
        for (int i = 0; i < 3; ++i) {
            qreal phase = std::fmod(m_waveProgress + (i * 0.33), 1.0);
            int arcRadius = static_cast<int>(phase * (m_expandedWidth * 1.5));
            int alpha = static_cast<int>((1.0 - phase) * 160);
            p.setPen(QPen(QColor(56, 189, 248, alpha), 2.0));
            if (m_edgePosition == EdgeRight) {
                p.drawArc(r.right() - arcRadius, centerY - arcRadius, arcRadius * 2, arcRadius * 2, 90 * 16, 180 * 16);
            } else {
                p.drawArc(r.left() - arcRadius, centerY - arcRadius, arcRadius * 2, arcRadius * 2, 270 * 16, 180 * 16);
            }
        }
        p.restore();
    }

    // Flash de impacto al soltar o recibir
    if (m_flashOpacity > 0.01) {
        p.save();
        p.setClipPath(path);
        int alpha = static_cast<int>(m_flashOpacity * 255);
        p.fillRect(r, QColor(255, 255, 255, qBound(0, alpha, 255)));
        p.restore();
    }

    // Contenido informativo centrado
    const int contentY = r.center().y() - 50;
    QRect contentRect(r.left() + 10, contentY, r.width() - 20, 100);

    // Icono del dispositivo
    QIcon devIcon = QIcon::fromTheme(m_deviceIconName, QIcon::fromTheme(QStringLiteral("smartphone")));
    QPixmap pix = devIcon.pixmap(36, 36);
    if (!pix.isNull()) {
        p.drawPixmap(contentRect.center().x() - 18, contentRect.top(), pix);
    }

    // Nombre del dispositivo
    p.setPen(Qt::white);
    QFont nameFont = p.font();
    nameFont.setBold(true);
    nameFont.setPointSize(10);
    p.setFont(nameFont);
    QRect nameRect(contentRect.left(), contentRect.top() + 42, contentRect.width(), 20);
    p.drawText(nameRect, Qt::AlignCenter, p.fontMetrics().elidedText(m_deviceName, Qt::ElideRight, nameRect.width()));

    // Texto de acción
    p.setPen(m_isReceiving ? QColor(74, 222, 128) : QColor(56, 189, 248));
    QFont subFont = p.font();
    subFont.setBold(false);
    subFont.setPointSize(8);
    p.setFont(subFont);
    QRect subRect(contentRect.left(), contentRect.top() + 64, contentRect.width(), 32);

    if (m_isReceiving) {
        p.drawText(subRect, Qt::AlignCenter | Qt::TextWordWrap, QStringLiteral("📥 Recibiendo..."));
    } else {
        p.drawText(subRect, Qt::AlignCenter | Qt::TextWordWrap, QStringLiteral("🚀 Soltar para enviar"));
    }
}
