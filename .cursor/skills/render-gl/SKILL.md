---
name: render-gl
description: OpenGL 4.1 Core rendering patterns. Use when modifying viewport, sketch renderer, or adding visual elements.
license: MIT
metadata:
  category: rendering
  priority: medium
---

## When to Use

- Adding new rendered elements
- Modifying Camera3D/Grid3D
- Shader development
- Debugging visual glitches

## Architecture

- Camera: `src/render/Camera3D.h/cpp`
- Grid: `src/render/Grid3D.h/cpp`
- SketchRenderer: `src/core/sketch/SketchRenderer.h/cpp` (1903 LOC)
- BodyRenderer: `src/render/BodyRenderer.h/cpp` (727 LOC)
- Viewport: `src/ui/viewport/Viewport.h/cpp` (3146 LOC)

## OpenGL 4.1 Core Profile

OneCAD uses OpenGL 4.1 Core (no fixed-function pipeline).

## Critical Patterns

### GL State Bind/Unbind Pairs
```cpp
// ALWAYS unbind what you bind
glBindVertexArray(vao);
// ... draw calls ...
glBindVertexArray(0);

glBindBuffer(GL_ARRAY_BUFFER, vbo);
// ... operations ...
glBindBuffer(GL_ARRAY_BUFFER, 0);

program->bind();
// ... draw ...
program->release();
```

### Shader Program Pattern
```cpp
QOpenGLShaderProgram program;
program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSrc);
program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSrc);
program.link();
```

### VAO/VBO Setup
```cpp
GLuint vao, vbo;
glGenVertexArrays(1, &vao);
glGenBuffers(1, &vbo);

glBindVertexArray(vao);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);

// Position attribute
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
glEnableVertexAttribArray(0);

glBindVertexArray(0);
```

## Camera3D

```cpp
// Orbit/pan/zoom
camera.orbit(deltaX, deltaY);
camera.pan(deltaX, deltaY);
camera.zoom(delta);

// View/projection matrices
QMatrix4x4 view = camera.viewMatrix();
QMatrix4x4 proj = camera.projectionMatrix();

// Ortho ↔ Perspective (scale-preserving)
camera.setOrthographic(true);
```

## Grid3D

- Fixed 10mm grid spacing
- Shader-based rendering
- Adapts to zoom level

## SketchRenderer Preview Methods

```cpp
void setPreviewLine(const Vec2d& start, const Vec2d& end);
void setPreviewCircle(const Vec2d& center, double radius);
void setPreviewArc(const Vec2d& center, double radius,
                   double startAngle, double endAngle);
void clearPreview();

void showSnapIndicator(const Vec2d& pos, SnapType type);
void setGhostConstraints(const std::vector<InferredConstraint>& ghosts);
```

## Common Issues

1. **State leaks**: Always unbind resources
2. **Missing makeCurrent()**: Call before GL operations
3. **Thread safety**: GL context is single-threaded
4. **Depth testing**: Enable/disable appropriately
5. **Blending for transparency**: GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
