#include "Viewport.h"
#include "ViewportCommon.h"
#include "../../render/BodyRenderer.h"
#include "../../render/Camera3D.h"
#include "../../render/Grid3D.h"
#include "../../core/sketch/SketchRenderer.h"
#include "../../core/sketch/Sketch.h"
#include "../../core/sketch/tools/SketchToolManager.h"
#include "../../core/modeling/SelectionTopologyResolver.h"
#include "../../app/document/Document.h"
#include "../../app/selection/SelectionManager.h"
#include "../../app/selection/SelectionTypes.h"
#include "../selection/ModelPickerAdapter.h"
#include "../tools/ModelingToolManager.h"
#include "../viewcube/ViewCube.h"
#include "../theme/ThemeManager.h"

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QResizeEvent>
#include <QVector2D>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace onecad {
namespace ui {

namespace sketch = core::sketch;

using namespace viewport_common;

void Viewport::initializeGL() {
    initializeOpenGLFunctions();

    // Background color set via updateTheme
    glClearColor(m_backgroundColor.redF(), m_backgroundColor.greenF(), m_backgroundColor.blueF(), m_backgroundColor.alphaF());

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // Disable states we don't want by default
    glDisable(GL_CULL_FACE);

    m_grid->initialize();

    m_bodyRenderer = std::make_unique<render::BodyRenderer>();
    m_bodyRenderer->initialize();

    // Create and initialize sketch renderer (requires OpenGL context)
    m_sketchRenderer = std::make_unique<sketch::SketchRenderer>();
    if (!m_sketchRenderer->initialize()) {
        qWarning() << "Failed to initialize SketchRenderer";
    }
    updateTheme();
}

void Viewport::updateTheme() {
    const ThemeDefinition& theme = ThemeManager::instance().currentTheme();
    m_backgroundColor = theme.viewport.background;
    if (m_grid) {
        m_grid->setMajorColor(theme.viewport.grid.major);
        m_grid->setMinorColor(theme.viewport.grid.minor);
        m_grid->setAxisColors(theme.viewport.grid.axisX,
                              theme.viewport.grid.axisY,
                              theme.viewport.grid.axisZ);
        m_grid->forceUpdate();
    }
    if (m_sketchRenderer) {
        sketch::SketchRenderStyle style = m_sketchRenderer->getStyle();
        applySketchColors(theme.sketch, &style);
        m_sketchRenderer->setStyle(style);
    }
    const auto& body = theme.viewport.body;
    m_renderTuning.keyLightDir = body.keyLightDir;
    m_renderTuning.fillLightDir = body.fillLightDir;
    m_renderTuning.fillLightIntensity = body.fillLightIntensity;
    m_renderTuning.ambientIntensity = body.ambientIntensity;
    m_renderTuning.hemiUpDir = body.hemiUpDir;
    m_renderTuning.gradientDir = body.ambientGradientDir;
    m_renderTuning.gradientStrength = body.ambientGradientStrength;
    update();
}

void Viewport::resizeGL(int w, int h) {
    m_width = w > 0 ? w : 1;
    m_height = h > 0 ? h : 1;

    // Handle Retina/High-DPI displays
    const qreal ratio = devicePixelRatio();
    glViewport(0, 0, static_cast<GLsizei>(m_width * ratio), static_cast<GLsizei>(m_height * ratio));
}

void Viewport::resizeEvent(QResizeEvent* event) {
    QOpenGLWidget::resizeEvent(event);
    if (m_viewCube) {
        // Position top-right with margin
        m_viewCube->move(width() - m_viewCube->width() - 20, 20);
    }
}

void Viewport::paintGL() {
    // Ensure viewport is set correctly with correct device pixel ratio
    const qreal ratio = devicePixelRatio();
    glViewport(0, 0, static_cast<GLsizei>(m_width * ratio), static_cast<GLsizei>(m_height * ratio));

    // Clear to background color
    glClearColor(m_backgroundColor.redF(), m_backgroundColor.greenF(), m_backgroundColor.blueF(), m_backgroundColor.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Reset depth test state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
    RenderClipPlanes clipPlanes;
    QMatrix4x4 projection = buildProjectionMatrix(aspectRatio, &clipPlanes);
    QMatrix4x4 view = m_camera->viewMatrix();
    QMatrix4x4 viewProjection = projection * view;

    // Calculate pixel scale for sketch rendering and adaptive grid
    double pixelScale = 1.0;
    float dist = m_camera->distance();
    float fov = m_camera->fov();

    if (m_camera->projectionType() == render::Camera3D::ProjectionType::Orthographic) {
        double worldHeight = static_cast<double>(m_camera->orthoScale());
        pixelScale = worldHeight / (static_cast<double>(m_height) * ratio);
    } else {
        double halfFov = qDegreesToRadians(fov * 0.5f);
        double worldHeight = 2.0 * static_cast<double>(dist) * std::tan(halfFov);
        pixelScale = worldHeight / (static_cast<double>(m_height) * ratio);
    }

    if (pixelScale <= 0.0) {
        pixelScale = 1.0;
    }
    m_pixelScale = pixelScale;

    const double maxDimension = static_cast<double>(qMax(m_width, m_height));
    const double viewExtent = 0.5 * maxDimension * ratio * pixelScale;

    // Render grid
    sketch::SketchPlane gridPlane =
        (m_inSketchMode && m_activeSketch) ? m_activeSketch->getPlane() : sketch::SketchPlane::XY();
    PlaneAxes gridAxes = buildPlaneAxes(gridPlane);
    QVector3D gridOrigin(gridPlane.origin.x, gridPlane.origin.y, gridPlane.origin.z);
    QVector3D gridNormal = gridAxes.normal;
    QVector3D gridXAxis = gridAxes.xAxis;
    QVector3D gridYAxis = gridAxes.yAxis;

    QMatrix4x4 gridModel = buildPlaneModelMatrix(gridOrigin, gridXAxis, gridYAxis, gridNormal);
    QMatrix4x4 gridMvp = viewProjection * gridModel;

    QVector3D target = m_camera->target();
    QVector3D forward = m_camera->forward();
    QVector3D position = m_camera->position();

    float planeDistance = dist;
    QVector3D planeAnchorWorld = gridOrigin;
    bool hasPlaneHit = intersectRayWithPlane(position,
                                             forward,
                                             gridOrigin,
                                             gridNormal,
                                             &planeAnchorWorld,
                                             &planeDistance);
    if (!hasPlaneHit) {
        float targetSignedDistance = QVector3D::dotProduct(target - gridOrigin, gridNormal);
        planeAnchorWorld = target - gridNormal * targetSignedDistance;
    }

    float depthScale = 1.0f;
    if (m_camera->projectionType() == render::Camera3D::ProjectionType::Perspective &&
        dist > 1e-4f) {
        depthScale = planeDistance / dist;
    }
    depthScale = std::clamp(depthScale, kGridDepthScaleMin, kGridDepthScaleMax);

    QVector2D planeAnchor = worldToPlaneCoords(planeAnchorWorld, gridOrigin, gridXAxis, gridYAxis);
    float viewHalf = static_cast<float>(viewExtent) * depthScale;
    QRectF fallbackBounds(QPointF(planeAnchor.x() - viewHalf, planeAnchor.y() - viewHalf),
                          QPointF(planeAnchor.x() + viewHalf, planeAnchor.y() + viewHalf));

    QRectF gridBounds = fallbackBounds;
    QRectF frustumBounds;
    bool hasFrustumBounds = computePlaneBoundsOnPlane(viewProjection,
                                                      gridOrigin,
                                                      gridNormal,
                                                      gridXAxis,
                                                      gridYAxis,
                                                      &frustumBounds);
    if (hasFrustumBounds) {
        float maxHalf = 0.5f * static_cast<float>(qMax(frustumBounds.width(),
                                                       frustumBounds.height()));
        QVector2D anchor2d(planeAnchor.x(), planeAnchor.y());
        QVector2D frustumCenter(static_cast<float>(frustumBounds.center().x()),
                                static_cast<float>(frustumBounds.center().y()));
        float centerDistance = (frustumCenter - anchor2d).length();
        float maxAllowed = viewHalf * kGridBoundsMaxScale;

        if (maxHalf > maxAllowed || centerDistance > maxAllowed) {
            hasFrustumBounds = false;
        }
    }
    if (hasFrustumBounds) {
        gridBounds = frustumBounds;
    }

    float minX = static_cast<float>(gridBounds.left());
    float maxX = static_cast<float>(gridBounds.right());
    float minY = static_cast<float>(gridBounds.top());
    float maxY = static_cast<float>(gridBounds.bottom());

    QVector2D fadeOriginPlane = worldToPlaneCoords(position, gridOrigin, gridXAxis, gridYAxis);

    m_grid->render(gridMvp,
                   static_cast<float>(pixelScale),
                   QVector2D(minX, minY),
                   QVector2D(maxX, maxY),
                   fadeOriginPlane);

    if (m_bodyRenderer) {
        render::BodyRenderer::RenderStyle style;
        const ThemeDefinition& theme = ThemeManager::instance().currentTheme();
        style.baseColor = theme.viewport.body.base;
        style.edgeColor = theme.viewport.body.edge;
        style.specularColor = theme.viewport.body.specular;
        style.rimColor = theme.viewport.body.rim;
        style.glowColor = theme.viewport.body.glow;
        style.highlightColor = theme.viewport.body.highlight;
        style.hemiSkyColor = theme.viewport.body.hemiSky;
        style.hemiGroundColor = theme.viewport.body.hemiGround;
        style.highlightStrength = 0.08f;
        style.drawGlow = true;
        style.glowAlpha = 0.18f;
        style.drawEdges = true;
        style.previewAlpha = 0.35f;
        if (!m_previewHiddenBodyId.empty()) {
            style.previewAlpha = 0.8f;
        }
        style.keyLightDir = m_renderTuning.keyLightDir;
        style.fillLightDir = m_renderTuning.fillLightDir;
        style.fillLightIntensity = m_renderTuning.fillLightIntensity;
        style.ambientIntensity = m_renderTuning.ambientIntensity;
        style.hemiUpDir = m_renderTuning.hemiUpDir;
        style.ambientGradientStrength = m_renderTuning.gradientStrength;
        style.ambientGradientDir = m_renderTuning.gradientDir;
        if (m_inSketchMode) {
            style.ghosted = true;
            style.ghostFactor = 0.6f;
            style.baseAlpha = 0.25f;
            style.edgeAlpha = 0.5f;
            style.glowAlpha = 0.12f;
            style.highlightStrength = 0.04f;
        }

        // Debug visualization modes (F1-F5)
        style.debugNormals = m_debugNormals;
        style.debugDepth = m_debugDepth;
        style.wireframeOnly = m_wireframeOnly;
        style.disableGamma = m_disableGamma;
        style.useMatcap = m_useMatcap;
        style.nearPlane = clipPlanes.nearPlane;
        style.farPlane = clipPlanes.farPlane;
        style.isOrtho = (m_camera->projectionType() == render::Camera3D::ProjectionType::Orthographic);

        // Dynamic quality: reduce during navigation for better responsiveness
        if (m_isNavigating) {
            style.drawEdges = false;
            style.drawGlow = false;
        }

        m_bodyRenderer->render(viewProjection, view, style);
    }

    // Render sketch(es)
    if (m_inSketchMode && m_activeSketch && m_sketchRenderer) {
        // In sketch mode: render only the active sketch with tool preview
        const auto& plane = m_activeSketch->getPlane();
        const SketchPlaneViewportInfo sketchViewportInfo = buildSketchViewportForPlane(
            plane,
            *m_camera,
            viewProjection,
            m_width,
            m_height,
            ratio,
            pixelScale
        );
        m_sketchRenderer->setViewport(sketchViewportInfo.viewport);
        m_sketchRenderer->setPixelScale(pixelScale);

        // Render tool preview
        if (m_toolManager) {
            m_toolManager->renderPreview();
        }

        m_sketchRenderer->render(view, projection);

        // Render preview dimensions overlay
        const auto& dims = m_sketchRenderer->getPreviewDimensions();
        if (!dims.empty()) {
            // Unbind GL context resources before QPainter to be safe
            // QPainter painter(this) automatically handles GL state for the widget
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);

            const ThemeDefinition& theme = ThemeManager::instance().currentTheme();
            QColor textColor = theme.viewport.overlay.previewDimensionText;
            QColor bgColor = theme.viewport.overlay.previewDimensionBackground;

            QFont font = painter.font();
            font.setPointSize(10);
            font.setBold(true);
            painter.setFont(font);

            PlaneAxes axes = buildPlaneAxes(plane);
            QVector3D origin(plane.origin.x, plane.origin.y, plane.origin.z);

            for (const auto& dim : dims) {
                // Calculate world position: origin + xAxis * x + yAxis * y
                QVector3D worldPos = origin +
                                   axes.xAxis * dim.position.x +
                                   axes.yAxis * dim.position.y;

                QPointF screenPos;
                if (projectToScreen(viewProjection, worldPos,
                                  static_cast<float>(width()),
                                  static_cast<float>(height()),
                                  &screenPos)) {

                    QString text = QString::fromStdString(dim.text);
                    QFontMetrics fm(font);
                    int textWidth = fm.horizontalAdvance(text);
                    int textHeight = fm.height();
                    int padding = 4;

                    QRectF bgRect(screenPos.x() - textWidth / 2 - padding,
                                screenPos.y() - textHeight / 2 - padding,
                                textWidth + 2 * padding,
                                textHeight + 2 * padding);

                    painter.setPen(Qt::NoPen);
                    painter.setBrush(bgColor);
                    painter.drawRoundedRect(bgRect, 4, 4);

                    painter.setPen(textColor);
                    painter.drawText(bgRect, Qt::AlignCenter, text);
                }
            }
        }

        // Render snap hint overlay
        const auto& snap = m_sketchRenderer->getSnapIndicator();
        if (snap.active && !snap.hintText.empty()) {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);

            const ThemeDefinition& theme = ThemeManager::instance().currentTheme();
            QColor textColor = theme.viewport.overlay.previewDimensionText;
            QColor bgColor = theme.viewport.overlay.previewDimensionBackground;

            QFont font = painter.font();
            font.setPointSize(10);
            font.setBold(true);
            painter.setFont(font);

            PlaneAxes axes = buildPlaneAxes(plane);
            QVector3D origin(plane.origin.x, plane.origin.y, plane.origin.z);

            QVector3D worldPos = origin +
                               axes.xAxis * snap.position.x +
                               axes.yAxis * snap.position.y;

            QPointF screenPos;
            if (projectToScreen(viewProjection, worldPos,
                              static_cast<float>(width()),
                              static_cast<float>(height()),
                              &screenPos)) {

                QString text = QString::fromStdString(snap.hintText);
                QFontMetrics fm(font);
                int textWidth = fm.horizontalAdvance(text);
                int textHeight = fm.height();
                int padding = 4;

                // Offset ~20px above snap point
                QRectF bgRect(screenPos.x() - textWidth / 2 - padding,
                            screenPos.y() - textHeight / 2 - padding - 20,
                            textWidth + 2 * padding,
                            textHeight + 2 * padding);

                painter.setPen(Qt::NoPen);
                painter.setBrush(bgColor);
                painter.drawRoundedRect(bgRect, 4, 4);

                painter.setPen(textColor);
                painter.drawText(bgRect, Qt::AlignCenter, text);
            }
        }
    } else if (m_document && m_sketchRenderer) {
        const auto visibleSketchIds = visibleModelSketchIds();
        if (!visibleSketchIds.empty()) {
            const sketch::SketchRenderStyle previousStyle = m_sketchRenderer->getStyle();
            sketch::SketchRenderStyle ghostStyle = previousStyle;
            ghostStyle.colors.normalGeometry.x *= 0.6;
            ghostStyle.colors.normalGeometry.y *= 0.6;
            ghostStyle.colors.normalGeometry.z *= 0.6;
            ghostStyle.colors.constructionGeometry.x *= 0.6;
            ghostStyle.colors.constructionGeometry.y *= 0.6;
            ghostStyle.colors.constructionGeometry.z *= 0.6;
            ghostStyle.colors.selectedGeometry.x *= 0.7;
            ghostStyle.colors.selectedGeometry.y *= 0.7;
            ghostStyle.colors.selectedGeometry.z *= 0.7;
            ghostStyle.regionOpacity *= 0.4f;
            ghostStyle.regionHoverOpacity *= 0.4f;
            ghostStyle.regionSelectedOpacity *= 0.6f;
            m_sketchRenderer->setStyle(ghostStyle);

            for (const auto& sketchId : visibleSketchIds) {
                auto* sketchModel = m_document->getSketch(sketchId);
                if (!sketchModel) {
                    continue;
                }

                m_sketchRenderer->setSketch(sketchModel);
                applySketchOverlayStateForSketch(sketchId);

                const auto& plane = sketchModel->getPlane();
                const SketchPlaneViewportInfo sketchViewportInfo = buildSketchViewportForPlane(
                    plane,
                    *m_camera,
                    viewProjection,
                    m_width,
                    m_height,
                    ratio,
                    pixelScale
                );
                m_sketchRenderer->setViewport(sketchViewportInfo.viewport);
                m_sketchRenderer->setPixelScale(pixelScale);
                m_sketchRenderer->render(view, projection);
            }

            m_sketchRenderer->setStyle(previousStyle);
            m_documentSketchesDirty = false;
        }
    }

