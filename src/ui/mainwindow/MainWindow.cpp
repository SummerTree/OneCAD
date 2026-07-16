#include "MainWindow.h"
#include "../theme/ThemeManager.h"
#include "../components/IconLoader.h"
#include "../viewport/Viewport.h"
#include "../viewport/RenderDebugPanel.h"
#include "../viewport/SnapSettingsPanel.h"
#include "../../render/Camera3D.h"
#include "../../core/sketch/Sketch.h"
#include "../../core/sketch/tools/SketchToolManager.h"
#include "../../app/commands/AddDatumPlaneCommand.h"
#include "../../app/commands/AddSketchCommand.h"
#include "../../app/commands/SketchDragGestureCommand.h"
#include "../../app/commands/UpdateSketchAttachmentCommand.h"
#include "../../app/commands/CommandProcessor.h"
#include "../../app/commands/DeleteBodyCommand.h"
#include "../../app/commands/DeleteSketchCommand.h"
#include "../../app/commands/RemoveOperationCommand.h"
#include "../../app/commands/RenameBodyCommand.h"
#include "../../app/commands/RenameSketchCommand.h"
#include "../../app/commands/RollbackCommand.h"
#include "../../app/commands/SetOperationSuppressionCommand.h"
#include "../../app/commands/ToggleVisibilityCommand.h"
#include "../../app/document/Document.h"
#include "../../app/AutosaveManager.h"
#include "../../app/history/RegenerationEngine.h"
#include "../navigator/ModelNavigator.h"
#include "../toolbar/ContextToolbar.h"
#include "../sketch/ConstraintPanel.h"
#include "../sketch/ConstraintApplicability.h"
#include "../sketch/SketchModePanel.h"
#include "../../core/sketch/SketchRenderer.h"
#include "../../core/sketch/SketchTypes.h"
#include "../../app/selection/SelectionTypes.h"
#include "../dialogs/CrashRecoveryDialog.h"
#include "../dialogs/MeshExportDialog.h"
#include "../../io/mesh/StlExporter.h"
#include "../../io/mesh/ObjExporter.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QSlider>
#include <QApplication>
#include <QCloseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QActionGroup>
#include <QSettings>
#include <QHBoxLayout>
#include <QWidget>
#include <QSizePolicy>
#include <QEvent>
#include <QDir>
#include <QInputDialog>
#include <QLineEdit>
#include <QFile>
#include <QTimer>
#include <QShortcut>
#include <QDebug>
#include <QLoggingCategory>
#include <algorithm>
#include <unordered_set>

#include "../../io/OneCADFileIO.h"
#include "../../io/step/StepImporter.h"
#include "../../io/step/StepExporter.h"
#include "../../app/commands/ImportStepCommand.h"
#include "../dialogs/StepExportDialog.h"
#include <QProgressDialog>

#include "../components/SidebarToolButton.h"
#include "../start/StartOverlay.h"
#include "../history/HistoryPanel.h"
#include "../history/EditParameterDialog.h"
#include "../history/RegenFailureDialog.h"
#include "../inspector/PropertyInspector.h"
#include "../palette/CommandPalette.h"
#include "../palette/ActionRegistry.h"

namespace onecad {
namespace ui {

Q_LOGGING_CATEGORY(logMainWindow, "onecad.ui.mainwindow")

namespace {
// Default constraint values
constexpr double kDefaultDistanceMm = 10.0;
constexpr double kDefaultAngleDeg = 90.0;
constexpr double kDefaultRadiusMm = 10.0;

QString formatOperationDisplayName(const app::OperationRecord& op) {
    QString typeName;
    QString params;

    switch (op.type) {
        case app::OperationType::Extrude:
            typeName = "Extrude";
            if (std::holds_alternative<app::ExtrudeParams>(op.params)) {
                const auto& p = std::get<app::ExtrudeParams>(op.params);
                params = QString(" (%1mm)").arg(p.distance, 0, 'f', 1);
            }
            break;
        case app::OperationType::Revolve:
            typeName = "Revolve";
            if (std::holds_alternative<app::RevolveParams>(op.params)) {
                const auto& p = std::get<app::RevolveParams>(op.params);
                params = QString(" (%1°)").arg(p.angleDeg, 0, 'f', 0);
            }
            break;
        case app::OperationType::Fillet:
            typeName = "Fillet";
            if (std::holds_alternative<app::FilletChamferParams>(op.params)) {
                const auto& p = std::get<app::FilletChamferParams>(op.params);
                params = QString(" (R%1)").arg(p.radius, 0, 'f', 1);
            }
            break;
        case app::OperationType::Chamfer:
            typeName = "Chamfer";
            if (std::holds_alternative<app::FilletChamferParams>(op.params)) {
                const auto& p = std::get<app::FilletChamferParams>(op.params);
                params = QString(" (%1)").arg(p.radius, 0, 'f', 1);
            }
            break;
        case app::OperationType::Shell:
            typeName = "Shell";
            if (std::holds_alternative<app::ShellParams>(op.params)) {
                const auto& p = std::get<app::ShellParams>(op.params);
                params = QString(" (%1mm)").arg(p.thickness, 0, 'f', 1);
            }
            break;
        case app::OperationType::Boolean:
            typeName = "Boolean";
            if (std::holds_alternative<app::BooleanParams>(op.params)) {
                const auto& p = std::get<app::BooleanParams>(op.params);
                switch (p.operation) {
                    case app::BooleanParams::Op::Union: params = " (Union)"; break;
                    case app::BooleanParams::Op::Cut: params = " (Cut)"; break;
                    case app::BooleanParams::Op::Intersect: params = " (Intersect)"; break;
                }
            }
            break;
        case app::OperationType::LinearPattern:
            typeName = "Linear Pattern";
            if (std::holds_alternative<app::LinearPatternParams>(op.params)) {
                const auto& p = std::get<app::LinearPatternParams>(op.params);
                params = QString(" (%1x, %2mm)").arg(p.count).arg(p.spacing, 0, 'f', 1);
            }
            break;
        case app::OperationType::CircularPattern:
            typeName = "Circular Pattern";
            if (std::holds_alternative<app::CircularPatternParams>(op.params)) {
                const auto& p = std::get<app::CircularPatternParams>(op.params);
                params = QString(" (%1x, %2°)").arg(p.count).arg(p.angleDeg, 0, 'f', 0);
            }
            break;
        case app::OperationType::Loft:
            typeName = "Loft";
            if (std::holds_alternative<app::LoftParams>(op.params)) {
                const auto& p = std::get<app::LoftParams>(op.params);
                params = QString(" (%1 sections)").arg(p.profileSketchIds.size());
            }
            break;
        case app::OperationType::Sweep:
            typeName = "Sweep";
            break;
        case app::OperationType::MirrorBody:
            typeName = "Mirror Body";
            if (std::holds_alternative<app::MirrorBodyParams>(op.params)) {
                const auto& p = std::get<app::MirrorBodyParams>(op.params);
                params = p.fuseWithOriginal ? " (Fused)" : "";
            }
            break;
    }

    return typeName + params;
}
} // anonymous namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {

    setWindowTitle(tr("OneCAD"));
    resize(1280, 800);
    setMinimumSize(800, 600);

    // Create document model (no Qt parent - unique_ptr manages lifetime)
    m_document = std::make_unique<app::Document>();
    m_commandProcessor = std::make_unique<app::commands::CommandProcessor>();

    // Busy feedback: regeneration runs synchronously on the UI thread, so show a
    // wait cursor + status for the duration of every command/undo/redo.
    connect(m_commandProcessor.get(), &app::commands::CommandProcessor::commandStarted,
            this, [this](const QString& label) {
        if (m_busyDepth++ == 0) {
            QGuiApplication::setOverrideCursor(Qt::WaitCursor);
        }
        if (m_toolStatus && !label.isEmpty()) {
            m_toolStatus->setText(tr("Working — %1…").arg(label));
        }
    });
    connect(m_commandProcessor.get(), &app::commands::CommandProcessor::commandFinished,
            this, [this]() {
        if (m_busyDepth > 0 && --m_busyDepth == 0) {
            QGuiApplication::restoreOverrideCursor();
            if (m_toolStatus) {
                m_toolStatus->setText(tr("Ready"));
            }
        }
    });

    // Autosave manager (parent = this for automatic cleanup)
    m_autosaveManager = new app::AutosaveManager(this);
    m_autosaveManager->setSaveFunction([](const QString& path, const app::Document* doc) {
        return io::OneCADFileIO::save(path, doc, QImage()).success;
    });
    m_autosaveManager->setVersionCheckFunction([](const QString& path) {
        return io::OneCADFileIO::getFileVersion(path);
    });
    m_autosaveManager->setDocument(m_document.get());
    app::AutosaveManager::cleanupOldAutosaves();

    applyTheme();
    setupMenuBar();
    setupViewport();
    setupToolBar();
    setupStatusBar();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::applyDofStatusStyle, Qt::UniqueConnection);

    connectDocumentSignals();

    // Connect navigator item actions
    connect(m_navigator, &ModelNavigator::deleteRequested,
            this, &MainWindow::onDeleteItem);
    connect(m_navigator, &ModelNavigator::renameRequested,
            this, &MainWindow::onRenameItem);
    connect(m_navigator, &ModelNavigator::renameCommitted, this,
            [this](const QString& itemId, const QString& newName) {
                std::string id = itemId.toStdString();
                bool isBody = m_document->getBodyShape(id) != nullptr;
                if (isBody) {
                    auto cmd = std::make_unique<app::commands::RenameBodyCommand>(
                        m_document.get(), id, newName.toStdString());
                    m_commandProcessor->execute(std::move(cmd));
                } else {
                    auto cmd = std::make_unique<app::commands::RenameSketchCommand>(
                        m_document.get(), id, newName.toStdString());
                    m_commandProcessor->execute(std::move(cmd));
                }
            });
    connect(m_navigator, &ModelNavigator::visibilityToggled,
            this, &MainWindow::onVisibilityToggled);
    connect(m_navigator, &ModelNavigator::isolateRequested,
            this, &MainWindow::onIsolateItem);

    // Property inspector dock. Hidden by default; revealed on selection (unless the
    // user explicitly hid it) or via the View > Inspector toggle.
    m_propertyInspector = new PropertyInspector(this);
    m_propertyInspector->setDocument(m_document.get());
    addDockWidget(Qt::RightDockWidgetArea, m_propertyInspector);
    m_propertyInspector->setVisible(false);

    // Route inspector edits through undoable commands (same pattern as the navigator).
    connect(m_propertyInspector, &PropertyInspector::bodyRenameRequested, this,
            [this](const QString& bodyId, const QString& newName) {
                auto cmd = std::make_unique<app::commands::RenameBodyCommand>(
                    m_document.get(), bodyId.toStdString(), newName.toStdString());
                m_commandProcessor->execute(std::move(cmd));
            });
    connect(m_propertyInspector, &PropertyInspector::bodyVisibilityChangeRequested, this,
            [this](const QString& bodyId, bool visible) {
                auto cmd = std::make_unique<app::commands::ToggleVisibilityCommand>(
                    m_document.get(), bodyId.toStdString(),
                    app::commands::ToggleVisibilityCommand::ItemType::Body, visible);
                m_commandProcessor->execute(std::move(cmd));
                if (m_viewport) {
                    m_viewport->update();
                }
            });
    // Keep displayed properties fresh across regeneration/rollback.
    connect(m_document.get(), &app::Document::bodyUpdated,
            m_propertyInspector, &PropertyInspector::refresh);
    connect(m_document.get(), &app::Document::operationUpdated,
            m_propertyInspector, &PropertyInspector::refresh);
    connect(m_document.get(), &app::Document::appliedOpCountChanged,
            m_propertyInspector, [this](qulonglong) { m_propertyInspector->refresh(); });

    // Command palette (Cmd+K)
    m_commandPalette = new CommandPalette(this);
    auto* paletteShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), this);
    connect(paletteShortcut, &QShortcut::activated,
            m_commandPalette, &CommandPalette::activate);

    loadSettings();

    registerMenuActions();

    QTimer::singleShot(0, this, &MainWindow::showStartDialog);
}

MainWindow::~MainWindow() {
    saveSettings();
}

void MainWindow::applyTheme() {
    ThemeManager::instance().applyTheme();
    applyDofStatusStyle();
}

