/**
 * @file ProjectTile.h
 * @brief Widget for displaying project thumbnail in StartOverlay
 */

#pragma once

#include <QWidget>
#include <QImage>
#include <QString>

class QLabel;
class QContextMenuEvent;
namespace onecad::ui {

/**
 * @brief Tile widget showing project thumbnail, name, path, and date
 */
class ProjectTile : public QWidget {
    Q_OBJECT

public:
    explicit ProjectTile(const QString& path,
                         const QImage& thumbnail = QImage(),
                         QWidget* parent = nullptr);
    ~ProjectTile() override;

    QString filePath() const { return m_path; }

signals:
    void clicked(const QString& path);
    void deleteRequested(const QString& path);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void applyTheme();

    QString m_path;
    QLabel* m_thumbnailLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_pathLabel = nullptr;
    QLabel* m_dateLabel = nullptr;
    QMetaObject::Connection m_themeConnection;
};

} // namespace onecad::ui