    if (m_planeSelectionActive) {
        glDisable(GL_DEPTH_TEST);
        drawPlaneSelectionOverlay(viewProjection);
    }

    drawModelSelectionOverlay(viewProjection);
    drawModelToolOverlay(viewProjection);

    // Box selection rubber band
    if (m_boxSelectionActive) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QRect rect = QRect(m_boxSelectionStart, m_boxSelectionEnd).normalized();

        // L->R = blue (window), R->L = green (crossing)
        bool crossing = (m_boxSelectionEnd.x() < m_boxSelectionStart.x());
        QColor fillColor = crossing ? QColor(0, 200, 0, 30) : QColor(0, 120, 215, 30);
        QColor borderColor = crossing ? QColor(0, 200, 0, 180) : QColor(0, 120, 215, 180);

        painter.setPen(QPen(borderColor, 1, crossing ? Qt::DashLine : Qt::SolidLine));
        painter.setBrush(fillColor);
        painter.drawRect(rect);
    }
}

void Viewport::setDebugToggles(bool normals, bool depth, bool wireframe, bool disableGamma, bool matcap) {
    if (normals && depth) {
        depth = false;
    }
    if (m_debugNormals == normals &&
        m_debugDepth == depth &&
        m_wireframeOnly == wireframe &&
        m_disableGamma == disableGamma &&
        m_useMatcap == matcap) {
        return;
    }

    m_debugNormals = normals;
    m_debugDepth = depth;
    m_wireframeOnly = wireframe;
    m_disableGamma = disableGamma;
    m_useMatcap = matcap;
    update();
    emit debugTogglesChanged(m_debugNormals, m_debugDepth, m_wireframeOnly, m_disableGamma, m_useMatcap);
}

