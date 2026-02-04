---
name: qt-widget
description: Create Qt6 widgets following OneCAD UI conventions. Use when adding panels, dialogs, or UI components.
license: MIT
metadata:
  category: ui
  priority: medium
---

## When to Use

- Creating new UI panel/dialog
- Signal/slot connection patterns
- Theme integration

## Architecture

- MainWindow: `src/ui/mainwindow/MainWindow.h/cpp`
- Viewport: `src/ui/viewport/Viewport.h/cpp`
- Panels: `src/ui/sketch/`, `src/ui/navigator/`
- Toolbar: `src/ui/toolbar/ContextToolbar.h/cpp`

## Widget Patterns

### Parent ownership
```cpp
auto* widget = new QWidget(parent);  // Parent owns lifetime
// Or
auto* widget = new QWidget(this);    // 'this' owns it
```

### Signal/slot connections
```cpp
// Same thread (Direct)
connect(sender, &Sender::signal, receiver, &Receiver::slot);

// Cross-thread (Queued) - REQUIRED for thread safety
connect(sender, &Sender::signal, receiver, &Receiver::slot,
        Qt::QueuedConnection);
```

### Panel template
```cpp
class MyPanel : public QWidget {
    Q_OBJECT
public:
    explicit MyPanel(QWidget* parent = nullptr);

signals:
    void somethingChanged();

private slots:
    void onButtonClicked();

private:
    void setupUI();
};
```

### ThemeManager integration
```cpp
#include "ui/theme/ThemeManager.h"

// In constructor
connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
        this, &MyPanel::updateTheme);

void MyPanel::updateTheme() {
    bool dark = ThemeManager::instance().isDarkMode();
    // Update colors
}
```

## Existing Panels (reference)

- `ConstraintPanel` - Constraint list display
- `SketchModePanel` - Sketch mode controls
- `SnapSettingsPanel` - Snap configuration
- `ModelNavigator` - Model tree view
- `ViewCube` - 3D navigation widget
