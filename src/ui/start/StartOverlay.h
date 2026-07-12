/**
 * @file StartOverlay.h
 * @brief Startup overlay for new/open/recent projects.
 */

#pragma once

#include <QWidget>
#include <QStringList>
#include <QMetaObject>
#include <QHash>
#include <QImage>

class QLabel;
class QGridLayout;
class QLineEdit;
class QComboBox;
class QScrollArea;
class QTimer;
class QResizeEvent;

namespace onecad::ui {

class StartOverlay : public QWidget {
    Q_OBJECT

public:
    explicit StartOverlay(QWidget* parent = nullptr);

    void setProjects(const QStringList& projects);

signals:
    void newProjectRequested();
    void openProjectRequested();
    void recentProjectRequested(const QString& path);
    void deleteProjectRequested(const QString& path);
    void importStepProjectRequested();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class SortMode { Name, DateModified };

    void applyTheme();
    void applyFilterAndSort();
    void rebuildRecentGrid();
    QStringList computeVisibleProjects() const;
    int columnCountForWidth(int width) const;
    void handleNewProject();
    void handleOpenProject();
    void handleRecentClicked(const QString& path);
    void handleDeleteClicked(const QString& path);

    QStringList projects_;
    QWidget* recentContainer_ = nullptr;
    QGridLayout* recentLayout_ = nullptr;
    QLabel* recentEmptyLabel_ = nullptr;
    QWidget* panel_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QComboBox* sortCombo_ = nullptr;
    QTimer* resizeDebounce_ = nullptr;
    SortMode sortMode_ = SortMode::DateModified;
    QString filterText_;
    int lastColumnCount_ = 0;
    QHash<QString, QImage> thumbnailCache_;
    QMetaObject::Connection themeConnection_;
};

} // namespace onecad::ui