void MainWindow::connectDocumentSignals() {
    for (const auto& connection : m_documentConnections) {
        disconnect(connection);
    }
    m_documentConnections.clear();

    if (!m_document || !m_navigator) {
        return;
    }

    auto trackConnection = [this](const QMetaObject::Connection& connection) {
        m_documentConnections.push_back(connection);
    };

    trackConnection(connect(m_document.get(), &app::Document::sketchAdded,
            m_navigator, &ModelNavigator::onSketchAdded));
    trackConnection(connect(m_document.get(), &app::Document::sketchRemoved,
            m_navigator, &ModelNavigator::onSketchRemoved));
    trackConnection(connect(m_document.get(), &app::Document::sketchRenamed,
            m_navigator, &ModelNavigator::onSketchRenamed));
    trackConnection(connect(m_document.get(), &app::Document::bodyAdded,
            m_navigator, &ModelNavigator::onBodyAdded));
    trackConnection(connect(m_document.get(), &app::Document::bodyRemoved,
            m_navigator, &ModelNavigator::onBodyRemoved));
    trackConnection(connect(m_document.get(), &app::Document::bodyRenamed,
            m_navigator, &ModelNavigator::onBodyRenamed));
    trackConnection(connect(m_document.get(), &app::Document::bodyVisibilityChanged,
            m_navigator, &ModelNavigator::onBodyVisibilityChanged));
    trackConnection(connect(m_document.get(), &app::Document::sketchVisibilityChanged,
            m_navigator, &ModelNavigator::onSketchVisibilityChanged));

    if (m_historyPanel) {
        trackConnection(connect(m_document.get(), &app::Document::operationAdded,
                m_historyPanel, &HistoryPanel::onOperationAdded));
        trackConnection(connect(m_document.get(), &app::Document::operationRemoved,
                m_historyPanel, &HistoryPanel::onOperationRemoved));
        trackConnection(connect(m_document.get(), &app::Document::operationUpdated,
                m_historyPanel, &HistoryPanel::onOperationAdded));
        trackConnection(connect(m_document.get(), &app::Document::operationSuppressionChanged,
                m_historyPanel, &HistoryPanel::onOperationSuppressed));
        trackConnection(connect(m_document.get(), &app::Document::operationFailed,
                m_historyPanel, &HistoryPanel::onOperationFailed));
        trackConnection(connect(m_document.get(), &app::Document::operationSucceeded,
                m_historyPanel, &HistoryPanel::onOperationSucceeded));
        trackConnection(connect(m_document.get(), &app::Document::appliedOpCountChanged,
                this, [this](qulonglong) {
                    if (m_historyPanel) {
                        m_historyPanel->rebuild();
                    }
                }));
    }
    trackConnection(connect(m_document.get(), &app::Document::documentCleared,
            this, [this]() {
                if (m_navigator) {
                    m_navigator->rebuild(m_document.get());
                }
                if (m_historyPanel) {
                    m_historyPanel->setDocument(m_document.get());
                }
                if (m_propertyInspector) {
                    m_propertyInspector->setDocument(m_document.get());
                    m_propertyInspector->showEmptyState();
                }
            }));
}

void MainWindow::updateDofStatus(core::sketch::Sketch* sketch) {
    if (!m_dofStatus) {
        return;
    }

    if (!sketch) {
        m_dofStatus->setText(tr("DOF: —"));
        m_dofStatus->setStyleSheet("");
        m_hasCachedDof = false;
        return;
    }

    int dof = sketch->getDegreesOfFreedom();
    bool overConstrained = sketch->isOverConstrained();
    m_dofStatus->setText(tr("DOF: %1").arg(dof));
    m_cachedDof = dof;
    m_cachedOverConstrained = overConstrained;
    m_hasCachedDof = true;

    applyDofStatusStyle();
}

void MainWindow::applyDofStatusStyle() {
    if (!m_dofStatus || !m_hasCachedDof) {
        return;
    }

    const ThemeStatusColors& status = ThemeManager::instance().currentTheme().status;
    QColor color = status.dofWarning;
    if (m_cachedOverConstrained) {
        color = status.dofError;
    } else if (m_cachedDof == 0) {
        color = status.dofOk;
    }

    m_dofStatus->setStyleSheet(QStringLiteral("color: %1;").arg(toQssColor(color)));
}

void MainWindow::setupMenuBar() {
    QMenuBar* menuBar = this->menuBar();
    
    // File menu
    QMenu* fileMenu = menuBar->addMenu(tr("&File"));
    registerActionIcon(fileMenu->addAction(tr("&New"), QKeySequence::New, this, &MainWindow::onNewDocument), ":/icons/paper_pencil.svg");
    registerActionIcon(fileMenu->addAction(tr("&Open..."), QKeySequence::Open, this, &MainWindow::onOpenDocument), ":/icons/ic_folder.svg");
    fileMenu->addSeparator();
    registerActionIcon(fileMenu->addAction(tr("&Save"), QKeySequence::Save, this, &MainWindow::onSaveDocument), ":/icons/ic_save.svg");
    registerActionIcon(fileMenu->addAction(tr("Save &As..."), QKeySequence::SaveAs, this, &MainWindow::onSaveDocumentAs), ":/icons/ic_save.svg");
    fileMenu->addSeparator();
    registerActionIcon(fileMenu->addAction(tr("&Import STEP..."), this, &MainWindow::onImport), ":/icons/ic_import.svg");
    fileMenu->addAction(tr("&Export STEP..."), this, &MainWindow::onExportStep);
    fileMenu->addAction(tr("Export &Mesh (STL/OBJ)..."), this, &MainWindow::onExportMesh);
    fileMenu->addSeparator();
    // Route Quit through close() so closeEvent runs the unsaved-changes guard.
    registerActionIcon(fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close), ":/icons/ic_close.svg");
    
    // Edit menu
    QMenu* editMenu = menuBar->addMenu(tr("&Edit"));
    m_undoAction = registerActionIcon(editMenu->addAction(tr("&Undo"), QKeySequence::Undo, this, [this]() {
        if (m_commandProcessor) {
            m_commandProcessor->undo();
        }
    }), ":/icons/ic_undo.svg");
    m_redoAction = registerActionIcon(editMenu->addAction(tr("&Redo"), QKeySequence::Redo, this, [this]() {
        if (m_commandProcessor) {
            m_commandProcessor->redo();
        }
    }), ":/icons/ic_redo.svg");
    if (m_undoAction) {
        m_undoAction->setEnabled(false);
    }
    if (m_redoAction) {
        m_redoAction->setEnabled(false);
    }
    if (m_commandProcessor) {
        if (m_undoAction) {
            connect(m_commandProcessor.get(), &app::commands::CommandProcessor::canUndoChanged,
                    m_undoAction, &QAction::setEnabled);
        }
        if (m_redoAction) {
            connect(m_commandProcessor.get(), &app::commands::CommandProcessor::canRedoChanged,
                    m_redoAction, &QAction::setEnabled);
        }
    }
    // NOTE: no Edit>Delete / Select All entries yet — the previous no-op stubs
    // swallowed the Delete key and polluted the command palette. Fully wired
    // QAction versions land with the action-architecture phase.

    // Insert menu
    QMenu* insertMenu = menuBar->addMenu(tr("&Insert"));
    registerActionIcon(insertMenu->addAction(tr("&Datum Plane..."), this, &MainWindow::onCreateDatumPlane),
                       ":/icons/ic_plane.svg");
    registerActionIcon(insertMenu->addAction(tr("&Update Sketch Attachment"), this,
                          &MainWindow::onUpdateSketchAttachment), ":/icons/ic_sketch.svg");

    // View menu
    QMenu* viewMenu = menuBar->addMenu(tr("&View"));
    registerActionIcon(viewMenu->addAction(tr("Zoom to &Fit"), QKeySequence(Qt::Key_0), this, [this]() {
        m_viewport->fitToView();
    }), ":/icons/ic_fit.svg");
    viewMenu->addSeparator();
    registerActionIcon(viewMenu->addAction(tr("&Front"), QKeySequence(Qt::Key_1), this, [this]() {
        m_viewport->setFrontView();
    }), ":/icons/ic_view_front.svg");
    viewMenu->addAction(tr("&Back"), QKeySequence(Qt::Key_2), this, [this]() {
        m_viewport->setBackView();
    });
    viewMenu->addAction(tr("&Left"), QKeySequence(Qt::Key_3), this, [this]() {
        m_viewport->setLeftView();
    });
    registerActionIcon(viewMenu->addAction(tr("&Right"), QKeySequence(Qt::Key_4), this, [this]() {
        m_viewport->setRightView();
    }), ":/icons/ic_view_right.svg");
    registerActionIcon(viewMenu->addAction(tr("&Top"), QKeySequence(Qt::Key_5), this, [this]() {
        m_viewport->setTopView();
    }), ":/icons/ic_view_top.svg");
    viewMenu->addAction(tr("Botto&m"), QKeySequence(Qt::Key_6), this, [this]() {
        m_viewport->setBottomView();
    });
    registerActionIcon(viewMenu->addAction(tr("&Isometric"), QKeySequence(Qt::Key_7), this, [this]() {
        m_viewport->setIsometricView();
    }), ":/icons/ic_view_iso.svg");

    viewMenu->addSeparator();

    registerActionIcon(viewMenu->addAction(tr("Toggle &Grid"), QKeySequence(Qt::Key_G), this, [this]() {
        m_viewport->toggleGrid();
    }), ":/icons/ic_grid.svg");
    viewMenu->addSeparator();

    // Theme Submenu
    QMenu* themeMenu = viewMenu->addMenu(tr("&Theme"));
    QActionGroup* themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);

    QAction* systemAction = themeMenu->addAction(tr("&System"));
    systemAction->setCheckable(true);
    themeGroup->addAction(systemAction);
    connect(systemAction, &QAction::triggered, this, [](){
        ThemeManager::instance().setThemeMode(ThemeManager::ThemeMode::System);
    });

    themeMenu->addSeparator();

    QAction* checkedAction = nullptr;
    const auto& themes = ThemeManager::instance().availableThemes();
    for (const auto& theme : themes) {
        QAction* action = themeMenu->addAction(theme.displayName);
        action->setCheckable(true);
        action->setData(theme.id);
        themeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [themeId = theme.id]() {
            ThemeManager::instance().setThemeId(themeId);
        });

        if (ThemeManager::instance().themeMode() == ThemeManager::ThemeMode::Fixed &&
            ThemeManager::instance().themeId() == theme.id) {
            checkedAction = action;
        }
    }

    if (ThemeManager::instance().themeMode() == ThemeManager::ThemeMode::System || !checkedAction) {
        systemAction->setChecked(true);
    } else {
        checkedAction->setChecked(true);
    }

    // Icon preset toggle (two-tone accent vs monochrome).
    themeMenu->addSeparator();
    QAction* twoToneAction = themeMenu->addAction(tr("&Two-tone icons"));
    twoToneAction->setCheckable(true);
    twoToneAction->setChecked(ThemeManager::instance().iconTwoTone());
    connect(twoToneAction, &QAction::triggered, this, [](bool checked) {
        ThemeManager::instance().setIconTwoTone(checked);
    });

    viewMenu->addSeparator();

    // Inspector toggle. Checkmark stays in sync with the dock's own visibility
    // (e.g. when closed via its title-bar X), and an explicit hide is remembered so
    // selection does not force the dock back open.
    m_inspectorAction = viewMenu->addAction(tr("&Inspector"));
    m_inspectorAction->setCheckable(true);
    connect(m_inspectorAction, &QAction::triggered, this, [this](bool checked) {
        m_inspectorUserHidden = !checked;
        if (m_propertyInspector) {
            m_propertyInspector->setVisible(checked);
        }
    });
    if (m_propertyInspector) {
        connect(m_propertyInspector, &QDockWidget::visibilityChanged, this,
                [this](bool visible) {
                    if (m_inspectorAction) {
                        QSignalBlocker block(m_inspectorAction);
                        m_inspectorAction->setChecked(visible);
                    }
                });
    }

    viewMenu->addSeparator();

    // Help menu
    QMenu* helpMenu = menuBar->addMenu(tr("&Help"));
    registerActionIcon(helpMenu->addAction(tr("&About OneCAD"), this, [this]() {
        QMessageBox::about(this, tr("About OneCAD"),
            tr("<h3>OneCAD</h3>"
               "<p>Version 0.1.0</p>"
               "<p>A beginner-friendly 3D CAD for makers.</p>"
               "<p>Built with Qt 6 + OpenCASCADE + Eigen3</p>"));
    }), ":/icons/ic_help.svg");

    // Sketch mode keyboard shortcuts (global, work when viewport has focus)
    QAction* lineAction = new QAction(tr("Line Tool"), this);
    lineAction->setShortcut(Qt::Key_L);
    connect(lineAction, &QAction::triggered, this, [this]() {
        if (m_viewport && m_viewport->isInSketchMode()) {
            m_viewport->activateLineTool();
        }
    });
    addAction(lineAction);
    registerActionIcon(lineAction, ":/icons/ic_line.svg");

    QAction* rectAction = new QAction(tr("Rectangle Tool"), this);
    rectAction->setShortcut(Qt::Key_R);
    connect(rectAction, &QAction::triggered, this, [this]() {
        if (m_viewport && m_viewport->isInSketchMode()) {
            m_viewport->activateRectangleTool();
        }
    });
    addAction(rectAction);
    registerActionIcon(rectAction, ":/icons/ic_rectangle.svg");

    QAction* circleAction = new QAction(tr("Circle Tool"), this);
    circleAction->setShortcut(Qt::Key_C);
    connect(circleAction, &QAction::triggered, this, [this]() {
        if (m_viewport && m_viewport->isInSketchMode()) {
            m_viewport->activateCircleTool();
        }
    });
    addAction(circleAction);
    registerActionIcon(circleAction, ":/icons/ic_circle.svg");

    QAction* arcAction = new QAction(tr("Arc Tool"), this);
    arcAction->setShortcut(Qt::Key_A);
    connect(arcAction, &QAction::triggered, this, [this]() {
        if (m_viewport && m_viewport->isInSketchMode()) {
            m_viewport->activateArcTool();
        }
    });
    addAction(arcAction);
    registerActionIcon(arcAction, ":/icons/ic_arc.svg");

    // NOTE: ellipse creation is intentionally hidden — SketchEllipse is not
    // registered with the PlaneGCS solver, so created ellipses could not be
    // constrained or dimensioned. The entity, IO, and rendering remain so
    // existing documents still load; creation returns with solver support.

    QAction* trimAction = new QAction(tr("Trim Tool"), this);
    trimAction->setShortcut(Qt::Key_T);
    connect(trimAction, &QAction::triggered, this, [this]() {
        if (m_viewport && m_viewport->isInSketchMode()) {
            m_viewport->activateTrimTool();
        }
    });
    addAction(trimAction);
    registerActionIcon(trimAction, ":/icons/ic_trim.svg");

    QAction* mirrorAction = new QAction(tr("Mirror Tool"), this);
    mirrorAction->setShortcut(Qt::Key_M);
    connect(mirrorAction, &QAction::triggered, this, [this]() {
        if (m_viewport && m_viewport->isInSketchMode()) {
            m_viewport->activateMirrorTool();
        }
    });
    addAction(mirrorAction);
    registerActionIcon(mirrorAction, ":/icons/ic_mirror.svg");

    QAction* escAction = new QAction(tr("Cancel/Exit"), this);
    escAction->setShortcut(Qt::Key_Escape);
    connect(escAction, &QAction::triggered, this, [this]() {
        if (m_viewport) {
            if (m_viewport->isInSketchMode()) {
                auto* tm = m_viewport->toolManager();
                if (tm && tm->hasActiveTool()) {
                    m_viewport->deactivateTool();
                } else {
                    onExitSketch();
                }
            } else if (m_viewport->isPlaneSelectionActive()) {
                m_viewport->cancelPlaneSelection();
            }
        }
    });
    addAction(escAction);

    // Paint two-tone icons onto the registered menu/shortcut actions and keep them
    // in sync with theme / icon-preset changes.
    updateActionIcons();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::updateActionIcons, Qt::UniqueConnection);
}