void Viewport::setRenderLightRig(const QVector3D& keyDir,
                                 const QVector3D& fillDir,
                                 float fillIntensity,
                                 float ambientIntensity,
                                 const QVector3D& hemiUpDir,
                                 const QVector3D& gradientDir,
                                 float gradientStrength) {
    m_renderTuning.keyLightDir = keyDir;
    m_renderTuning.fillLightDir = fillDir;
    m_renderTuning.fillLightIntensity = fillIntensity;
    m_renderTuning.ambientIntensity = ambientIntensity;
    m_renderTuning.hemiUpDir = hemiUpDir;
    m_renderTuning.gradientDir = gradientDir;
    m_renderTuning.gradientStrength = gradientStrength;
    update();
}

void Viewport::toggleGrid() {
    m_grid->setVisible(!m_grid->isVisible());
    update();
}

QImage Viewport::captureThumbnail(int maxSize) {
    if (maxSize <= 0) {
        return {};
    }
    if (!m_camera) {
        return {};
    }

    // Qt6 grabFramebuffer() handles MSAA internally
    const QVector3D savedPosition = m_camera->position();
    const QVector3D savedTarget = m_camera->target();
    const QVector3D savedUp = m_camera->up();
    const float savedCameraAngle = m_camera->cameraAngle();
    const float savedFov = m_camera->fov();
    const float savedOrthoScale = m_camera->orthoScale();

    m_camera->setCameraAngle(kThumbnailCameraAngle);
    m_camera->setIsometricView();

    QImage frame = grabFramebuffer();

    m_camera->setCameraAngle(savedCameraAngle);
    m_camera->setFov(savedFov);
    m_camera->setOrthoScale(savedOrthoScale);
    m_camera->setPosition(savedPosition);
    m_camera->setTarget(savedTarget);
    m_camera->setUp(savedUp);

    if (frame.isNull()) {
        return {};
    }
    frame.setDevicePixelRatio(1.0);

    QImage scaled = frame;
    if (frame.width() > maxSize || frame.height() > maxSize) {
        scaled = frame.scaled(maxSize, maxSize,
                              Qt::KeepAspectRatio,
                              Qt::SmoothTransformation);
    }

    QImage result(QSize(maxSize, maxSize), QImage::Format_ARGB32_Premultiplied);
    if (result.isNull()) {
        return {};
    }

    QColor background = m_backgroundColor.isValid() ? m_backgroundColor : QColor(32, 32, 32);
    background.setAlpha(255);
    result.fill(background);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const int x = (maxSize - scaled.width()) / 2;
    const int y = (maxSize - scaled.height()) / 2;
    painter.drawImage(QPoint(x, y), scaled);
    return result;
}

