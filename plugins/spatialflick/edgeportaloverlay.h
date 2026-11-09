/**
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QList>
#include <QPropertyAnimation>
#include <QUrl>
#include <QWidget>

class EdgePortalOverlay : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int portalWidth READ portalWidth WRITE setPortalWidth)
    Q_PROPERTY(qreal flashOpacity READ flashOpacity WRITE setFlashOpacity)
    Q_PROPERTY(qreal waveProgress READ waveProgress WRITE setWaveProgress)

public:
    enum EdgePosition {
        EdgeRight = 0,
        EdgeLeft = 1,
        EdgeTop = 2,
        EdgeBottom = 3,
    };
    Q_ENUM(EdgePosition)

    explicit EdgePortalOverlay(QWidget *parent = nullptr);
    ~EdgePortalOverlay() override;

    void setEdgePosition(EdgePosition pos);
    EdgePosition edgePosition() const;

    void setDeviceName(const QString &name);
    QString deviceName() const;

    void setDeviceIconName(const QString &iconName);

    int portalWidth() const;
    void setPortalWidth(int width);

    qreal flashOpacity() const;
    void setFlashOpacity(qreal opacity);

    qreal waveProgress() const;
    void setWaveProgress(qreal progress);

    void showReceiveAnimation(const QString &fileName);
    void updateGeometryForScreen();

Q_SIGNALS:
    void filesDropped(const QList<QUrl> &urls);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void initLayerShell();
    void animateOpen();
    void animateClose();

    EdgePosition m_edgePosition = EdgeRight;
    QString m_deviceName = QStringLiteral("Teléfono");
    QString m_deviceIconName = QStringLiteral("smartphone");
    QString m_receivedFileName;

    int m_portalWidth = 14;
    int m_restWidth = 14;
    int m_expandedWidth = 140;
    qreal m_flashOpacity = 0.0;
    qreal m_waveProgress = 0.0;

    bool m_isDragActive = false;
    bool m_isReceiving = false;

    QPropertyAnimation *m_widthAnim = nullptr;
    QPropertyAnimation *m_flashAnim = nullptr;
    QPropertyAnimation *m_waveAnim = nullptr;
};