QAction* MainWindow::registerActionIcon(QAction* action, const QString& iconPath) {
    if (action && !iconPath.isEmpty()) {
        m_actionIcons.emplace_back(action, iconPath);
    }
    return action;
}

void MainWindow::updateActionIcons() {
    const QColor color = ThemeManager::instance().currentTheme().ui.menuText;
    for (const auto& [action, path] : m_actionIcons) {
        if (action) {
            action->setIcon(IconLoader::load(path, color, 18));
        }
    }
}

void MainWindow::registerMenuActions() {
    auto& reg = ActionRegistry::instance();

    // Collect all actions from menus
    for (QMenu* menu : menuBar()->findChildren<QMenu*>()) {
        const QString category = menu->title().remove(QLatin1Char('&'));
        for (QAction* action : menu->actions()) {
            if (action->isSeparator() || action->menu()) continue;
            const QString name = action->text().remove(QLatin1Char('&'));
            const QString id = QStringLiteral("%1/%2").arg(category, name);
            reg.registerAction(id, name, category, action);
        }
    }

    // Register shortcut-only actions (sketch tools etc.)
    for (QAction* action : actions()) {
        const QString name = action->text().remove(QLatin1Char('&'));
        if (name.isEmpty()) continue;
        const QString id = QStringLiteral("Action/%1").arg(name);
        reg.registerAction(id, name, tr("Tools"), action);
    }
}

void MainWindow::setupToolBar() {
    if (!m_viewport) {
        return;
    }

    m_toolbar = new ContextToolbar(m_viewport);
    m_toolbar->setContext(ContextToolbar::Context::Default);
    m_toolbar->show();
    positionToolbarOverlay();

    connect(m_toolbar, &ContextToolbar::newSketchRequested,
            this, &MainWindow::onNewSketch);
    connect(m_toolbar, &ContextToolbar::extrudeRequested, this, [this]() {
        if (!m_viewport) {
            return;
        }
        // Fast path: a single planar model face auto-creates a sketch on that face
        // (projecting its boundary) and extrudes the resulting region. Extrude no
        // longer push-pulls a raw face — it always operates on a sketch region.
        const auto modelSel = m_viewport->modelSelection();
        if (m_document && modelSel.size() == 1 &&
            modelSel.front().kind == app::selection::SelectionKind::Face) {
            const auto& face = modelSel.front();
            const std::string sketchId =
                createSketchOnFace(face.id.ownerId, face.id.elementId);
            const auto regionId = sketchId.empty()
                ? std::optional<std::string>{}
                : m_document->primaryRegionId(sketchId);
            const bool activated = regionId
                ? m_viewport->activateExtrudeToolForRegion(sketchId, *regionId)
                : false;
            if (m_toolStatus) {
                m_toolStatus->setText(activated
                    ? tr("Extrude tool active - sketch created on face")
                    : tr("Could not start extrude on the selected face"));
            }
            if (m_toolbar) {
                m_toolbar->setExtrudeActive(activated);
            }
            return;
        }

        const bool activated = m_viewport->activateExtrudeTool();
        if (m_toolStatus) {
            m_toolStatus->setText(activated
                ? tr("Extrude tool active")
                : tr("Select a sketch region to extrude"));
        }
        if (m_toolbar) {
            m_toolbar->setExtrudeActive(activated);
        }
    });
    connect(m_toolbar, &ContextToolbar::revolveRequested, this, [this]() {
        if (!m_viewport) {
            return;
        }
        const bool activated = m_viewport->activateRevolveTool();
        if (m_toolStatus) {
            m_toolStatus->setText(activated
                ? tr("Revolve tool active - Select axis")
                : tr("Select a sketch region or face to revolve"));
        }
        if (m_toolbar) {
            m_toolbar->setRevolveActive(activated);
        }
    });
    connect(m_toolbar, &ContextToolbar::filletRequested, this, [this]() {
        if (!m_viewport) {
            return;
        }
        const bool activated = m_viewport->activateFilletTool();
        if (m_toolStatus) {
            m_toolStatus->setText(activated
                ? tr("Fillet/Chamfer tool active - Drag to adjust")
                : tr("Select an edge to fillet or chamfer"));
        }
        if (m_toolbar) {
            m_toolbar->setFilletActive(activated);
        }
    });
    connect(m_toolbar, &ContextToolbar::shellRequested, this, [this]() {
        if (!m_viewport) {
            return;
        }
        const bool activated = m_viewport->activateShellTool();
        if (m_toolStatus) {
            m_toolStatus->setText(activated
                ? tr("Shell tool active - Shift+click faces, then drag")
                : tr("Select a body to shell"));
        }
        if (m_toolbar) {
            m_toolbar->setShellActive(activated);
        }
    });
    connect(m_toolbar, &ContextToolbar::exitSketchRequested,
            this, &MainWindow::onExitSketch);

    if (m_viewport) {
        auto activateTool = [this](void (Viewport::*slot)()) {
            (m_viewport->*slot)();
        };
        connect(m_toolbar, &ContextToolbar::lineToolActivated, this, [this, activateTool]() {
            activateTool(&Viewport::activateLineTool);
        });
        connect(m_toolbar, &ContextToolbar::circleToolActivated, this, [this, activateTool]() {
            activateTool(&Viewport::activateCircleTool);
        });
        connect(m_toolbar, &ContextToolbar::rectangleToolActivated, this, [this, activateTool]() {
            activateTool(&Viewport::activateRectangleTool);
        });
        connect(m_toolbar, &ContextToolbar::arcToolActivated, this, [this, activateTool]() {
            activateTool(&Viewport::activateArcTool);
        });
        // ellipseToolActivated intentionally unwired (uncontrainable entity;
        // see the ellipse note in setupActions).
        connect(m_toolbar, &ContextToolbar::trimToolActivated, this, [this, activateTool]() {
            activateTool(&Viewport::activateTrimTool);
        });
        connect(m_toolbar, &ContextToolbar::mirrorToolActivated, this, [this, activateTool]() {
            activateTool(&Viewport::activateMirrorTool);
        });

        connect(m_viewport, &Viewport::extrudeToolActiveChanged,
                m_toolbar, &ContextToolbar::setExtrudeActive);
        connect(m_viewport, &Viewport::revolveToolActiveChanged,
                m_toolbar, &ContextToolbar::setRevolveActive);
        connect(m_viewport, &Viewport::filletToolActiveChanged,
                m_toolbar, &ContextToolbar::setFilletActive);
        connect(m_viewport, &Viewport::shellToolActiveChanged,
                m_toolbar, &ContextToolbar::setShellActive);
        connect(m_viewport, &Viewport::selectionContextChanged, this, [this](int context) {
            if (!m_toolbar || !m_viewport || m_viewport->isInSketchMode()) {
                return;
            }
            using app::selection::SelectionKind;

            const auto modelSelection = m_viewport->modelSelection();
            const auto sketchSelection = m_viewport->sketchSelection();
            const bool hasSingleModelSelection = modelSelection.size() == 1 && sketchSelection.empty();
            const bool hasSingleSketchRegion = sketchSelection.size() == 1 && modelSelection.empty() &&
                sketchSelection.front().kind == SelectionKind::SketchRegion;
            const bool hasFace = hasSingleModelSelection &&
                modelSelection.front().kind == SelectionKind::Face;
            const bool hasEdge = hasSingleModelSelection &&
                modelSelection.front().kind == SelectionKind::Edge;
            const bool hasBody = hasSingleModelSelection &&
                modelSelection.front().kind == SelectionKind::Body;

            m_toolbar->setModelActionAvailability(
                hasFace || hasSingleSketchRegion,
                hasFace || hasSingleSketchRegion,
                hasEdge,
                hasBody);

            // Map int context to ContextToolbar::Context
            switch (context) {
                case 1:
                    m_toolbar->setContext(ContextToolbar::Context::Edge);
                    break;
                case 2:
                case 4:
                    m_toolbar->setContext(ContextToolbar::Context::Face);
                    break;
                case 3:
                    m_toolbar->setContext(ContextToolbar::Context::Body);
                    break;
                default:
                    m_toolbar->setContext(ContextToolbar::Context::Default);
                    break;
            }

            // Drive the selection-driven Inspector. A single body populates it; a
            // fully empty selection clears it; face/edge/region selections leave the
            // last shown content (avoids flicker during modeling).
            if (hasBody) {
                onInspectorBodySelected(
                    QString::fromStdString(modelSelection.front().id.ownerId));
            } else if (context == 0 && modelSelection.empty() && sketchSelection.empty()) {
                onInspectorSelectionCleared();
            }
        });
    }

    // Reposition toolbar when context changes (button visibility affects height)
    connect(m_toolbar, &ContextToolbar::contextChanged,
            this, &MainWindow::positionToolbarOverlay);

    // Tool activation signals - connect after m_viewport is created
    // These are connected in setupViewport() after viewport exists
}

void MainWindow::setupNavigatorOverlayButton() {
    if (!m_viewport || !m_navigator) {
        return;
    }

    m_navigatorOverlayButton = SidebarToolButton::fromSvgIcon(
        ":/icons/stack.svg", 
        tr("Toggle navigator"), 
        m_viewport
    );
    m_navigatorOverlayButton->setFixedSize(42, 42);

    connect(m_navigatorOverlayButton, &SidebarToolButton::clicked, this, [this]() {
        if (m_navigator) {
            m_navigator->setCollapsed(!m_navigator->isCollapsed());
        }
    });

    connect(m_navigator, &ModelNavigator::collapsedChanged, this, [this](bool collapsed) {
        if (m_navigatorOverlayButton) {
            positionNavigatorOverlayButton();
            m_navigatorOverlayButton->setToolTip(collapsed ? tr("Show navigator")
                                                          : tr("Hide navigator"));
        }
    });

    m_navigatorOverlayButton->setVisible(true);
    m_navigatorOverlayButton->setToolTip(m_navigator->isCollapsed()
        ? tr("Show navigator")
        : tr("Hide navigator"));
    positionNavigatorOverlayButton();
}

void MainWindow::setupHomeOverlayButton() {
    if (!m_viewport) {
        return;
    }

    m_homeOverlayButton = SidebarToolButton::fromSvgIcon(
        ":/icons/ic_home.svg",
        tr("Return to Home"),
        m_viewport
    );
    m_homeOverlayButton->setFixedSize(42, 42);
    m_homeOverlayButton->setVisible(true);

    connect(m_homeOverlayButton, &SidebarToolButton::clicked, this, [this]() {
        if (!m_startOverlay) {
            return;
        }

        if (!hasOpenProject()) {
            showStartDialog();
            return;
        }

        onSaveDocument();
        if (m_document && m_document->isModified()) {
            return;
        }

        resetDocumentState();
        showStartDialog();
    });

    positionHomeOverlayButton();
}

void MainWindow::positionHomeOverlayButton() {
    if (!m_viewport || !m_homeOverlayButton) {
        return;
    }

    const int margin = 20;
    m_homeOverlayButton->move(margin, margin);
    m_homeOverlayButton->raise();
}

void MainWindow::positionNavigatorOverlayButton() {
    if (!m_viewport || !m_navigatorOverlayButton) {
        return;
    }

    const int margin = 20;
    int y = margin;
    if (m_homeOverlayButton) {
        y = m_homeOverlayButton->y() + m_homeOverlayButton->height() + 10;
    }
    m_navigatorOverlayButton->move(margin, y);
    m_navigatorOverlayButton->raise();
}