void Viewport::drawModelSelectionOverlay(const QMatrix4x4& viewProjection) {
    if (!m_selectionManager || !m_modelPicker || m_inSketchMode) {
        return;
    }

    const auto hover = m_selectionManager->hover();
    const auto& selection = m_selectionManager->selection();
    if (!hover.has_value() && selection.empty()) {
        return;
    }

    struct HighlightStyle {
        QColor faceFillHover;
        QColor faceOutlineHover;
        QColor faceFillSelected;
        QColor faceOutlineSelected;
        QColor edgeHover;
        QColor edgeSelected;
        QColor vertexHover;
        QColor vertexSelected;
    };

    HighlightStyle style;
    const ThemeViewportSelectionColors& themeSelection =
        ThemeManager::instance().currentTheme().viewport.selection;
    style.faceFillHover = themeSelection.faceFillHover;
    style.faceOutlineHover = themeSelection.faceOutlineHover;
    style.faceFillSelected = themeSelection.faceFillSelected;
    style.faceOutlineSelected = themeSelection.faceOutlineSelected;
    style.edgeHover = themeSelection.edgeHover;
    style.edgeSelected = themeSelection.edgeSelected;
    style.vertexHover = themeSelection.vertexHover;
    style.vertexSelected = themeSelection.vertexSelected;

    auto isSameItem = [](const app::selection::SelectionItem& a,
                         const app::selection::SelectionItem& b) {
        return a.kind == b.kind &&
               a.id.ownerId == b.id.ownerId &&
               a.id.elementId == b.id.elementId;
    };

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    auto drawFace = [&](const app::selection::SelectionItem& item, bool hovered) {
        std::vector<std::array<QVector3D, 3>> triangles;
        if (!m_modelPicker->getFaceTriangles(item.id.ownerId, item.id.elementId, triangles)) {
            return;
        }
        const QColor fill = hovered ? style.faceFillHover : style.faceFillSelected;
        const QColor outline = hovered ? style.faceOutlineHover : style.faceOutlineSelected;

        // Draw filled triangles WITHOUT outline (no internal mesh lines)
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        for (const auto& tri : triangles) {
            QPolygonF poly;
            bool projected = true;
            for (const auto& v : tri) {
                QPointF screenPos;
                if (!projectToScreen(viewProjection, v, static_cast<float>(m_width),
                                     static_cast<float>(m_height), &screenPos)) {
                    projected = false;
                    break;
                }
                poly << screenPos;
            }
            if (projected && poly.size() == 3) {
                painter.drawPolygon(poly);
            }
        }

        // Draw boundary edges only (from OCCT topology, not tessellation)
        std::vector<std::vector<QVector3D>> boundaryEdges;
        if (m_modelPicker->getFaceBoundaryEdges(item.id.ownerId, item.id.elementId, boundaryEdges)) {
            painter.setPen(QPen(outline, hovered ? 1.5 : 2.0));
            painter.setBrush(Qt::NoBrush);
            for (const auto& edgePts : boundaryEdges) {
                QPolygonF line;
                for (const auto& pt : edgePts) {
                    QPointF screenPos;
                    if (projectToScreen(viewProjection, pt, static_cast<float>(m_width),
                                        static_cast<float>(m_height), &screenPos)) {
                        line << screenPos;
                    }
                }
                if (line.size() >= 2) {
                    painter.drawPolyline(line);
                }
            }
        }
    };

    auto drawEdge = [&](const app::selection::SelectionItem& item, bool hovered) {
        std::vector<std::vector<QVector3D>> polylines;
        if (!m_modelPicker->getEdgePolylines(item.id.ownerId, item.id.elementId, polylines) ||
            polylines.empty()) {
            return;
        }
        painter.setPen(QPen(hovered ? style.edgeHover : style.edgeSelected, hovered ? 2.0 : 3.0));
        for (const auto& polyline : polylines) {
            if (polyline.size() < 2) {
                continue;
            }
            QPolygonF line;
            line.reserve(static_cast<int>(polyline.size()));
            for (const auto& point : polyline) {
                QPointF screenPos;
                if (!projectToScreen(viewProjection, point, static_cast<float>(m_width),
                                     static_cast<float>(m_height), &screenPos)) {
                    continue;
                }
                line << screenPos;
            }
            if (line.size() >= 2) {
                painter.drawPolyline(line);
            }
        }
    };

    auto drawVertex = [&](const app::selection::SelectionItem& item, bool hovered) {
        QVector3D vertex;
        if (!m_modelPicker->getVertexPosition(item.id.ownerId, item.id.elementId, vertex)) {
            return;
        }
        QPointF screenPos;
        if (!projectToScreen(viewProjection, vertex, static_cast<float>(m_width),
                             static_cast<float>(m_height), &screenPos)) {
            return;
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(hovered ? style.vertexHover : style.vertexSelected);
        const double radius = hovered ? 4.0 : 5.0;
        painter.drawEllipse(screenPos, radius, radius);
    };

    auto drawBody = [&](const app::selection::SelectionItem& item, bool hovered) {
        std::vector<std::array<QVector3D, 3>> triangles;
        if (!m_modelPicker->getBodyTriangles(item.id.ownerId, triangles)) {
            return;
        }
        const QColor fill = hovered ? style.faceFillHover : style.faceFillSelected;

        // Draw filled triangles WITHOUT outline (no internal mesh lines)
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        for (const auto& tri : triangles) {
            QPolygonF poly;
            bool projected = true;
            for (const auto& v : tri) {
                QPointF screenPos;
                if (!projectToScreen(viewProjection, v, static_cast<float>(m_width),
                                     static_cast<float>(m_height), &screenPos)) {
                    projected = false;
                    break;
                }
                poly << screenPos;
            }
            if (projected && poly.size() == 3) {
                painter.drawPolygon(poly);
            }
        }
        // Note: Body selection doesn't draw boundary edges - just fill
    };

    for (const auto& item : selection) {
        if (item.kind == app::selection::SelectionKind::Face) {
            drawFace(item, false);
        } else if (item.kind == app::selection::SelectionKind::Edge) {
            drawEdge(item, false);
        } else if (item.kind == app::selection::SelectionKind::Vertex) {
            drawVertex(item, false);
        } else if (item.kind == app::selection::SelectionKind::Body) {
            drawBody(item, false);
        }
    }

    if (hover.has_value()) {
        bool alreadySelected = std::any_of(selection.begin(), selection.end(),
                                           [&](const app::selection::SelectionItem& item) {
            return isSameItem(item, hover.value());
        });
        if (!alreadySelected) {
            if (hover->kind == app::selection::SelectionKind::Face) {
                drawFace(*hover, true);
            } else if (hover->kind == app::selection::SelectionKind::Edge) {
                drawEdge(*hover, true);
            } else if (hover->kind == app::selection::SelectionKind::Vertex) {
                drawVertex(*hover, true);
            } else if (hover->kind == app::selection::SelectionKind::Body) {
                drawBody(*hover, true);
            }
        }
    }
}

namespace {
struct IndicatorGeometry {
    QPainterPath path;
    QPointF startScreen;
    QPointF endScreen;
    QPolygonF head;
    QPolygonF backHead;
    QPointF labelPos;
    bool visible = false;
};

 IndicatorGeometry calculateIndicatorGeometry(
    const ui::tools::ModelingTool::Indicator& indicator,
    const QMatrix4x4& viewProjection,
    float width,
    float height,
    double pixelScale)
{
    IndicatorGeometry geo;
    if (indicator.direction.lengthSquared() < 1e-6f) {
        return geo;
    }

    QVector3D dir = indicator.direction.normalized();

    const float visualLength = 30.0f; // 30 pixels per side (Compact)

    QPointF originScreen;
    if (!projectToScreen(viewProjection, indicator.origin, width, height, &originScreen)) {
        return geo;
    }

    // Project direction to screen to get 2D orientation
    QVector3D worldEnd = indicator.origin + dir * static_cast<float>(pixelScale * visualLength);
    QPointF endScreen;
    if (!projectToScreen(viewProjection, worldEnd, width, height, &endScreen)) {
        return geo;
    }

    QVector2D screenDir(endScreen - originScreen);
    if (screenDir.lengthSquared() < 1e-4f) {
        return geo;
    }
    screenDir.normalize();
    QVector2D perp(-screenDir.y(), screenDir.x());

    // Arrow Head parameters (Thick and distinct)
    const float headLength = 16.0f;
    const float headWidth = 10.0f;

    // Calculate endpoints
    geo.endScreen = originScreen + QPointF(screenDir.x() * visualLength, screenDir.y() * visualLength);

    // Front Head
    {
        QPointF headBase = geo.endScreen - QPointF(screenDir.x() * headLength, screenDir.y() * headLength);
        QPointF left = headBase + QPointF(perp.x() * headWidth, perp.y() * headWidth);
        QPointF right = headBase - QPointF(perp.x() * headWidth, perp.y() * headWidth);
        geo.head << geo.endScreen << left << right;
    }

    if (indicator.isDoubleSided) {
        geo.startScreen = originScreen - QPointF(screenDir.x() * visualLength, screenDir.y() * visualLength);

        // Back Head
        QPointF backHeadBase = geo.startScreen + QPointF(screenDir.x() * headLength, screenDir.y() * headLength);
        QPointF backLeft = backHeadBase + QPointF(perp.x() * headWidth, perp.y() * headWidth);
        QPointF backRight = backHeadBase - QPointF(perp.x() * headWidth, perp.y() * headWidth);
        geo.backHead << geo.startScreen << backLeft << backRight;
    } else {
        geo.startScreen = originScreen;
    }

    // Build the hit-test path
    // 1. Line segment
    QPainterPath linePath;
    linePath.moveTo(geo.startScreen);
    linePath.lineTo(geo.endScreen);

    // Use a stroker to make the line thick for hit testing
    QPainterPathStroker stroker;
    stroker.setWidth(60.0); // 60 pixel hit zone (Generous tolerance)
    stroker.setCapStyle(Qt::RoundCap);
    geo.path = stroker.createStroke(linePath);

    // 2. Add arrowheads to path
    geo.path.addPolygon(geo.head);
    if (!geo.backHead.isEmpty()) {
        geo.path.addPolygon(geo.backHead);
    }

    geo.labelPos = geo.endScreen + QPointF(perp.x() * 20.0f, perp.y() * 20.0f);
    geo.visible = true;
    return geo;
}
} // namespace

bool Viewport::isMouseOverIndicator(const QPoint& screenPos) const {
    if (!m_modelingToolManager) return false;
    auto indicator = m_modelingToolManager->activeIndicator();
    if (!indicator.has_value()) return false;

    QMatrix4x4 viewProjection = buildViewProjection();
    IndicatorGeometry geo = calculateIndicatorGeometry(*indicator, viewProjection,
                                                     width(), height(), m_pixelScale);
    if (!geo.visible) return false;

    return geo.path.contains(QPointF(screenPos));
}

void Viewport::drawModelToolOverlay(const QMatrix4x4& viewProjection) {
    if (m_inSketchMode || !m_modelingToolManager) {
        return;
    }

    auto indicator = m_modelingToolManager->activeIndicator();
    if (!indicator.has_value()) {
        return;
    }

    IndicatorGeometry geo = calculateIndicatorGeometry(*indicator, viewProjection,
                                                     width(), height(), m_pixelScale);
    if (!geo.visible) {
        return;
    }

    const ThemeViewportOverlayColors& overlay =
        ThemeManager::instance().currentTheme().viewport.overlay;
    QColor color = overlay.toolIndicator; // Normal state

    const bool isDragging = m_modelingToolManager->isDragging();

    if (isDragging) {
        // Dragging state: High contrast (White)
        color = Qt::white;
    } else if (m_indicatorHovered) {
        // Hover state: Brighter/Lighter
        color = color.lighter(130);
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw White Border (Thickest)
    QPen borderPen(Qt::white, 8.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(borderPen);
    painter.setBrush(Qt::white);

    painter.drawLine(geo.startScreen, geo.endScreen);
    painter.drawPolygon(geo.head);
    if (!geo.backHead.isEmpty()) {
        painter.drawPolygon(geo.backHead);
    }

    // Draw Inner Color (Thick)
    QPen innerPen(color, 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(innerPen);
    painter.setBrush(color);

    painter.drawLine(geo.startScreen, geo.endScreen);
    painter.drawPolygon(geo.head);
    if (!geo.backHead.isEmpty()) {
        painter.drawPolygon(geo.backHead);
    }

    if (indicator->showDistance) {
        const double distanceValue = std::abs(indicator->distance);
        QString text = QString::number(distanceValue, 'f', 2);

        QFont font = painter.font();
        font.setPointSize(10);
        font.setBold(true);
        painter.setFont(font);

        QColor textColor = overlay.toolLabelText;
        QColor bgColor = overlay.toolLabelBackground;

        QFontMetrics metrics(font);
        const int padding = 4;
        QRectF labelRect(geo.labelPos.x(), geo.labelPos.y(),
                         metrics.horizontalAdvance(text) + padding * 2,
                         metrics.height() + padding * 2);

        painter.setPen(Qt::NoPen);
        painter.setBrush(bgColor);
        painter.drawRoundedRect(labelRect, 4.0, 4.0);

        painter.setPen(textColor);
        painter.drawText(labelRect, Qt::AlignCenter, text);
    }
}

void Viewport::drawPlaneSelectionOverlay(const QMatrix4x4& viewProjection) {
    if (!m_planeSelectionActive) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QFont labelFont = painter.font();
    labelFont.setBold(true);
    labelFont.setPointSize(labelFont.pointSize() + 1);
    painter.setFont(labelFont);

    const ThemeViewportPlaneColors& planeColors =
        ThemeManager::instance().currentTheme().viewport.planes;
    const QColor textColor = planeColors.labelText;
    const auto selections = planeSelections(planeColors);
    for (int i = 0; i < static_cast<int>(selections.size()); ++i) {
        const auto& selection = selections[i];
        PlaneAxes axes = buildPlaneAxes(selection.plane);
        QVector3D origin(selection.plane.origin.x, selection.plane.origin.y, selection.plane.origin.z);

        QVector<QVector3D> corners;
        corners.reserve(4);
        corners.append(origin + axes.xAxis * -kPlaneSelectHalf + axes.yAxis * -kPlaneSelectHalf);
        corners.append(origin + axes.xAxis * kPlaneSelectHalf + axes.yAxis * -kPlaneSelectHalf);
        corners.append(origin + axes.xAxis * kPlaneSelectHalf + axes.yAxis * kPlaneSelectHalf);
        corners.append(origin + axes.xAxis * -kPlaneSelectHalf + axes.yAxis * kPlaneSelectHalf);

        QPolygonF polygon;
        bool projectedAll = true;
        for (const auto& corner : corners) {
            QPointF screenPos;
            if (!projectToScreen(viewProjection, corner, static_cast<float>(m_width),
                                 static_cast<float>(m_height), &screenPos)) {
                projectedAll = false;
                break;
            }
            polygon << screenPos;
        }

        if (!projectedAll) {
            continue;
        }

        QColor fillColor = selection.color;
        QColor outlineColor = selection.color;
        outlineColor.setAlpha(200);

        bool hovered = (i == m_planeHoverIndex);
        if (hovered) {
            fillColor = fillColor.lighter(130);
            fillColor.setAlpha(140);
            outlineColor = outlineColor.lighter(150);
        }

        painter.setPen(QPen(outlineColor, hovered ? 2.5 : 1.5));
        painter.setBrush(fillColor);
        painter.drawPolygon(polygon);

        QPointF center;
        if (projectToScreen(viewProjection, origin, static_cast<float>(m_width),
                            static_cast<float>(m_height), &center)) {
            painter.setPen(textColor);
            QRectF labelRect(center.x() - 18, center.y() - 10, 36, 20);
            painter.drawText(labelRect, Qt::AlignCenter, selection.label);
        }
    }
}

void Viewport::setModelPickMeshes(std::vector<selection::ModelPickerAdapter::Mesh>&& meshes) {
    if (m_modelPicker) {
        m_modelPicker->setMeshes(std::move(meshes));
    }
}

void Viewport::setModelPreviewMeshes(const std::vector<render::SceneMeshStore::Mesh>& meshes) {
    if (m_bodyRenderer) {
        m_bodyRenderer->setPreviewMeshes(meshes);
        update();
    }
}

void Viewport::clearModelPreviewMeshes() {
    if (m_bodyRenderer) {
        m_bodyRenderer->clearPreview();
        update();
    }
    clearPreviewHiddenBody();
}

void Viewport::setPreviewHiddenBody(const std::string& bodyId) {
    if (m_previewHiddenBodyId == bodyId) {
        return;
    }
    m_previewHiddenBodyId = bodyId;
    syncModelMeshes();
    update();
}

void Viewport::clearPreviewHiddenBody() {
    if (m_previewHiddenBodyId.empty()) {
        return;
    }
    m_previewHiddenBodyId.clear();
    syncModelMeshes();
    update();
}

void Viewport::syncModelMeshes() {
    if (!m_document || !m_modelPicker) {
        return;
    }
    const auto& store = m_document->meshStore();

    // Build filtered list of visible body meshes
    std::vector<render::SceneMeshStore::Mesh> visibleMeshes;
    store.forEachMesh([&](const render::SceneMeshStore::Mesh& mesh) {
        if (m_document->isBodyVisible(mesh.bodyId) &&
            (m_previewHiddenBodyId.empty() || mesh.bodyId != m_previewHiddenBodyId)) {
            visibleMeshes.push_back(mesh);
        }
    });

    if (m_bodyRenderer) {
        m_bodyRenderer->setMeshes(visibleMeshes);
    }

    // Build pick meshes from visible bodies only
    std::vector<selection::ModelPickerAdapter::Mesh> pickMeshes;
    for (const auto& mesh : visibleMeshes) {
        selection::ModelPickerAdapter::Mesh pickMesh;
        pickMesh.bodyId = mesh.bodyId;
        pickMesh.vertices.reserve(mesh.vertices.size());
        for (const auto& v : mesh.vertices) {
            QVector4D transformed = mesh.modelMatrix * QVector4D(v, 1.0f);
            pickMesh.vertices.emplace_back(transformed.x(), transformed.y(), transformed.z());
        }
        pickMesh.triangles.reserve(mesh.triangles.size());
        for (const auto& tri : mesh.triangles) {
            selection::ModelPickerAdapter::Triangle pickTri;
            pickTri.i0 = tri.i0;
            pickTri.i1 = tri.i1;
            pickTri.i2 = tri.i2;
            pickTri.faceId = tri.faceId;
            pickMesh.triangles.push_back(pickTri);
        }
        for (const auto& [faceId, topo] : mesh.topologyByFace) {
            selection::ModelPickerAdapter::FaceTopology faceTopo;
            for (const auto& edge : topo.edges) {
                selection::ModelPickerAdapter::EdgePolyline edgeLine;
                edgeLine.edgeId = edge.edgeId;
                edgeLine.points.reserve(edge.points.size());
                for (const auto& pt : edge.points) {
                    QVector4D transformed = mesh.modelMatrix * QVector4D(pt, 1.0f);
                    edgeLine.points.emplace_back(transformed.x(), transformed.y(), transformed.z());
                }
                faceTopo.edges.push_back(std::move(edgeLine));
            }
            for (const auto& vertex : topo.vertices) {
                selection::ModelPickerAdapter::VertexSample sample;
                sample.vertexId = vertex.vertexId;
                QVector4D transformed = mesh.modelMatrix * QVector4D(vertex.position, 1.0f);
                sample.position = QVector3D(transformed.x(), transformed.y(), transformed.z());
                faceTopo.vertices.push_back(std::move(sample));
            }
            pickMesh.topologyByFace[faceId] = std::move(faceTopo);
        }
        const TopoDS_Shape* bodyShape = m_document->getBodyShape(mesh.bodyId);
        if (bodyShape && !bodyShape->IsNull()) {
            const auto promotedTopology = core::modeling::SelectionTopologyResolver::resolve(
                *bodyShape, m_document->elementMap());
            pickMesh.faceGroupByFaceId = promotedTopology.faceLeaderByFaceId;
            pickMesh.edgeGroupByEdgeId = promotedTopology.edgeLeaderByEdgeId;
            pickMesh.suppressedVertexIds = promotedTopology.suppressedVertexIds;
        }
        pickMeshes.push_back(std::move(pickMesh));
    }
    setModelPickMeshes(std::move(pickMeshes));
}

} // namespace ui
} // namespace onecad
