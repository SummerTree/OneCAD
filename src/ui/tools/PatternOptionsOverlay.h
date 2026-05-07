#ifndef ONECAD_UI_TOOLS_PATTERNOPTIONSOVERLAY_H
#define ONECAD_UI_TOOLS_PATTERNOPTIONSOVERLAY_H

#include <QFrame>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace onecad::ui::tools {

class PatternOptionsOverlay : public QFrame {
    Q_OBJECT

public:
    explicit PatternOptionsOverlay(QWidget* parent = nullptr);

    void setSourceLabel(const QString& text);
    void setDirectionLabel(const QString& text);
    void setSpacingValue(double spacing);
    void setCountValue(int count);
    void setFuseResult(bool fuse);
    void setDirectionReady(bool ready);

signals:
    void spacingChanged(double spacing);
    void countChanged(int count);
    void fuseChanged(bool fuse);

private:
    QLabel* sourceValue_ = nullptr;
    QLabel* directionValue_ = nullptr;
    QLabel* hintLabel_ = nullptr;
    QDoubleSpinBox* spacingSpin_ = nullptr;
    QSpinBox* countSpin_ = nullptr;
    QCheckBox* fuseCheck_ = nullptr;
};

} // namespace onecad::ui::tools

#endif // ONECAD_UI_TOOLS_PATTERNOPTIONSOVERLAY_H