void MainWindow::setupRenderDebugOverlay() {
    if (!m_viewport) {
        return;
    }

    m_renderDebugButton = new SidebarToolButton(QStringLiteral("D"),
                                                tr("Toggle render debug panel"),
                                                m_viewport);
    m_renderDebugButton->setFixedSize(42, 42);

    connect(m_renderDebugButton, &SidebarToolButton::clicked, this, [this]() {
        if (!m_renderDebugPanel) {
            return;
        }
        const bool visible = !m_renderDebugPanel->isVisible();
        m_renderDebugPanel->setVisible(visible);
        positionRenderDebugPanel();
    });

    m_renderDebugPanel = new RenderDebugPanel(m_viewport);
    m_renderDebugPanel->setVisible(false);

    connect(m_renderDebugPanel, &RenderDebugPanel::debugTogglesChanged, this, [this]() {
        if (!m_viewport || !m_renderDebugPanel) {
            return;
        }
        const auto toggles = m_renderDebugPanel->debugToggles();
        m_viewport->setDebugToggles(toggles.normals,
                                    toggles.depth,
                                    toggles.wireframe,
                                    toggles.disableGamma,
                                    toggles.matcap);
    });

    connect(m_renderDebugPanel, &RenderDebugPanel::lightRigChanged, this, [this]() {
        if (!m_viewport || !m_renderDebugPanel) {
            return;
        }
        const auto rig = m_renderDebugPanel->lightRig();
        m_viewport->setRenderLightRig(rig.keyDir,
                                      rig.fillDir,
                                      rig.fillIntensity,
                                      rig.ambientIntensity,
                                      rig.hemiUpDir,
                                      rig.gradientDir,
                                      rig.gradientStrength);
    });

    connect(m_renderDebugPanel, &RenderDebugPanel::resetToThemeRequested, this, [this]() {
        applyRenderDebugDefaults();
    });

    connect(m_viewport, &Viewport::debugTogglesChanged, this,
            [this](bool normals, bool depth, bool wireframe, bool disableGamma, bool matcap) {
                if (!m_renderDebugPanel) {
                    return;
                }
                RenderDebugPanel::DebugToggles toggles;
                toggles.normals = normals;
                toggles.depth = depth;
                toggles.wireframe = wireframe;
                toggles.disableGamma = disableGamma;
                toggles.matcap = matcap;
                m_renderDebugPanel->setDebugToggles(toggles);
            });

    RenderDebugPanel::DebugToggles toggles;
    toggles.normals = m_viewport->debugNormalsEnabled();
    toggles.depth = m_viewport->debugDepthEnabled();
    toggles.wireframe = m_viewport->wireframeOnlyEnabled();
    toggles.disableGamma = m_viewport->gammaDisabled();
    toggles.matcap = m_viewport->matcapEnabled();
    m_renderDebugPanel->setDebugToggles(toggles);

    applyRenderDebugDefaults();
    positionRenderDebugButton();
}

void MainWindow::positionRenderDebugButton() {
    if (!m_viewport || !m_renderDebugButton) {
        return;
    }
    const int margin = 20;
    int x = margin;
    int y = margin;
    if (m_navigatorOverlayButton) {
        y = m_navigatorOverlayButton->y() + m_navigatorOverlayButton->height() + 10;
    } else if (m_homeOverlayButton) {
        y = m_homeOverlayButton->y() + m_homeOverlayButton->height() + 10;
    }
    m_renderDebugButton->move(x, y);
    m_renderDebugButton->raise();
}

void MainWindow::positionRenderDebugPanel() {
    if (!m_viewport || !m_renderDebugPanel || !m_renderDebugPanel->isVisible()) {
        return;
    }
    const int margin = 20;
    int x = margin;
    int y = margin;
    if (m_renderDebugButton) {
        y = m_renderDebugButton->y() + m_renderDebugButton->height() + 10;
    }
    m_renderDebugPanel->move(x, y);
    m_renderDebugPanel->raise();
}

void MainWindow::applyRenderDebugDefaults() {
    if (!m_viewport || !m_renderDebugPanel) {
        return;
    }
    const auto& body = ThemeManager::instance().currentTheme().viewport.body;
    RenderDebugPanel::LightRig rig;
    rig.keyDir = body.keyLightDir;
    rig.fillDir = body.fillLightDir;
    rig.fillIntensity = body.fillLightIntensity;
    rig.ambientIntensity = body.ambientIntensity;
    rig.hemiUpDir = body.hemiUpDir;
    rig.gradientDir = body.ambientGradientDir;
    rig.gradientStrength = body.ambientGradientStrength;
    m_renderDebugPanel->setLightRig(rig);
    m_viewport->setRenderLightRig(rig.keyDir,
                                  rig.fillDir,
                                  rig.fillIntensity,
                                  rig.ambientIntensity,
                                  rig.hemiUpDir,
                                  rig.gradientDir,
                                  rig.gradientStrength);
}

void MainWindow::positionConstraintPanel() {
    if (!m_viewport || !m_constraintPanel) {
        return;
    }

    const int margin = 20;
    int x = m_viewport->width() - m_constraintPanel->width() - margin;
    int y = margin + 130;  // Below ViewCube
    m_constraintPanel->move(x, y);
    m_constraintPanel->raise();
}

void MainWindow::positionSketchModePanel() {
    if (!m_viewport || !m_sketchModePanel) {
        return;
    }

    const int margin = 20;
    // Position on right side, below constraint panel
    int x = m_viewport->width() - m_sketchModePanel->width() - margin;
    int y = margin + 130;  // Same starting position, panels stack vertically
    if (m_constraintPanel && m_constraintPanel->isVisible()) {
        y = m_constraintPanel->y() + m_constraintPanel->height() + 10;
    }
    m_sketchModePanel->move(x, y);
    m_sketchModePanel->raise();
}

void MainWindow::positionStartOverlay() {
    if (!m_startOverlay) {
        return;
    }
    QWidget* central = centralWidget();
    if (!central) {
        return;
    }
    m_startOverlay->setGeometry(central->rect());
    m_startOverlay->raise();
}

void MainWindow::showEditParameterOverlay(const QString& opId) {
    if (m_editParameterOverlay) {
        return; // Single instance; one edit at a time.
    }
    if (!m_document || !m_viewport || !centralWidget()) {
        return;
    }

    m_editParameterOverlay = new EditParameterDialog(
        m_document.get(), m_viewport, m_commandProcessor.get(),
        opId.toStdString(), centralWidget());

    connect(m_editParameterOverlay, &EditParameterDialog::finished, this, [this](bool accepted) {
        if (accepted && m_historyPanel) {
            m_historyPanel->rebuild();
        }
        closeEditParameterOverlay();
    });

    // Geometry is self-managed by ModalOverlay (tracks its parent).
    m_editParameterOverlay->show();
    m_editParameterOverlay->raise();
    m_editParameterOverlay->setFocus(Qt::OtherFocusReason);
}

void MainWindow::closeEditParameterOverlay() {
    if (!m_editParameterOverlay) {
        return;
    }
    m_editParameterOverlay->deleteLater();
    m_editParameterOverlay = nullptr;
    if (m_viewport) {
        m_viewport->update(); // Destructor clears preview meshes.
    }
}

void MainWindow::setupViewport() {
    QWidget* central = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_centralLayout = layout;

    m_navigator = new ModelNavigator(central);
    m_navigator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    m_viewport = new Viewport(central);
    m_viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(m_navigator);
    layout->addWidget(m_viewport, 1);
    setCentralWidget(central);

    m_startOverlay = new StartOverlay(central);
    m_startOverlay->hide();
    positionStartOverlay();

    connect(m_startOverlay, &StartOverlay::newProjectRequested, this, [this]() {
        onNewDocument();
        if (m_startOverlay) {
            m_startOverlay->hide();
        }
    });

    connect(m_startOverlay, &StartOverlay::openProjectRequested, this, [this]() {
        onOpenDocument();
    });

    connect(m_startOverlay, &StartOverlay::importStepProjectRequested, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Import STEP File"), defaultProjectDirectory(),
            tr("STEP Files (*.step *.stp);;All Files (*)"));
        if (file.isEmpty()) {
            return;
        }
        // Fresh document, then reuse the existing STEP import pipeline.
        onNewDocument();
        importStepFile(file);
        if (m_startOverlay) {
            m_startOverlay->hide();
        }
    });

    connect(m_startOverlay, &StartOverlay::recentProjectRequested, this, [this](const QString& path) {
        if (!maybeSave()) {
            return;
        }
        if (loadDocumentFromPath(path) && m_startOverlay) {
            m_startOverlay->hide();
        }
    });

    connect(m_startOverlay, &StartOverlay::deleteProjectRequested, this, [this](const QString& path) {
        if (path.isEmpty()) {
            return;
        }

        QFileInfo info(path);
        QString name = info.fileName();
        auto choice = QMessageBox::question(this, tr("Delete Project"),
            tr("Delete \"%1\" from disk? This cannot be undone.").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            return;
        }

        if (!deleteProjectFromPath(path)) {
            QMessageBox::warning(this, tr("Delete Failed"),
                tr("Could not delete \"%1\".").arg(name));
            return;
        }

        QString resolvedPath = info.absoluteFilePath();
        if (!m_currentFilePath.isEmpty() && QFileInfo(m_currentFilePath).absoluteFilePath() == resolvedPath) {
            m_currentFilePath.clear();
            setWindowTitle(tr("OneCAD - Untitled"));
        }

        if (m_startOverlay) {
            m_startOverlay->setProjects(listProjectsInDefaultDirectory());
        }
    });

    central->installEventFilter(this);

    // Set document for rendering sketches in 3D mode
    m_viewport->setDocument(m_document.get());
    m_viewport->setCommandProcessor(m_commandProcessor.get());

    connect(m_viewport, &Viewport::mousePositionChanged,
            this, &MainWindow::onMousePositionChanged);
    connect(m_viewport, &Viewport::sketchModeChanged,
            this, &MainWindow::onSketchModeChanged);
    connect(m_viewport, &Viewport::sketchPlanePicked,
            this, &MainWindow::onSketchPlanePicked);
    connect(m_viewport, &Viewport::planeSelectionCancelled,
            this, &MainWindow::onPlaneSelectionCancelled);
    connect(m_viewport, &Viewport::sketchUpdated,
            this, &MainWindow::onSketchUpdated);
    connect(m_viewport, &Viewport::sketchSelectionChanged,
            this, &MainWindow::onSketchSelectionChanged);
    connect(m_viewport, &Viewport::openSketchForEditRequested,
            this, &MainWindow::openSketchForEdit);
    connect(m_viewport, &Viewport::statusMessageRequested, this, [this](const QString& message) {
        statusBar()->showMessage(message, 4000);
    });
    connect(m_viewport, &Viewport::constraintDeleteRequested,
            this, &MainWindow::onConstraintPanelDeleteRequested);
    connect(m_viewport, &Viewport::constraintSuppressRequested,
            this, &MainWindow::onConstraintPanelSuppressRequested);
    connect(m_viewport, &Viewport::fileDropped, this, &MainWindow::importStepFile);

    connect(m_navigator, &ModelNavigator::sketchSelected, this, [this](const QString& id) {
        if (m_viewport) {
            m_viewport->setReferenceSketch(id);
        }
        // A sketch has no body properties; clear the inspector content.
        onInspectorSelectionCleared();
    });
    connect(m_navigator, &ModelNavigator::bodySelected, this,
            &MainWindow::onInspectorBodySelected);
    connect(m_navigator, &ModelNavigator::editSketchRequested, this, &MainWindow::openSketchForEdit);

    setupNavigatorOverlayButton();
    setupHomeOverlayButton();
    setupRenderDebugOverlay();
    setupSnapOverlay(); // Before History panel to reserve space
    setupHistoryPanel();
    setupViewControlButtons();
    
    // Initial resize to position overlays
    positionHomeOverlayButton();
    positionNavigatorOverlayButton();

    // Create constraint panel (hidden initially)
    m_constraintPanel = new ConstraintPanel(m_viewport);
    m_constraintPanel->setVisible(false);
    connect(m_constraintPanel, &ConstraintPanel::constraintSelected,
            this, &MainWindow::onConstraintPanelConstraintSelected);
    connect(m_constraintPanel, &ConstraintPanel::constraintDeleteRequested,
            this, &MainWindow::onConstraintPanelDeleteRequested);
    connect(m_constraintPanel, &ConstraintPanel::constraintSuppressRequested,
            this, &MainWindow::onConstraintPanelSuppressRequested);
    connect(m_constraintPanel, &ConstraintPanel::restoreSuppressedRequested,
            this, &MainWindow::onConstraintPanelRestoreSuppressedRequested);

    // Create sketch mode panel (hidden initially)
    m_sketchModePanel = new SketchModePanel(m_viewport);
    m_sketchModePanel->setVisible(false);
    connect(m_sketchModePanel, &SketchModePanel::constraintRequested,
            this, &MainWindow::onConstraintRequested);

    // Setup history panel
    // setupHistoryPanel(); // Moved up

    m_viewport->installEventFilter(this);
}

