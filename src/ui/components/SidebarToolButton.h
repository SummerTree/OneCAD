#ifndef ONECAD_UI_COMPONENTS_SIDEBARTOOLBUTTON_H
#define ONECAD_UI_COMPONENTS_SIDEBARTOOLBUTTON_H

#include <QToolButton>
#include <QString>

namespace onecad {
namespace ui {

class SidebarToolButton : public QToolButton {
    Q_OBJECT

public:
    explicit SidebarToolButton(const QString& symbol,
                               const QString& tooltip,
                               QWidget* parent = nullptr);
    
    // Constructor for SVG icon from file path
    static SidebarToolButton* fromSvgIcon(const QString& svgPath,
                                          const QString& tooltip,
                                          QWidget* parent = nullptr);

    void setSymbol(const QString& symbol);
    QString symbol() const { return m_symbol; }

protected:
    void changeEvent(QEvent* event) override;

private:
    void updateIcon();
    QIcon iconFromSymbol(const QString& symbol) const;
    QIcon loadSvgIcon(const QString& svgPath) const;

    QString m_symbol;
    bool m_isFromSvg = false;
};

} // namespace ui
} // namespace onecad

#endif // ONECAD_UI_COMPONENTS_SIDEBARTOOLBUTTON_H
