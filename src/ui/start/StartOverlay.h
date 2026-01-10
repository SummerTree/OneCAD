/**
 * @file StartOverlay.h
 * @brief Startup overlay for new/open/recent projects.
 */

#pragma once

#include <QWidget>
#include <QStringList>

class QLabel;
class QGridLayout;

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

protected:
    void showEvent(QShowEvent* event) override;

private:
    void rebuildRecentGrid();
    void handleNewProject();
    void handleOpenProject();
    void handleRecentClicked(const QString& path);

    QStringList projects_;
    QWidget* recentContainer_ = nullptr;
    QGridLayout* recentGrid_ = nullptr;
    QLabel* recentEmptyLabel_ = nullptr;
    QWidget* panel_ = nullptr;
};

} // namespace onecad::ui