void MainWindow::positionToolbarOverlay() {
    if (!m_viewport || !m_toolbar) {
        return;
    }

    const int margin = 20;
    const int availableWidth = m_viewport->width();
    const int toolbarWidth = m_toolbar->width();
    int xOffset = qMax(margin, (availableWidth - toolbarWidth) / 2);
    int yOffset = margin;

    m_toolbar->move(xOffset, yOffset);
    m_toolbar->raise();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Resize) {
        if (obj == m_viewport || obj == centralWidget()) {
            if (m_startOverlay && m_startOverlay->isVisible()) {
                positionStartOverlay();
            }
        }
    }

    if (obj == m_viewport && event->type() == QEvent::Resize) {
        positionToolbarOverlay();
        positionHomeOverlayButton();
        positionNavigatorOverlayButton();
        positionRenderDebugButton();
        positionRenderDebugPanel();
        positionConstraintPanel();
        positionSketchModePanel();
        positionSnapOverlay();
        positionSnapSettingsPanel();
        positionHistoryPanel();
        positionViewControlButtons();
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setupStatusBar() {
    QStatusBar* status = statusBar();

    m_toolStatus = new QLabel(tr("Ready"));
    m_toolStatus->setMinimumWidth(150);
    status->addWidget(m_toolStatus);

    m_dofStatus = new QLabel(tr("DOF: —"));
    m_dofStatus->setMinimumWidth(80);
    status->addWidget(m_dofStatus);

    // Camera angle control (Shapr3D-style)
    QWidget* cameraAngleWidget = new QWidget(this);
    QHBoxLayout* cameraLayout = new QHBoxLayout(cameraAngleWidget);
    cameraLayout->setContentsMargins(10, 0, 10, 0);
    cameraLayout->setSpacing(8);

    QLabel* orthoLabel = new QLabel(tr("Orthographic"), cameraAngleWidget);
    orthoLabel->setStyleSheet("font-size: 10px;");

    m_cameraAngleSlider = new QSlider(Qt::Horizontal, cameraAngleWidget);
    m_cameraAngleSlider->setRange(0, 90);
    m_cameraAngleSlider->setValue(45);
    m_cameraAngleSlider->setFixedWidth(150);
    m_cameraAngleSlider->setTickPosition(QSlider::TicksBelow);
    m_cameraAngleSlider->setTickInterval(15);

    QLabel* perspLabel = new QLabel(tr("Perspective"), cameraAngleWidget);
    perspLabel->setStyleSheet("font-size: 10px;");

    m_cameraAngleLabel = new QLabel(tr("45°"), cameraAngleWidget);
    m_cameraAngleLabel->setMinimumWidth(35);
    m_cameraAngleLabel->setAlignment(Qt::AlignCenter);

    cameraLayout->addWidget(orthoLabel);
    cameraLayout->addWidget(m_cameraAngleSlider);
    cameraLayout->addWidget(perspLabel);
    cameraLayout->addWidget(m_cameraAngleLabel);

    status->addPermanentWidget(cameraAngleWidget);

    // Wire slider to camera
    connect(m_cameraAngleSlider, &QSlider::valueChanged, this, [this](int value) {
        m_cameraAngleLabel->setText(tr("%1°").arg(value));
        if (m_viewport) {
            m_viewport->setCameraAngle(static_cast<float>(value));
        }
    });

    m_coordStatus = new QLabel(tr("X: 0.00  Y: 0.00  Z: 0.00"));
    m_coordStatus->setMinimumWidth(200);
    status->addPermanentWidget(m_coordStatus);
}

void MainWindow::onNewSketch() {
    qCInfo(logMainWindow) << "onNewSketch:start";
    // If already in sketch mode, exit first (then continue create flow).
    if (m_viewport->isInSketchMode()) {
        qCDebug(logMainWindow) << "onNewSketch:exiting-existing-sketch-mode";
        onExitSketch();
    }

    m_activeSketchId.clear();

    const auto selection = m_viewport->modelSelection();
    qCDebug(logMainWindow) << "onNewSketch:modelSelectionCount=" << selection.size();
    if (selection.size() == 1 && selection[0].kind == app::selection::SelectionKind::Face) {
        const auto& selectedFace = selection[0];
        const std::string sketchId =
            createSketchOnFace(selectedFace.id.ownerId, selectedFace.id.elementId);
        if (!sketchId.empty()) {
            m_activeSketchId = sketchId;
            core::sketch::Sketch* sketchPtr = m_document->getSketch(m_activeSketchId);
            if (sketchPtr) {
                m_viewport->enterSketchMode(sketchPtr);
                m_toolStatus->setText(tr("Sketch Mode - Selected Face"));
                m_toolbar->setContext(ContextToolbar::Context::Sketch);
                qCInfo(logMainWindow) << "onNewSketch:entered-face-sketch-mode"
                                      << "sketchId=" << QString::fromStdString(m_activeSketchId);
                return;
            }
            m_activeSketchId.clear();
            m_toolStatus->setText(tr("Ready"));
            return;
        }
        qCWarning(logMainWindow) << "onNewSketch:failed-to-create-sketch-on-face"
                                 << "bodyId=" << QString::fromStdString(selectedFace.id.ownerId)
                                 << "faceId=" << QString::fromStdString(selectedFace.id.elementId);
    }

    m_viewport->beginPlaneSelection();
    m_toolStatus->setText(tr("Select a plane to start sketch"));
    qCInfo(logMainWindow) << "onNewSketch:plane-selection-started";
}

std::string MainWindow::createSketchOnFace(const std::string& bodyId,
                                           const std::string& faceId) {
    if (!m_document || bodyId.empty() || faceId.empty()) {
        return {};
    }
    qCDebug(logMainWindow) << "createSketchOnFace"
                           << "bodyId=" << QString::fromStdString(bodyId)
                           << "faceId=" << QString::fromStdString(faceId);

    auto plane = m_document->getSketchPlaneForFace(bodyId, faceId);
    if (!plane) {
        qCWarning(logMainWindow) << "createSketchOnFace:failed-to-resolve-face-plane";
        return {};
    }

    auto sketch = std::make_unique<core::sketch::Sketch>(*plane);
    sketch->setHostFaceAttachment(bodyId, faceId);
    const std::string sketchId = m_document->addSketch(std::move(sketch));
    if (sketchId.empty()) {
        qCWarning(logMainWindow) << "createSketchOnFace:add-sketch-failed";
        return {};
    }

    const bool projected = m_document->ensureHostFaceBoundariesProjected(sketchId);
    qCDebug(logMainWindow) << "createSketchOnFace:host-boundary-projection"
                           << "sketchId=" << QString::fromStdString(sketchId)
                           << "projected=" << projected;
    m_viewport->setReferenceSketch(QString::fromStdString(sketchId));
    return sketchId;
}

void MainWindow::onSketchPlanePicked(int planeIndex) {
    core::sketch::SketchPlane plane;
    QString planeName;
    switch (planeIndex) {
        case 0:
            plane = core::sketch::SketchPlane::XY();
            planeName = "XY";
            break;
        case 1:
            plane = core::sketch::SketchPlane::XZ();
            planeName = "XZ";
            break;
        case 2:
        default:
            plane = core::sketch::SketchPlane::YZ();
            planeName = "YZ";
            break;
    }

    auto sketch = std::make_unique<core::sketch::Sketch>(plane);
    m_activeSketchId = m_document->addSketch(std::move(sketch));
    if (!m_activeSketchId.empty()) {
        m_viewport->setReferenceSketch(QString::fromStdString(m_activeSketchId));
        qCDebug(logMainWindow) << "onSketchPlanePicked:reference-sketch-set"
                               << "planeIndex=" << planeIndex
                               << "sketchId=" << QString::fromStdString(m_activeSketchId);
    }

    core::sketch::Sketch* sketchPtr = m_document->getSketch(m_activeSketchId);
    if (!sketchPtr) {
        qCWarning(logMainWindow) << "onSketchPlanePicked:sketch-not-found-after-create"
                                 << "planeIndex=" << planeIndex;
        m_activeSketchId.clear();
        m_toolStatus->setText(tr("Ready"));
        return;
    }

    m_viewport->enterSketchMode(sketchPtr);
    m_toolStatus->setText(tr("Sketch Mode - %1 Plane").arg(planeName));
    m_toolbar->setContext(ContextToolbar::Context::Sketch);
    qCInfo(logMainWindow) << "onSketchPlanePicked:entered-sketch-mode"
                          << "plane=" << planeName
                          << "sketchId=" << QString::fromStdString(m_activeSketchId);
}

void MainWindow::onCreateDatumPlane() {
    if (!m_document || !m_viewport) {
        return;
    }
    if (m_viewport->isInSketchMode()) {
        onExitSketch();
    }

    bool ok = false;
    const double offset = QInputDialog::getDouble(
        this, tr("Create Datum Plane"), tr("Offset from XY plane (mm):"),
        10.0, -1.0e6, 1.0e6, 3, &ok);
    if (!ok) {
        return;
    }

    app::DatumPlane datum;
    datum.name = tr("Datum (XY +%1)").arg(offset).toStdString();
    datum.kind = app::DatumPlane::Kind::OffsetFromPlane;
    datum.basePlaneId = "XY";
    datum.offset = offset;

    if (!m_commandProcessor) {
        return;
    }

    // Datum + its follow-up sketch are one user action: a single transaction so
    // one Undo removes both (previously the sketch bypassed the undo stack).
    m_commandProcessor->beginTransaction(tr("Create Datum Plane").toStdString());

    auto command = std::make_unique<app::commands::AddDatumPlaneCommand>(m_document.get(), datum);
    auto* rawDatum = command.get();
    if (!m_commandProcessor->execute(std::move(command))) {
        m_commandProcessor->cancelTransaction();
        m_toolStatus->setText(tr("Failed to create datum plane"));
        return;
    }

    // Start a sketch on the new datum (frozen frame).
    const app::DatumPlane* created = m_document->getDatumPlane(rawDatum->datumId());
    if (!created || !created->resolvedValid) {
        m_commandProcessor->endTransaction();
        m_toolStatus->setText(tr("Datum plane created"));
        return;
    }
    auto sketchCommand = std::make_unique<app::commands::AddSketchCommand>(
        m_document.get(), created->resolvedPlane);
    const std::string newSketchId = sketchCommand->sketchId();
    if (!m_commandProcessor->execute(std::move(sketchCommand))) {
        m_commandProcessor->endTransaction();  // keep the datum; sketch add failed
        m_toolStatus->setText(tr("Datum plane created"));
        return;
    }
    m_commandProcessor->endTransaction();

    m_activeSketchId = newSketchId;
    core::sketch::Sketch* sketchPtr = m_document->getSketch(m_activeSketchId);
    if (!sketchPtr) {
        m_activeSketchId.clear();
        m_toolStatus->setText(tr("Datum plane created"));
        return;
    }
    m_viewport->setReferenceSketch(QString::fromStdString(m_activeSketchId));
    m_viewport->enterSketchMode(sketchPtr);
    m_toolbar->setContext(ContextToolbar::Context::Sketch);
    m_toolStatus->setText(tr("Sketch Mode - Datum Plane"));
}

void MainWindow::onUpdateSketchAttachment() {
    if (!m_document || m_activeSketchId.empty()) {
        if (m_toolStatus) {
            m_toolStatus->setText(tr("No active sketch to update"));
        }
        return;
    }
    core::sketch::Sketch* sketch = m_document->getSketch(m_activeSketchId);
    if (!sketch || !sketch->hasHostFaceAttachment()) {
        if (m_toolStatus) {
            m_toolStatus->setText(tr("Active sketch is not attached to a face"));
        }
        return;
    }
    auto cmd = std::make_unique<app::commands::UpdateSketchAttachmentCommand>(
        m_document.get(), m_activeSketchId);
    const bool ok = m_commandProcessor
        ? m_commandProcessor->execute(std::move(cmd))
        : cmd->execute();
    if (m_toolStatus) {
        m_toolStatus->setText(ok ? tr("Sketch attachment updated")
                                 : tr("Failed to update sketch attachment"));
    }
}

void MainWindow::onPlaneSelectionCancelled() {
    if (!m_viewport->isInSketchMode()) {
        m_toolStatus->setText(tr("Ready"));
    }
}

void MainWindow::openSketchForEdit(const QString& sketchId) {
    if (sketchId.isEmpty() || !m_viewport || !m_document) {
        return;
    }
    const std::string id = sketchId.toStdString();
    m_viewport->setReferenceSketch(sketchId);
    m_activeSketchId = id;
    core::sketch::Sketch* sketchPtr = m_document->getSketch(m_activeSketchId);
    if (!sketchPtr) {
        m_activeSketchId.clear();
        return;
    }
    m_viewport->enterSketchMode(sketchPtr);
    QString sketchName = QString::fromStdString(m_document->getSketchName(id));
    m_toolStatus->setText(tr("Sketch Mode - %1").arg(sketchName));
    m_toolbar->setContext(ContextToolbar::Context::Sketch);
}

void MainWindow::onExitSketch() {
    if (!m_viewport->isInSketchMode()) return;

    // Exit sketch mode but keep sketch in document
    m_viewport->exitSketchMode();

    // Clear active sketch ID (we're no longer editing)
    m_activeSketchId.clear();

    m_toolStatus->setText(tr("Ready"));
    m_toolbar->setContext(ContextToolbar::Context::Default);

    // Trigger viewport update to show sketch in 3D view
    m_viewport->update();
}

void MainWindow::onSketchModeChanged(bool inSketchMode) {
    core::sketch::Sketch* activeSketch = nullptr;
    if (!m_activeSketchId.empty()) {
        activeSketch = m_document->getSketch(m_activeSketchId);
    }

    if (inSketchMode && activeSketch) {
        if (m_viewport && m_snapSettingsPanel) {
            m_viewport->updateSnapSettings(m_snapSettingsPanel->settings());
        }

        updateDofStatus(activeSketch);

        if (m_constraintPanel) {
            m_constraintPanel->setSketch(activeSketch);
            m_constraintPanel->setVisible(true);
            positionConstraintPanel();
        }

        if (m_sketchModePanel) {
            m_sketchModePanel->setSketch(activeSketch);
        }
        updateSketchConstraintUi();
    } else {
        updateDofStatus(nullptr);

        if (m_constraintPanel) {
            m_constraintPanel->setVisible(false);
        }

        if (m_sketchModePanel) {
            m_sketchModePanel->setVisible(false);
        }
    }
}

void MainWindow::onImport() {
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Import STEP File"), QString(),
        tr("STEP Files (*.step *.stp);;All Files (*)"));

    if (fileName.isEmpty()) {
        return;
    }

    importStepFile(fileName);
}

void MainWindow::importStepFile(const QString& path) {
    m_toolStatus->setText(tr("Importing: %1").arg(QFileInfo(path).fileName()));

    QProgressDialog progress(tr("Importing STEP..."), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);
    progress.show();
    QApplication::processEvents();

    auto result = io::StepImporter::import(path);
    progress.close();

    if (!result.success) {
        QMessageBox::critical(this, tr("Import Failed"), result.errorMessage);
        m_toolStatus->setText(tr("Import failed"));
        return;
    }

    if (!m_commandProcessor) return;

    const int importedCount = static_cast<int>(result.bodies.size());
    auto cmd = std::make_unique<app::commands::ImportStepCommand>(
        m_document.get(), std::move(result.bodies));
    m_commandProcessor->execute(std::move(cmd));

    if (m_viewport) {
        m_viewport->update();
    }
    m_toolStatus->setText(tr("Imported %1 body(ies)").arg(importedCount));
}

void MainWindow::onExportStep() {
    auto bodyIds = m_document->getBodyIds();
    if (bodyIds.empty()) {
        QMessageBox::warning(this, tr("Export"), tr("No bodies to export."));
        return;
    }

    StepExportDialog exportDialog(this);
    if (exportDialog.exec() != QDialog::Accepted) {
        return;
    }
    auto opts = exportDialog.options();

    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Export STEP File"), QString(),
        tr("STEP Files (*.step)"));

    if (fileName.isEmpty()) {
        return;
    }

    if (!fileName.endsWith(".step", Qt::CaseInsensitive)) {
        fileName += ".step";
    }

    QProgressDialog progress(tr("Exporting STEP..."), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);
    progress.show();
    QApplication::processEvents();

    io::ExportOptions exportOpts;
    exportOpts.schema = opts.schema;
    exportOpts.visibleOnly = opts.visibleOnly;

    auto result = io::StepExporter::exportDocument(fileName, m_document.get(), exportOpts);
    progress.close();

    if (!result.success) {
        QMessageBox::critical(this, tr("Export Failed"), result.errorMessage);
        return;
    }

    m_toolStatus->setText(tr("Exported %1 body(ies) to STEP").arg(result.bodyCount));
}

void MainWindow::onExportMesh() {
    auto bodyIds = m_document->getBodyIds();
    if (bodyIds.empty()) {
        QMessageBox::warning(this, tr("Export"), tr("No bodies to export."));
        return;
    }

    MeshExportDialog exportDialog(this);
    if (exportDialog.exec() != QDialog::Accepted) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Export Mesh"), QString(), exportDialog.fileFilter());
    if (fileName.isEmpty()) {
        return;
    }

    QString ext = exportDialog.defaultExtension();
    if (!fileName.endsWith(ext, Qt::CaseInsensitive)) {
        fileName += ext;
    }

    auto format = exportDialog.selectedFormat();
    bool visibleOnly = exportDialog.visibleOnly();

    io::MeshExportResult result;
    if (format == MeshExportDialog::OBJ) {
        io::ObjExportOptions opts;
        opts.visibleOnly = visibleOnly;
        result = io::ObjExporter::exportDocument(fileName, m_document.get(), opts);
    } else {
        io::StlExportOptions opts;
        opts.binary = (format == MeshExportDialog::STL_Binary);
        opts.visibleOnly = visibleOnly;
        result = io::StlExporter::exportDocument(fileName, m_document.get(), opts);
    }

    if (!result.success) {
        QMessageBox::critical(this, tr("Export Failed"), result.errorMessage);
        return;
    }

    m_toolStatus->setText(
        tr("Exported %1 body(ies), %2 triangles")
            .arg(result.bodyCount)
            .arg(result.triangleCount));
}

bool MainWindow::maybeSave() {
    if (!m_document || !m_document->isModified()) {
        return true;
    }
    
    auto result = QMessageBox::warning(this, tr("Unsaved Changes"),
        tr("The document has been modified.\nDo you want to save your changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    
    if (result == QMessageBox::Save) {
        onSaveDocument();
        return !m_document->isModified();  // Returns true if save succeeded
    }
    
    return result == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::onNewDocument() {
    if (!maybeSave()) return;

    resetDocumentState();
    setWindowTitle(tr("OneCAD - Untitled"));
    m_toolStatus->setText(tr("New document"));
    if (m_startOverlay && m_startOverlay->isVisible()) {
        m_startOverlay->hide();
    }
    statusBar()->show();
}

QString MainWindow::defaultProjectDirectory() const {
    QString dir = QDir::homePath() + "/OneCAD/Projects";
    if (!QDir().mkpath(dir)) {
        qWarning() << "Failed to create project directory:" << dir;
        return QDir::homePath();
    }
    return dir;
}

bool MainWindow::loadDocumentFromPath(const QString& fileName) {
    if (fileName.isEmpty()) {
        return false;
    }

    // The edit overlay holds a raw Document*; tear it down before the document
    // is replaced to avoid a dangling pointer.
    closeEditParameterOverlay();

    QString resolvedPath = QFileInfo(fileName).absoluteFilePath();

    // Exit sketch mode if active
    if (m_viewport && m_viewport->isInSketchMode()) {
        onExitSketch();
    }
    if (m_viewport) {
        m_viewport->resetTransientState();
    }

    m_toolStatus->setText(tr("Loading: %1").arg(QFileInfo(resolvedPath).fileName()));

    QString errorMessage;
    auto loadedDoc = io::OneCADFileIO::load(resolvedPath, errorMessage);
    if (!loadedDoc) {
        QMessageBox::critical(this, tr("Load Failed"), errorMessage);
        m_toolStatus->setText(tr("Load failed"));
        return false;
    }

    // Replace current document with loaded one. Detach listeners holding raw
    // pointers BEFORE the swap destroys the old Document (QPointer in
    // AutosaveManager makes this belt-and-braces).
    if (m_autosaveManager) {
        m_autosaveManager->setDocument(nullptr);
    }
    m_document = std::move(loadedDoc);
    if (m_commandProcessor) {
        m_commandProcessor->clear();
    }
    m_activeSketchId.clear();
    m_currentFilePath = resolvedPath;
    m_viewport->setDocument(m_document.get());

    // Reconnect document signals to navigator
    connectDocumentSignals();

    if (m_autosaveManager) {
        m_autosaveManager->setDocument(m_document.get());
        m_autosaveManager->setCurrentFilePath(m_currentFilePath);
    }

    if (m_historyPanel) {
        m_historyPanel->setDocument(m_document.get());
    }

    if (m_propertyInspector) {
        m_propertyInspector->setDocument(m_document.get());
    }

    // Populate navigator with loaded data
    m_navigator->rebuild(m_document.get());

    setWindowTitle(tr("OneCAD - %1").arg(QFileInfo(resolvedPath).fileName()));
    m_toolStatus->setText(tr("Loaded successfully"));

    if (m_viewport) {
        m_viewport->update();
    }

    handleRegenerationFailures();

    if (m_startOverlay && m_startOverlay->isVisible()) {
        m_startOverlay->hide();
    }
    statusBar()->show();
    return true;
}

void MainWindow::handleRegenerationFailures() {
    if (!m_document) {
        return;
    }

    const auto& failures = m_document->operationFailures();
    if (failures.empty()) {
        return;
    }

    std::vector<RegenFailureDialog::FailedOp> failedOps;
    failedOps.reserve(failures.size());
    for (const auto& [opId, reason] : failures) {
        const app::OperationRecord* op = m_document->findOperation(opId);
        QString description = op ? formatOperationDisplayName(*op)
                                 : QString::fromStdString(opId);
        failedOps.push_back({
            QString::fromStdString(opId),
            description,
            QString::fromStdString(reason)
        });
    }

    RegenFailureDialog dialog(failedOps, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    auto action = dialog.selectedAction();
    std::vector<std::string> failedIds;
    failedIds.reserve(failures.size());
    for (const auto& [opId, reason] : failures) {
        failedIds.push_back(opId);
    }

    bool changed = false;
    if (action == RegenFailureDialog::Result::DeleteFailed) {
        for (const auto& opId : failedIds) {
            changed |= m_document->removeOperation(opId);
        }
    } else if (action == RegenFailureDialog::Result::SuppressFailed) {
        for (const auto& opId : failedIds) {
            changed |= m_document->setOperationSuppressed(opId, true);
        }
    } else {
        return;
    }

    if (changed) {
        app::history::RegenerationEngine regen(m_document.get());
        regen.regenerateToAppliedCount(m_document->appliedOpCount());
        m_document->setModified(true);
        if (m_historyPanel) {
            m_historyPanel->rebuild();
        }
        if (m_viewport) {
            m_viewport->update();
        }
    }
}

void MainWindow::onOpenDocument() {
    if (!maybeSave()) return;
    
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open OneCAD Project"), defaultProjectDirectory(),
        tr("OneCAD Files (*.onecad);;OneCAD Packages (*.onecadpkg);;All Files (*)"));
    
    if (fileName.isEmpty()) return;

    loadDocumentFromPath(fileName);
}

bool MainWindow::saveDocumentToPath(const QString& filePath) {
    if (!m_document) {
        return false;
    }

    if (m_toolStatus) {
        m_toolStatus->setText(tr("Saving..."));
    }

    // Capture viewport thumbnail
    QImage thumbnail;
    if (m_viewport) {
        thumbnail = m_viewport->captureThumbnail(512);
    }

    auto result = io::OneCADFileIO::save(filePath, m_document.get(), thumbnail);
    if (!result.success) {
        QMessageBox::critical(this, tr("Save Failed"), result.errorMessage);
        if (m_toolStatus) {
            m_toolStatus->setText(tr("Save failed"));
        }
        return false;
    }

    m_document->setModified(false);
    if (m_autosaveManager) {
        m_autosaveManager->setCurrentFilePath(filePath);
    }
    if (m_toolStatus) {
        m_toolStatus->setText(tr("Saved"));
    }
    return true;
}

bool MainWindow::hasOpenProject() const {
    if (!m_document) {
        return false;
    }

    if (!m_currentFilePath.isEmpty()) {
        return true;
    }

    if (m_document->isModified()) {
        return true;
    }

    return m_document->sketchCount() > 0 || m_document->bodyCount() > 0;
}

void MainWindow::resetDocumentState() {
    // The edit overlay holds a raw Document*; tear it down before the document
    // is cleared to avoid a dangling pointer.
    closeEditParameterOverlay();

    if (m_viewport && m_viewport->isInSketchMode()) {
        onExitSketch();
    }
    if (m_viewport) {
        m_viewport->resetTransientState();
    }

    if (m_commandProcessor) {
        m_commandProcessor->clear();
    }

    if (m_document) {
        m_document->clear();
    }

    m_activeSketchId.clear();
    m_currentFilePath.clear();
    if (m_autosaveManager) {
        m_autosaveManager->setDocument(m_document.get());
        m_autosaveManager->setCurrentFilePath(QString());
    }
    setWindowTitle(tr("OneCAD - Untitled"));
    if (m_toolStatus) {
        m_toolStatus->setText(tr("Ready"));
    }

    if (m_propertyInspector) {
        m_propertyInspector->setDocument(m_document.get());
        m_propertyInspector->showEmptyState();
    }

    if (m_historyPanel) {
        m_historyPanel->setDocument(m_document.get());
    }

    if (m_navigator) {
        m_navigator->rebuild(m_document.get());
    }

    if (m_viewport) {
        m_viewport->update();
    }
}

void MainWindow::onSaveDocument() {
    QString targetPath = m_currentFilePath;
    if (targetPath.isEmpty()) {
        bool ok = false;
        QString name = QInputDialog::getText(
            this,
            tr("Save Project"),
            tr("Project name:"),
            QLineEdit::Normal,
            tr("Untitled"),
            &ok);

        if (!ok) {
            return;
        }

        name = name.trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, tr("Save Project"), tr("Project name cannot be empty."));
            return;
        }

        name.replace("/", "-");
        name.replace("\\", "-");

        targetPath = defaultProjectDirectory() + "/" + name;
        if (!targetPath.endsWith(".onecad", Qt::CaseInsensitive)) {
            targetPath += ".onecad";
        }

        QFileInfo fileInfo(targetPath);
        if (fileInfo.exists()) {
            auto choice = QMessageBox::question(this, tr("Overwrite Project"),
                tr("A project with this name already exists. Overwrite it?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (choice != QMessageBox::Yes) {
                return;
            }
        }
    }
    
    if (!saveDocumentToPath(targetPath)) {
        return;
    }

    m_currentFilePath = targetPath;
    setWindowTitle(tr("OneCAD - %1").arg(QFileInfo(targetPath).fileName()));
}

void MainWindow::onSaveDocumentAs() {
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save OneCAD Project As"), defaultProjectDirectory() + "/Untitled.onecad",
        tr("OneCAD Files (*.onecad)"));
    
    if (fileName.isEmpty()) return;
    
    // Ensure .onecad extension
    if (!fileName.endsWith(".onecad", Qt::CaseInsensitive)) {
        fileName += ".onecad";
    }
    
    if (!saveDocumentToPath(fileName)) {
        return;
    }

    m_currentFilePath = fileName;
    setWindowTitle(tr("OneCAD - %1").arg(QFileInfo(fileName).fileName()));
}

void MainWindow::showStartDialog() {
    // Check for crash recovery files
    auto recoveryFiles = m_autosaveManager->scanRecoveryFiles();
    if (!recoveryFiles.empty()) {
        CrashRecoveryDialog dialog(recoveryFiles, this);
        if (dialog.exec() == QDialog::Accepted) {
            if (dialog.selectedAction() == CrashRecoveryDialog::Recover) {
                QString path = dialog.selectedFilePath();
                if (!path.isEmpty() && loadDocumentFromPath(path)) {
                    m_currentFilePath.clear(); // Treat as untitled
                    if (m_autosaveManager) {
                        m_autosaveManager->setCurrentFilePath(QString());
                    }
                    setWindowTitle(tr("OneCAD - Recovered"));
                    return;
                }
            } else if (dialog.selectedAction() == CrashRecoveryDialog::Discard) {
                for (const auto& info : recoveryFiles) {
                    app::AutosaveManager::removeRecoveryFile(info.autosavePath);
                }
            }
        }
    }

    if (!m_startOverlay) {
        return;
    }
    m_startOverlay->setProjects(listProjectsInDefaultDirectory());
    statusBar()->hide();
    m_startOverlay->show();
    positionStartOverlay();
    m_startOverlay->raise();
    m_startOverlay->setFocus(Qt::OtherFocusReason);
}

bool MainWindow::deleteProjectFromPath(const QString& filePath) {
    QFileInfo info(filePath);
    if (!info.exists()) {
        return true;
    }

    if (info.isDir()) {
        QDir dir(info.absoluteFilePath());
        return dir.removeRecursively();
    }

    return QFile::remove(info.absoluteFilePath());
}

QStringList MainWindow::listProjectsInDefaultDirectory() const {
    QDir dir(defaultProjectDirectory());
    QList<QFileInfo> entries;

    QFileInfoList files = dir.entryInfoList(QStringList() << "*.onecad",
                                            QDir::Files | QDir::NoDotAndDotDot);
    QFileInfoList packages = dir.entryInfoList(QStringList() << "*.onecadpkg",
                                               QDir::Dirs | QDir::NoDotAndDotDot);

    entries.append(files);
    entries.append(packages);

    std::sort(entries.begin(), entries.end(),
              [](const QFileInfo& a, const QFileInfo& b) {
                  return a.lastModified() > b.lastModified();
              });

    QStringList paths;
    for (const QFileInfo& info : entries) {
        paths.append(info.absoluteFilePath());
    }

    return paths;
}

void MainWindow::onMousePositionChanged(double x, double y, double z) {
    m_coordStatus->setText(tr("X: %1  Y: %2  Z: %3")
        .arg(x, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(z, 0, 'f', 2));
}

void MainWindow::onSketchUpdated() {
    if (!m_viewport->isInSketchMode()) return;

    core::sketch::Sketch* sketch = m_viewport->activeSketch();
    if (!sketch) return;

    updateDofStatus(sketch);
    updateSketchConstraintUi();
}

void MainWindow::onSketchSelectionChanged() {
    updateSketchConstraintUi();
}

void MainWindow::updateSketchConstraintUi() {
    if (!m_viewport || !m_viewport->isInSketchMode()) {
        return;
    }

    core::sketch::Sketch* sketch = m_viewport->activeSketch();
    if (!sketch) {
        return;
    }

    const auto selection = m_viewport->sketchSelection();
    const auto applicability = evaluateConstraintApplicability(sketch, selection);

    std::vector<std::string> selectedEntityIds;
    selectedEntityIds.reserve(selection.size());
    QString selectedConstraintId;
    std::unordered_set<std::string> seenEntityIds;
    for (const auto& item : selection) {
        if (item.kind == app::selection::SelectionKind::SketchConstraint && selectedConstraintId.isEmpty()) {
            selectedConstraintId = QString::fromStdString(item.id.elementId);
            continue;
        }
        if (item.kind == app::selection::SelectionKind::SketchPoint ||
            item.kind == app::selection::SelectionKind::SketchEdge) {
            if (!item.id.elementId.empty() && seenEntityIds.insert(item.id.elementId).second) {
                selectedEntityIds.push_back(item.id.elementId);
            }
        }
    }

    if (m_constraintPanel) {
        const int conflicts = static_cast<int>(sketch->getConflictingConstraints().size());
        const int suppressedCount = m_viewport->suppressedConstraintMarkerCount();
        m_constraintPanel->setSketch(sketch);
        m_constraintPanel->setContext(selectedEntityIds,
                                      selectedConstraintId,
                                      sketch->getDegreesOfFreedom(),
                                      conflicts,
                                      suppressedCount);
        m_constraintPanel->setVisible(true);
        positionConstraintPanel();
    }

    if (m_sketchModePanel) {
        m_sketchModePanel->setSketch(sketch);
        m_sketchModePanel->setApplicableConstraints(applicability.applicableConstraints);
        const bool showTools = applicability.hasApplicableConstraints();
        m_sketchModePanel->setVisible(showTools);
        if (showTools) {
            positionSketchModePanel();
        } else if (m_toolStatus) {
            m_toolStatus->setText(tr("Select geometry to constrain"));
        }
    }
}

void MainWindow::onConstraintPanelConstraintSelected(const QString& constraintId) {
    if (!m_viewport || !m_viewport->isInSketchMode() || constraintId.isEmpty()) {
        return;
    }
    m_viewport->selectSketchConstraint(constraintId);
}


namespace {
// Captures a whole-sketch snapshot around a mutation scope and pushes ONE
// undoable command when anything actually changed (no-ops never reach the
// undo stack). Commits on scope exit so early returns are covered.
struct ScopedSketchGesture {
    std::unique_ptr<onecad::app::commands::SketchDragGestureCommand> cmd;
    onecad::app::commands::CommandProcessor* processor = nullptr;
    bool active = false;

    ScopedSketchGesture(onecad::app::Document* document, const std::string& sketchId,
                        std::string gestureLabel,
                        onecad::app::commands::CommandProcessor* commandProcessor)
        : cmd(std::make_unique<onecad::app::commands::SketchDragGestureCommand>(
              document, sketchId, std::move(gestureLabel)))
        , processor(commandProcessor) {
        active = cmd->beginGesture();
    }
    ~ScopedSketchGesture() {
        if (active && cmd && cmd->finalizeGesture() && cmd->hasCapturedChange() && processor) {
            processor->execute(std::move(cmd));
        }
    }
};
} // namespace

void MainWindow::onConstraintPanelDeleteRequested(const QString& constraintId) {
    if (!m_viewport || !m_viewport->isInSketchMode() || constraintId.isEmpty()) {
        return;
    }

    core::sketch::Sketch* sketch = m_viewport->activeSketch();
    auto* renderer = m_viewport->sketchRenderer();
    if (!sketch || !renderer) {
        return;
    }

    ScopedSketchGesture gesture(m_document.get(), m_activeSketchId,
                                "Delete Constraint", m_commandProcessor.get());
    const bool removed = sketch->removeConstraint(constraintId.toStdString());
    if (!removed) {
        m_toolStatus->setText(tr("Constraint could not be removed"));
        return;
    }

    m_viewport->unsuppressConstraintMarker(constraintId);

    auto result = sketch->solve();
    renderer->updateGeometry();
    renderer->updateConstraints();
    m_viewport->notifySketchUpdated();
    m_viewport->update();
    m_toolStatus->setText(result.success ? tr("Constraint removed")
                                         : tr("Constraint removed - solver failed"));
}

void MainWindow::onConstraintPanelSuppressRequested(const QString& constraintId) {
    if (!m_viewport || !m_viewport->isInSketchMode() || constraintId.isEmpty()) {
        return;
    }

    m_viewport->suppressConstraintMarker(constraintId);
    m_toolStatus->setText(tr("Constraint marker suppressed"));
    updateSketchConstraintUi();
}

void MainWindow::onConstraintPanelRestoreSuppressedRequested() {
    if (!m_viewport || !m_viewport->isInSketchMode()) {
        return;
    }
    m_viewport->clearSuppressedConstraintMarkers();
    m_toolStatus->setText(tr("Constraint markers restored"));
    updateSketchConstraintUi();
}

void MainWindow::onConstraintRequested(core::sketch::ConstraintType constraintType) {
    if (!m_viewport || !m_viewport->isInSketchMode()) {
        return;
    }

    core::sketch::Sketch* sketch = m_viewport->activeSketch();
    if (!sketch) {
        return;
    }

    // Get selected entities from renderer
    auto* renderer = m_viewport->sketchRenderer();
    if (!renderer) {
        return;
    }

    std::vector<core::sketch::EntityID> selected = renderer->getSelectedEntities();

    ScopedSketchGesture gesture(m_document.get(), m_activeSketchId,
                                "Add Constraint", m_commandProcessor.get());

    using CT = core::sketch::ConstraintType;
    CT type = constraintType;
    core::sketch::ConstraintID constraintId;

    const auto applicability = evaluateConstraintApplicability(sketch, m_viewport->sketchSelection());
    if (!applicability.isApplicable(type)) {
        m_toolStatus->setText(tr("Constraint unavailable for current selection"));
        return;
    }

    switch (type) {
        case CT::Horizontal:
            if (selected.size() == 1) {
                constraintId = sketch->addHorizontal(selected[0]);
            } else if (selected.size() == 2) {
                constraintId = sketch->addHorizontal(selected[0], selected[1]);
            }
            break;

        case CT::Vertical:
            if (selected.size() == 1) {
                constraintId = sketch->addVertical(selected[0]);
            } else if (selected.size() == 2) {
                constraintId = sketch->addVertical(selected[0], selected[1]);
            }
            break;

        case CT::Parallel:
            if (selected.size() == 2) {
                constraintId = sketch->addParallel(selected[0], selected[1]);
            } else {
                m_toolStatus->setText(tr("Parallel requires exactly 2 lines"));
                return;
            }
            break;

        case CT::Perpendicular:
            if (selected.size() == 2) {
                constraintId = sketch->addPerpendicular(selected[0], selected[1]);
            } else {
                m_toolStatus->setText(tr("Perpendicular requires exactly 2 lines"));
                return;
            }
            break;

        case CT::Coincident:
            if (selected.size() == 2) {
                constraintId = sketch->addCoincident(selected[0], selected[1]);
            } else {
                m_toolStatus->setText(tr("Coincident requires exactly 2 points"));
                return;
            }
            break;

        case CT::Fixed:
            if (selected.size() == 1) {
                constraintId = sketch->addFixed(selected[0]);
            } else {
                m_toolStatus->setText(tr("Fixed requires exactly 1 point"));
                return;
            }
            break;

        case CT::Distance:
            if (selected.size() == 2) {
                constraintId = sketch->addDistance(selected[0], selected[1], kDefaultDistanceMm);
            } else {
                m_toolStatus->setText(tr("Distance requires exactly 2 entities"));
                return;
            }
            break;

        case CT::HorizontalDistance:
            if (selected.size() == 2) {
                constraintId = sketch->addHorizontalDistance(selected[0], selected[1], kDefaultDistanceMm);
            } else {
                m_toolStatus->setText(tr("Horizontal Distance requires exactly 2 points"));
                return;
            }
            break;

        case CT::VerticalDistance:
            if (selected.size() == 2) {
                constraintId = sketch->addVerticalDistance(selected[0], selected[1], kDefaultDistanceMm);
            } else {
                m_toolStatus->setText(tr("Vertical Distance requires exactly 2 points"));
                return;
            }
            break;

        case CT::Angle:
            if (selected.size() == 2) {
                constraintId = sketch->addAngle(selected[0], selected[1], kDefaultAngleDeg);
            } else {
                m_toolStatus->setText(tr("Angle requires exactly 2 lines"));
                return;
            }
            break;

        case CT::Radius:
            if (selected.size() == 1) {
                constraintId = sketch->addRadius(selected[0], kDefaultRadiusMm);
            } else {
                m_toolStatus->setText(tr("Radius requires exactly 1 circle or arc"));
                return;
            }
            break;

        case CT::Diameter:
            if (selected.size() == 1) {
                constraintId = sketch->addDiameter(selected[0], kDefaultRadiusMm * 2.0);
            } else {
                m_toolStatus->setText(tr("Diameter requires exactly 1 circle or arc"));
                return;
            }
            break;

        case CT::Concentric:
            if (selected.size() == 2) {
                constraintId = sketch->addConcentric(selected[0], selected[1]);
            } else {
                m_toolStatus->setText(tr("Concentric requires exactly 2 arcs/circles"));
                return;
            }
            break;

        case CT::Symmetric: {
            core::sketch::EntityID point1;
            core::sketch::EntityID point2;
            core::sketch::EntityID axisLine;
            for (const auto& id : selected) {
                auto* entity = sketch->getEntity(id);
                if (!entity) continue;
                if (entity->type() == core::sketch::EntityType::Point) {
                    if (point1.empty()) {
                        point1 = id;
                    } else if (point2.empty()) {
                        point2 = id;
                    } else {
                        m_toolStatus->setText(tr("Symmetric requires exactly 2 points"));
                        return;
                    }
                } else if (entity->type() == core::sketch::EntityType::Line) {
                    if (!axisLine.empty()) {
                        m_toolStatus->setText(tr("Symmetric requires exactly 1 axis line"));
                        return;
                    }
                    axisLine = id;
                }
            }
            if (point1.empty() || point2.empty() || axisLine.empty()) {
                m_toolStatus->setText(tr("Symmetric requires 2 points and 1 axis line"));
                return;
            }
            constraintId = sketch->addSymmetric(point1, point2, axisLine);
            break;
        }

        case CT::OnCurve: {
            // Need exactly 1 point and 1 curve
            core::sketch::EntityID pointId, curveId;
            for (const auto& id : selected) {
                auto* entity = sketch->getEntity(id);
                if (!entity) continue;

                if (entity->type() == core::sketch::EntityType::Point) {
                    if (!pointId.empty()) {
                        m_toolStatus->setText(tr("Point On Curve requires exactly 1 point"));
                        return;
                    }
                    pointId = id;
                } else if (entity->type() == core::sketch::EntityType::Arc ||
                           entity->type() == core::sketch::EntityType::Circle ||
                           entity->type() == core::sketch::EntityType::Line) {
                    if (!curveId.empty()) {
                        m_toolStatus->setText(tr("Point On Curve requires exactly 1 curve"));
                        return;
                    }
                    curveId = id;
                }
            }

            if (pointId.empty() || curveId.empty()) {
                m_toolStatus->setText(tr("Point On Curve requires 1 point and 1 curve"));
                return;
            }

            // Auto-detect position (Start/End/Arbitrary)
            constraintId = sketch->addPointOnCurve(pointId, curveId);
            break;
        }

        default:
            // Other constraint types not yet implemented
            m_toolStatus->setText(tr("Constraint type not implemented"));
            return;
    }

    if (!constraintId.empty()) {
        // Solve and update
        auto result = sketch->solve();
        if (result.success) {
            renderer->updateGeometry();
            renderer->setConflictingConstraints(sketch->getConflictingConstraints());
            renderer->updateConstraints();
            m_viewport->notifySketchUpdated();
            m_viewport->update();
            m_toolStatus->setText(tr("Constraint applied"));
        } else {
            // Solver failed - show error to user
            m_toolStatus->setText(tr("Constraint applied - solver failed (over-constrained or conflicting)"));
            // Note: constraint was still added to sketch, just not solved
            // Could optionally remove the constraint here if desired
            renderer->setConflictingConstraints(sketch->getConflictingConstraints());
            renderer->updateConstraints();
        }
    } else {
        if (type == CT::Fixed && selected.size() == 1) {
            m_toolStatus->setText(tr("Fixed requires a point (select a point, not an edge)"));
        } else {
            m_toolStatus->setText(tr("Cannot apply constraint to selection"));
        }
    }

    updateSketchConstraintUi();
}

void MainWindow::onDeleteItem(const QString& itemId) {
    std::string id = itemId.toStdString();

    // Determine item type
    bool isBody = m_document->getBodyShape(id) != nullptr;
    bool isSketch = m_document->getSketch(id) != nullptr;

    if (!isBody && !isSketch) {
        return;
    }

    // Get item name for confirmation
    QString itemName = isBody
        ? QString::fromStdString(m_document->getBodyName(id))
        : QString::fromStdString(m_document->getSketchName(id));

    // Always show confirmation dialog
    if (QMessageBox::question(this, tr("Confirm Delete"),
            tr("Delete '%1'?").arg(itemName),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    // If deleting active sketch, exit sketch mode first
    if (isSketch && m_activeSketchId == id) {
        onExitSketch();
    }

    // Create and execute command
    if (isBody) {
        auto cmd = std::make_unique<app::commands::DeleteBodyCommand>(m_document.get(), id);
        m_commandProcessor->execute(std::move(cmd));
    } else {
        auto cmd = std::make_unique<app::commands::DeleteSketchCommand>(m_document.get(), id);
        m_commandProcessor->execute(std::move(cmd));
    }

    m_viewport->update();
}

void MainWindow::onRenameItem(const QString& itemId) {
    // Trigger inline edit in navigator
    m_navigator->startInlineEdit(itemId);
}

void MainWindow::onVisibilityToggled(const QString& itemId, bool visible) {
    std::string id = itemId.toStdString();
    bool isBody = m_document->getBodyShape(id) != nullptr;

    auto cmd = std::make_unique<app::commands::ToggleVisibilityCommand>(
        m_document.get(), id,
        isBody ? app::commands::ToggleVisibilityCommand::ItemType::Body
               : app::commands::ToggleVisibilityCommand::ItemType::Sketch,
        visible);
    m_commandProcessor->execute(std::move(cmd));

    m_viewport->update();
}

void MainWindow::onIsolateItem(const QString& itemId) {
    std::string id = itemId.toStdString();

    // Toggle isolation
    if (m_document->isolatedItemId() == id) {
        m_document->clearIsolation();
    } else {
        m_document->isolateItem(id);
    }

    m_viewport->update();
}

void MainWindow::onInspectorBodySelected(const QString& bodyId) {
    if (!m_propertyInspector) {
        return;
    }
    if (bodyId.isEmpty()) {
        onInspectorSelectionCleared();
        return;
    }
    m_propertyInspector->showBodyProperties(bodyId);
    if (!m_inspectorUserHidden && !m_propertyInspector->isVisible()) {
        m_propertyInspector->setVisible(true);
    }
}

void MainWindow::onInspectorOperationSelected(const QString& opId) {
    if (!m_propertyInspector || opId.isEmpty()) {
        return;
    }
    m_propertyInspector->showOperationProperties(opId);
    if (!m_inspectorUserHidden && !m_propertyInspector->isVisible()) {
        m_propertyInspector->setVisible(true);
    }
}

void MainWindow::onInspectorSelectionCleared() {
    if (m_propertyInspector) {
        m_propertyInspector->showEmptyState();
    }
}

void MainWindow::loadSettings() {
    QSettings settings("OneCAD", "OneCAD");

    // Restore saved camera angle (default to 45 degrees if not set)
    float savedAngle = settings.value("viewport/cameraAngle", 45.0f).toFloat();

    if (m_cameraAngleSlider) {
        m_cameraAngleSlider->setValue(static_cast<int>(savedAngle));
    }

    if (m_viewport) {
        m_viewport->setCameraAngle(savedAngle);
    }

    // Restore Inspector dock visibility (default hidden).
    const bool inspectorVisible = settings.value("ui/inspectorVisible", false).toBool();
    m_inspectorUserHidden = !inspectorVisible;
    if (m_propertyInspector) {
        m_propertyInspector->setVisible(inspectorVisible);
    }
    if (m_inspectorAction) {
        QSignalBlocker block(m_inspectorAction);
        m_inspectorAction->setChecked(inspectorVisible);
    }
}

void MainWindow::saveSettings() {
    QSettings settings("OneCAD", "OneCAD");

    // Save current camera angle
    if (m_viewport && m_viewport->camera()) {
        float currentAngle = m_viewport->camera()->cameraAngle();
        settings.setValue("viewport/cameraAngle", currentAngle);
    }

    if (m_propertyInspector) {
        settings.setValue("ui/inspectorVisible", m_propertyInspector->isVisible());
    }

    settings.sync(); // Force immediate write
}

void MainWindow::setupSnapOverlay() {
    if (!m_viewport) return;

    m_snapSettingsButton = new SidebarToolButton(":/icons/ic_snap.svg", tr("Snap Settings"), m_viewport);
    m_snapSettingsButton->setFixedSize(42, 42);
    m_snapSettingsButton->setVisible(true);

    m_snapSettingsPanel = new SnapSettingsPanel(m_viewport);
    m_snapSettingsPanel->setVisible(false);

    connect(m_snapSettingsButton, &SidebarToolButton::clicked, this, [this]() {
        if (m_snapSettingsPanel) {
            m_snapSettingsPanel->setVisible(!m_snapSettingsPanel->isVisible());
            positionSnapSettingsPanel();
        }
    });
    
    // Connect settings changes to viewport
    connect(m_snapSettingsPanel, &SnapSettingsPanel::settingsChanged, this, [this]() {
        if (m_viewport && m_snapSettingsPanel) {
            m_viewport->updateSnapSettings(m_snapSettingsPanel->settings());
        }
    });

    positionSnapOverlay();
}

void MainWindow::positionSnapOverlay() {
    if (!m_viewport || !m_snapSettingsButton) return;

    const int margin = 20;
    int x = m_viewport->width() - m_snapSettingsButton->width() - margin;
    int y = margin;
    m_snapSettingsButton->move(x, y);
    m_snapSettingsButton->raise();
}

void MainWindow::positionSnapSettingsPanel() {
    if (!m_viewport || !m_snapSettingsPanel || !m_snapSettingsButton) return;
    
    if (!m_snapSettingsPanel->isVisible()) return;

    const int margin = 10;
    int x = m_snapSettingsButton->x() - m_snapSettingsPanel->width() - margin;
    int y = m_snapSettingsButton->y();
    
    // Ensure it doesn't go off screen
    if (x < margin) x = margin;
    
    m_snapSettingsPanel->move(x, y);
    m_snapSettingsPanel->raise();
}

void MainWindow::setupViewControlButtons() {
    if (!m_viewport) return;

    // Fit-to-view (frames all visible geometry; matches the View > Zoom to Fit / 0).
    m_fitViewButton = SidebarToolButton::fromSvgIcon(
        ":/icons/ic_fit.svg", tr("Fit to view (0)"), m_viewport);
    m_fitViewButton->setFixedSize(42, 42);
    m_fitViewButton->setVisible(true);
    connect(m_fitViewButton, &SidebarToolButton::clicked, this, [this]() {
        if (m_viewport) m_viewport->fitToView();
    });

    // Home (default isometric view; matches View > Isometric / 7).
    m_homeViewButton = SidebarToolButton::fromSvgIcon(
        ":/icons/ic_view_iso.svg", tr("Home / Isometric (7)"), m_viewport);
    m_homeViewButton->setFixedSize(42, 42);
    m_homeViewButton->setVisible(true);
    connect(m_homeViewButton, &SidebarToolButton::clicked, this, [this]() {
        if (m_viewport) m_viewport->setIsometricView();
    });

    positionViewControlButtons();
}

void MainWindow::positionViewControlButtons() {
    if (!m_viewport || !m_fitViewButton || !m_homeViewButton) return;

    // Bottom-left stack (Home above Fit): clear of the top-left overlay buttons,
    // the top-right ViewCube/snap/history cluster, and the right-side history panel.
    const int margin = 20;
    const int spacing = 8;
    const int x = margin;
    const int fitY = m_viewport->height() - m_fitViewButton->height() - margin;
    const int homeY = fitY - m_homeViewButton->height() - spacing;
    m_homeViewButton->move(x, homeY);
    m_homeViewButton->raise();
    m_fitViewButton->move(x, fitY);
    m_fitViewButton->raise();
}

void MainWindow::setupHistoryPanel() {
    if (!m_viewport) return;

    // Create history panel (right sidebar)
    QWidget* parent = centralWidget() ? centralWidget() : m_viewport;
    m_historyPanel = new HistoryPanel(parent);
    m_historyPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    if (m_centralLayout) {
        m_centralLayout->addWidget(m_historyPanel);
    }
    m_historyPanel->setDocument(m_document.get());
    m_historyPanel->setViewport(m_viewport);
    m_historyPanel->setCommandProcessor(m_commandProcessor.get());

    connect(m_historyPanel, &HistoryPanel::rollbackRequested, this, [this](const QString& opId) {
        auto cmd = std::make_unique<app::commands::RollbackCommand>(
            m_document.get(), opId.toStdString());
        m_commandProcessor->execute(std::move(cmd));
        if (m_historyPanel) {
            m_historyPanel->rebuild();
        }
        if (m_viewport) {
            m_viewport->update();
        }
    });

    connect(m_historyPanel, &HistoryPanel::suppressRequested, this,
            [this](const QString& opId, bool suppress) {
                auto cmd = std::make_unique<app::commands::SetOperationSuppressionCommand>(
                    m_document.get(), opId.toStdString(), suppress);
                m_commandProcessor->execute(std::move(cmd));
                if (m_viewport) {
                    m_viewport->update();
                }
            });

    connect(m_historyPanel, &HistoryPanel::deleteRequested, this, [this](const QString& opId) {
        auto cmd = std::make_unique<app::commands::RemoveOperationCommand>(
            m_document.get(), opId.toStdString());
        m_commandProcessor->execute(std::move(cmd));
        if (m_viewport) {
            m_viewport->update();
        }
    });

    connect(m_historyPanel, &HistoryPanel::editRequested, this, [this](const QString& opId) {
        showEditParameterOverlay(opId);
    });

    // Single-click selecting a history row populates the Inspector (distinct from the
    // double-click/edit interaction above, which opens the parameter overlay).
    connect(m_historyPanel, &HistoryPanel::operationSelected, this,
            &MainWindow::onInspectorOperationSelected);

    // Create toggle button
    m_historyOverlayButton = new SidebarToolButton("H", tr("Toggle History Panel (Cmd+H)"), m_viewport);
    m_historyOverlayButton->setFixedSize(42, 42);
    m_historyOverlayButton->setVisible(true);

    connect(m_historyOverlayButton, &SidebarToolButton::clicked, this, [this]() {
        if (m_historyPanel) {
            m_historyPanel->setCollapsed(!m_historyPanel->isCollapsed());
        }
    });

    connect(m_historyPanel, &HistoryPanel::collapsedChanged, this, [this](bool collapsed) {
        if (m_historyOverlayButton) {
            m_historyOverlayButton->setToolTip(collapsed ? tr("Show history")
                                                         : tr("Hide history"));
        }
        QSettings settings("OneCAD", "OneCAD");
        settings.setValue("ui/historyPanelVisible", !collapsed);
    });

    auto* shortcut = new QShortcut(QKeySequence("Ctrl+H"), m_viewport);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        if (m_historyPanel) {
            m_historyPanel->setCollapsed(!m_historyPanel->isCollapsed());
        }
    });

    QSettings settings("OneCAD", "OneCAD");
    const bool historyVisible = settings.value("ui/historyPanelVisible", false).toBool();
    m_historyPanel->setCollapsed(!historyVisible);
    m_historyOverlayButton->setToolTip(historyVisible ? tr("Hide history")
                                                      : tr("Show history"));

    positionHistoryPanel();
}

void MainWindow::positionHistoryPanel() {
    if (!m_viewport || !m_historyOverlayButton) return;

    const int margin = 20;
    int x = m_viewport->width() - m_historyOverlayButton->width() - margin;
    int y = margin;
    
    // Position below Snap button if it exists
    if (m_snapSettingsButton && m_snapSettingsButton->isVisible()) {
        y = m_snapSettingsButton->y() + m_snapSettingsButton->height() + 10;
    }

    m_historyOverlayButton->move(x, y);
    m_historyOverlayButton->raise();
}

} // namespace ui
} // namespace onecad
