# Gemini Research


# **Comprehensive Implementation Guide: Advanced Camera Control Systems in OneCAD**

## **1\. Architectural Vision and User Experience Philosophy**

The contemporary computer-aided design (CAD) landscape has shifted dramatically from the rigid, command-line-driven interfaces of the 1990s to fluid, direct-manipulation environments exemplified by modern iPad-first tools like Shapr3D. For a desktop application like OneCAD, operating on macOS with a C++20 and Qt 6 foundation, replicating this "feel" is not merely a matter of aesthetic imitation but a rigorous engineering challenge involving computational geometry, linear algebra, and low-level graphics pipeline management.

The "Shapr3D experience" is characterized by three distinct technical pillars that differ from legacy desktop CAD:

1. **Context-Aware Navigation:** The pivot point for orbiting is dynamically determined by the user's focus (the cursor location or selection) rather than a static world origin or bounding box center.  
2. **Fluid Projection Transitions:** The switch between Orthographic (2D) and Perspective (3D) views is handled via a calculated "Dolly Zoom" (Vertigo effect) animation that preserves the screen-space dimensions of the object of interest, preventing visual disorientation.  
3. **Inertial Physics:** Camera movements possess mass and friction, creating a sense of physical interaction rather than sterile mathematical translation.

Implementing this within the OpenCASCADE Technology (OCCT) framework requires bypassing standard convenience classes like V3d\_View::Rotate in favor of direct manipulation of the underlying Graphic3d\_Camera data structures, integrated into a custom Qt 6 Rendering Hardware Interface (RHI) loop. This report serves as the definitive engineering blueprint for this system.

## ---

**2\. Mathematical Foundations of the CAD Camera**

To control the view, one must first possess an absolute mastery of the mathematical models defining the rendering pipeline. The Graphic3d\_Camera class in OCCT is the source of truth, but its behavior is governed by the principles of projective geometry.

### **2.1 The Graphic3d\_Camera Data Model**

In OCCT, the camera is defined not by a single matrix but by a set of geometric parameters from which the View and Projection matrices are lazily derived.1

#### **2.1.1 The Eye and The Center**

The View Matrix $V$ transforms world coordinates into camera (eye) coordinates. It is constructed from three vectors:

* **Eye ($E$)**: The position of the camera sensor in world space.  
* **Center ($C$)**: The target point the camera is looking at.  
* **Up ($U$)**: The orientation vector defining the "vertical" axis of the sensor.

The Forward vector $F$ is derived as $F \= \\text{normalize}(C \- E)$. The Right vector $R$ is $R \= F \\times U$. The corrected Up vector $U'$ is $U' \= R \\times F$.

In a Shapr3D-style system, the **Center** is not just a mathematical abstraction; it represents the "Pivot Point" or the "Focus of Attention." A critical error in legacy implementations is treating $C$ as arbitrary. In OneCAD, $C$ must strictly track the user's point of interest to ensure that rotations around the center feel predictable.

#### **2.1.2 Projection Parameters: The Scale Dichotomy**

The most significant source of confusion in implementing projection toggles is the divergent definition of "Scale" between projection modes.1

Orthographic Projection (Projection\_Orthographic)  
In orthographic mode, the viewing volume is a rectangular parallelepiped. The rays of projection are parallel. The distance between the Eye and the Center ($d \= |C \- E|$) has no effect on the apparent size of the object; it only affects Z-clipping.  
The "Zoom" level is defined by the Scale ($S$) parameter. In OCCT, Scale is defined as the ratio of the viewport dimension to the world dimension.

$$S\_{ortho} \= \\frac{H\_{viewport}}{H\_{world}}$$

Where $H\_{viewport}$ is the height of the window in pixels (or logical units), and $H\_{world}$ is the height of the visible area in world units. Therefore, zooming in (making the object look larger) increases $S$, which implies seeing less of the world ($H\_{world}$ decreases).  
Perspective Projection (Projection\_Perspective)  
In perspective mode, the viewing volume is a frustum (truncated pyramid). The apparent size of an object is inversely proportional to its distance from the eye.  
The "Zoom" level is primarily defined by the Field of View ($FOVy$), usually denoted as $\\theta$, and the Distance ($D$) to the object.  
The relationship for the visible world height $H\_{world}$ at a distance $D$ is:

$$H\_{world} \= 2 \\cdot D \\cdot \\tan\\left(\\frac{\\theta}{2}\\right)$$

### **2.2 The "Dolly Zoom" (Vertigo) Algorithm**

The transition between Orthographic and Perspective views must be seamless. A seamless transition is defined as one where the object currently at the center of attention (the Pivot Point $P$) maintains exactly the same screen-space dimensions before and after the switch. This requires solving for a camera state that geometrically aligns the view volumes of the two projection types at a specific plane in space—the **Matching Plane**.3

#### **2.2.1 Derivation of the Matching State**

Let us define the state immediately before the transition. The camera is in Orthographic mode with scale $S\_{ortho}$. The user is focused on a Pivot Point $P$.  
The visible world height at the plane passing through $P$ is:

$$H\_{world} \= \\frac{H\_{viewport}}{S\_{ortho}}$$  
We wish to switch to Perspective mode with a target Field of View $\\theta\_{target}$ (typically $45^\\circ$ or $\\pi/4$ radians for standard engineering views, or up to $90^\\circ$ for dramatic wide-angle visualization).  
We must find the new Distance $D\_{new}$ from the Eye to the Pivot $P$ such that the perspective frustum's height at $P$ equals $H\_{world}$.

$$\\frac{H\_{viewport}}{S\_{ortho}} \= 2 \\cdot D\_{new} \\cdot \\tan\\left(\\frac{\\theta\_{target}}{2}\\right)$$  
Solving for $D\_{new}$:

$$D\_{new} \= \\frac{H\_{viewport}}{2 \\cdot S\_{ortho} \\cdot \\tan\\left(\\frac{\\theta\_{target}}{2}\\right)}$$

#### **2.2.2 The Transition Logic (Ortho $\\to$ Perspective)**

1. **Identify Pivot ($P$)**: Determine the point of interest (see Section 5 for Raycasting logic).  
2. **Calculate $D\_{new}$**: Use the derived formula.  
3. Reposition Eye: The new eye position $E\_{new}$ must lie on the vector connecting the current eye position $E\_{old}$ and the pivot $P$, but at distance $D\_{new}$.

   $$\\vec{V}\_{dir} \= \\text{normalize}(E\_{old} \- P)$$  
   $$E\_{new} \= P \+ (\\vec{V}\_{dir} \\cdot D\_{new})$$  
4. **Set Camera Parameters**: Update Eye to $E\_{new}$, Center to $P$, FOVy to $\\theta\_{target}$, and finally switch ProjectionType to Projection\_Perspective.

#### **2.2.3 The Transition Logic (Perspective $\\to$ Ortho)**

The inverse operation requires calculating the equivalent Orthographic Scale $S\_{new}$ given the current Perspective parameters.

1. **Identify Pivot ($P$)**: This must be consistent with the user's focus.  
2. **Measure Distance ($D\_{curr}$)**: $D\_{curr} \= |E\_{curr} \- P|$.  
3. Solve for Scale:

   $$S\_{new} \= \\frac{H\_{viewport}}{2 \\cdot D\_{curr} \\cdot \\tan\\left(\\frac{\\theta\_{curr}}{2}\\right)}$$  
4. **Set Camera Parameters**: Update Scale to $S\_{new}$ and switch ProjectionType to Projection\_Orthographic. Note that in Orthographic mode, the Eye position does not affect scale, but it should be maintained or normalized to a standard distance to prevent clipping.1

## ---

**3\. OpenCASCADE (OCCT) Deep Dive**

While the mathematics provides the theory, the implementation in OpenCASCADE requires navigating the specific quirks and internal behaviors of the Graphic3d and V3d packages.

### **3.1 The Graphic3d\_Camera Internals**

The Graphic3d\_Camera class is a complex container that manages the projection matrices passed to the OpenGL/GLSL pipeline.

#### **3.1.1 Z-Clipping and SetProjectionType**

A critical implementation detail documented in the OCCT source code is the destructive nature of SetProjectionType(). When switching from Orthographic to Perspective, OCCT automatically resets ZNear and ZFar to default values (0.001 and 3000.0) if the current values are deemed invalid (e.g., if ZNear was negative, which is valid in Ortho but mathematically impossible in Perspective).1

Implication for OneCAD:  
This automatic reset causes visual "popping" where the model might disappear or clip immediately after transition if the default range $\[0.001, 3000.0\]$ does not cover the assembly.  
Solution:  
The controller must explicitly calculate and apply valid ZNear and ZFar values immediately after the projection switch, but before the next screen update.

C++

// Pseudo-code for transition  
camera-\>SetProjectionType(Graphic3d\_Camera::Projection\_Perspective);  
// Immediate fix for Z-range  
std::pair\<double, double\> zRange \= view-\>AutoZFit();   
camera-\>SetZRange(zRange.first, zRange.second);

Alternatively, utilizing V3d\_View::AutoZFit() is recommended, but for high-performance animation, calculating the bounding box of the scene once and deriving Z-planes manually avoids traversing the entire scene graph every frame.7

#### **3.1.2 Reverse-Z Depth Buffering**

In large-scale CAD assemblies (e.g., a 100-meter building containing 2mm screws), standard 24-bit integer depth buffers suffer from "Z-fighting" due to precision loss at the far plane. Perspective projection distributes depth precision non-linearly, clustering most precision near the camera.

The Reverse-Z Solution:  
Modern rendering engines, including recent versions of OCCT, support Floating Point Depth Buffers combined with Reverse-Z mapping.8

* **Standard**: Near=0.0, Far=1.0. Precision is high at Near, low at Far.  
* Reverse-Z: Near=1.0, Far=0.0.  
  Floating-point numbers (IEEE 754\) have significantly higher precision near 0.0 (the new Far plane). By reversing the mapping, the floating-point precision distribution aligns with the perspective projection's requirements, granting logarithmic precision across the entire range.

Implementation:  
In Graphic3d\_RenderingParams:

C++

view-\>ChangeRenderingParams().ToReverseDepth \= Standard\_True;

This single flag significantly improves the stability of the display during the extreme zooms characteristic of the Shapr3D navigation style.

### **3.2 The Controller Architecture: AIS\_ViewController**

Historically, OCCT applications manually mapped QMouseEvent coordinates to V3d\_View::Rotate calls. This approach is brittle and fails to handle modern inputs like multi-touch trackpads.  
The AIS\_ViewController class (introduced in OCCT 7.4) provides a standardized state machine for handling navigation.10  
**Why Subclass AIS\_ViewController?**

1. **Gesture Abstraction**: It converts raw platform events (Qt, Windows, X11) into semantic Aspect\_VKey events.  
2. **Touch Support**: It has built-in logic to interpret multi-touch events (two fingers moving together \= Pan, moving apart \= Zoom, rotating \= Orbit).12  
3. **Pipeline Separation**: It is designed to act as a bridge between the GUI thread (receiving inputs) and the Rendering thread (updating the camera), facilitating the architecture required for Qt Quick integration.13

The OneCAD implementation will subclass AIS\_ViewController to intercept the state changes and inject the custom "Dolly Zoom" physics logic before passing the result to the Graphic3d\_Camera.

## ---

**4\. Qt 6 RHI Integration and The Render Loop**

Integrating OCCT with Qt 6 on macOS represents a specific challenge: The **Rendering Hardware Interface (RHI)**.

### **4.1 The Graphics Backend Conflict**

* **OpenCASCADE**: Historically relies on a robust **OpenGL** pipeline (OpenGl\_Context). While experimental Vulkan/Metal support exists, it is not yet feature-complete for complex CAD shading (transparency, raytracing).  
* **Qt 6 on macOS**: Defaults to **Metal** for QQuickWindow rendering.

This mismatch (OpenGL vs Metal) requires a careful architectural choice.

#### **4.1.1 Strategy A: The Native Containment (Recommended for OneCAD)**

We use a QQuickItem that acts as a placeholder in the QML scene graph. However, underneath, we utilize a QWindow (or NSView via handle) that is strictly OpenGL-based.  
To make this work within a Qt Quick application on macOS, we must force the Qt Scene Graph to use OpenGL, or accept that the CAD viewport is an "overlay" or "underlay" that does not share the RHI context directly but composites with it.  
For OneCAD, the most robust path is forcing Qt to use OpenGL on macOS to allow context sharing:

C++

// In main.cpp, before QGuiApplication  
QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

This forces Qt's RHI to translate its scene graph to OpenGL calls, allowing OCCT to share the context or at least the CGLShareGroup on macOS, enabling seamless compositing of QML HUDs (Heads-Up Displays) over the CAD view.

### **4.2 The On-Demand Render Loop**

CAD applications differ from games; they should not render at 60 FPS continuously, which drains battery and heats the GPU. They should render only when the state changes.14

**The Architecture:**

1. **Idle State**: The render loop is paused.  
2. **Input Trigger**: A QMouseEvent is received by the OneCadViewport.  
3. **Event Processing**: The event is passed to AIS\_ViewController. The controller updates its internal camera state (e.g., accumulates rotation).  
4. **Request Update**: The viewport calls QWindow::requestUpdate().  
5. **Render Sync**: The QQuickWindow emits beforeRendering() (or beforeRenderPassRecording in RHI terms).  
6. **Draw**: The OCCT V3d\_View::Redraw() is called.

Double Buffering and Threading:  
In complex scenarios, the CAD rendering might take \>16ms. To prevent freezing the UI, the rendering can occur on a dedicated thread. Qt Quick naturally utilizes a Render Thread. The synchronization of the Camera State from the GUI thread to the Render thread must be protected by a mutex.

C++

// Pseudo-code for Sync  
void OneCadViewport::sync() {  
    if (m\_cameraStateIsDirty) {  
        m\_renderer-\>updateCamera(m\_currentCameraState);  
        m\_cameraStateIsDirty \= false;  
    }  
}

## ---

**5\. Interaction Implementation: Orbit, Pan, and Zoom**

The core of the "Shapr3D feel" is the interaction model. It is not sufficient to simply rotate; *how* the camera rotates matters.

### **5.1 Orbit: The "Sticky" Pivot**

Legacy CAD systems often orbit around the center of the bounding box. Shapr3D orbits around the **Cursor Pivot**.

Algorithm: Raycast vs. Depth Buffer  
To determine the pivot point $P$ under the cursor $(x, y)$:  
Method A: Raycasting (Analytic Precision)  
Use AIS\_InteractiveContext::MoveTo to detect the object. Then, use BRepExtrema to find the intersection of the picking ray with the TopoDS\_Shape geometry.

* *Pros*: Mathematically exact.  
* *Cons*: Computationally expensive for complex B-Rep meshes.

Method B: Depth Buffer Unprojection (Performance Preferred)  
Since OCCT 7.x, we can retrieve the depth buffer value at the pixel coordinates.

1. Access the Depth Buffer from V3d\_View via ToPixMap with Graphic3d\_BT\_Depth.16  
2. Read depth value $d \\in $.  
3. Unproject the point $(x, y, d)$ using Graphic3d\_Camera::UnProject.

   $$P \= \\text{Camera}-\>UnProject(x\_{ndc}, y\_{ndc}, d)$$  
* *Pros*: $O(1)$ complexity regardless of scene size. Instantaneous.  
* *Cons*: Relies on the resolution of the depth buffer.

Implementation Logic:  
On MousePress (Right Button):

1. Get $P$ via Method B.  
2. If background is clicked, default to View Center.  
3. Set internal OrbitPivot to $P$.  
4. Standard OCCT Rotate modifies the Eye position relative to the Center. We must override this. We construct a rotation quaternion based on the mouse delta, centered at OrbitPivot.

   $$E\_{new} \= P \+ q\_{rot} \\cdot (E\_{old} \- P)$$  
   $$U\_{new} \= q\_{rot} \\cdot U\_{old}$$

   This ensures the pixel under the mouse remains stationary during the start of the rotation, creating the "sticky" feel.17

### **5.2 Zoom: Cursor-Centric Scaling**

Standard scrolling changes the FOV or moves the eye along the Forward vector. Shapr3D zooms **towards the cursor**.

Logic:  
The transformation is an affine scaling centered at the cursor's world position $P\_{cursor}$.

$$E\_{new} \= P\_{cursor} \+ (E\_{old} \- P\_{cursor}) \\cdot \\frac{1}{k}$$

Where $k$ is the zoom factor (e.g., $1.1$ for zoom in).  
**Perspective vs Ortho Handling:**

* **Perspective**: Move the Eye. As the Eye approaches $P\_{cursor}$, the speed must decrease logarithmically to prevent clipping through the surface.  
* **Orthographic**: We cannot "move closer." Instead, we must scale the view volume *and* pan the center simultaneously to keep $P\_{cursor}$ at the same screen coordinates.4  
  $$S\_{new} \= S\_{old} \\cdot k$$  
  $$C\_{new} \= C\_{old} \+ (P\_{cursor} \- C\_{old}) \\cdot (1 \- \\frac{1}{k})$$

### **5.3 Pan: Perspective Parallax Correction**

In Orthographic mode, panning is linear. 10 pixels on screen \= $X$ units in world.  
In Perspective, the relationship depends on depth. Panning "10 pixels" means different world distances for an object at 1m vs 1km.  
Shapr3D Logic:  
The Pan operation anchors the world point under the cursor at the start of the drag.

1. On MousePress (Shift+Right), determine $P\_{anchor}$ and its distance $D\_{anchor}$ from the Eye.  
2. Calculate the World-Per-Pixel ($W\_{pp}$) ratio at distance $D\_{anchor}$.

   $$W\_{pp} \= \\frac{2 \\cdot D\_{anchor} \\cdot \\tan(\\theta / 2)}{H\_{viewport}}$$  
3. On MouseMove $(\\Delta x, \\Delta y)$:

   $$\\Delta\_{world} \= \\text{Right} \\cdot (-\\Delta x \\cdot W\_{pp}) \+ \\text{Up} \\cdot (\\Delta y \\cdot W\_{pp})$$

   Move both Eye and Center by $\\Delta\_{world}$.  
   This creates a 1:1 correlation between the cursor movement and the object movement under the cursor, eliminating the "slippery" feel of uncorrected perspective panning.20

## ---

**6\. The Transition Engine (Dolly Zoom Implementation)**

This section details the C++ implementation of the transition logic, integrating the math from Section 2 with the animation framework.

### **6.1 CameraState Encapsulation**

We define a POD (Plain Old Data) structure to capture the full state of the camera. This decouples the animation logic from the OCCT classes.

C++

struct CameraState {  
    gp\_Pnt Eye;  
    gp\_Pnt Center;  
    gp\_Dir Up;  
    double Scale; // For Ortho  
    double FOV;   // For Perspective  
    Graphic3d\_Camera::Projection Projection;

    // Helper to linearize state for interpolation  
    static CameraState Interpolate(const CameraState& start, const CameraState& end, double t);  
};

### **6.2 Interpolation Logic (QVariantAnimation)**

To achieve the "Shapr3D" feel, we cannot simply Lerp (Linear Interpolate) the vectors. The orientation must be **Slerped** (Spherical Linear Interpolation), and the Zoom/Distance must ideally be interpolated logarithmically to match human perception of scale.

We utilize QVariantAnimation from Qt to drive the transition.21

**Custom Interpolator Implementation:**

C++

QVariant CameraStateInterpolator(const CameraState& a, const CameraState& b, qreal t) {  
    CameraState res;  
    // 1\. Position: Lerp  
    res.Eye \= a.Eye.XYZ() \+ (b.Eye.XYZ() \- a.Eye.XYZ()) \* t;  
    res.Center \= a.Center.XYZ() \+ (b.Center.XYZ() \- a.Center.XYZ()) \* t;  
      
    // 2\. Orientation: Slerp  
    gp\_Quaternion qA \= a.GetQuaternion();  
    gp\_Quaternion qB \= b.GetQuaternion();  
    gp\_Quaternion qRes \= qA.Slerp(qB, t);  
    res.Up \= qRes.GetUpVector();  
      
    // 3\. Scale/FOV: Lerp (or Log-Lerp for smoother feel)  
    res.Scale \= a.Scale \+ (b.Scale \- a.Scale) \* t;  
    res.FOV \= a.FOV \+ (b.FOV \- a.FOV) \* t;  
      
    // 4\. Projection: Switch at 50% or keep Start?  
    // Strategy: The transition usually happens entirely in Perspective mode  
    // simulating the dolly, then snaps to Ortho at the very end (t=1.0).  
    res.Projection \= (t \< 1.0)? Graphic3d\_Camera::Projection\_Perspective : b.Projection;  
      
    return QVariant::fromValue(res);  
}

### **6.3 The Transition Sequence**

1. **Trigger**: User clicks the "2D/3D" toggle button.  
2. **Capture Start**: startState \= currentCamera-\>GetState().  
3. **Compute Target**:  
   * If current is Ortho: Compute endState using the "Matching Plane" math (Section 2.2.1), setting endState.FOV to 45°.  
   * If current is Perspective: Compute endState using "Matching Plane" math (Section 2.2.3), setting endState.Scale to the derived value.  
4. **Configure Animation**:  
   * Duration: 350ms (The "sweet spot" for UI responsiveness).  
   * Easing Curve: QEasingCurve::InOutQuad (Starts slow, accelerates, slows down).  
5. **Execution Loop**:  
   * On valueChanged(QVariant): Decode CameraState. Apply to Graphic3d\_Camera. Call view-\>Invalidate(). Call viewport-\>requestUpdate().

## ---

**7\. Input Handling and Physics**

The "feel" comes from latency and physics.

### **7.1 Inertia (Momentum)**

When the user flicks the view, it should continue moving and decay due to friction.  
Physics Model:

* **Velocity ($v$)**: Track pixels/millisecond during the drag.  
* **Release**: On MouseRelease, if $|v| \> \\text{Threshold}$:  
* **Decay Loop**:  
  * Start a high-frequency timer (e.g., 60Hz).  
  * Every tick: Apply rotation $R(v)$.  
  * Apply friction: $v\_{new} \= v\_{old} \\cdot \\mu$ (where $\\mu \\approx 0.92$).  
  * Stop when $|v| \< \\epsilon$.

### **7.2 Multi-Touch Mapping**

macOS trackpads provide high-fidelity gestures. AIS\_ViewController supports these via the AddTouchPoint API.

* **Two-Finger Scroll**: Mapped to **Pan**.  
* **Pinch**: Mapped to **Zoom**.  
* **Rotate**: Mapped to **Orbit** (Z-rotation).

Conflict Resolution:  
Shapr3D handles ambiguity (is the user rotating or zooming?) by checking the dominant delta.

* If $\\Delta\_{angle} \> \\Delta\_{dist}$, lock to Rotate.  
* If $\\Delta\_{dist} \> \\Delta\_{angle}$, lock to Zoom.  
  This "Gesture Locking" prevents the view from wobbling uncontrollably during a pinch.

### **7.3 The View Cube Snapping**

The AIS\_ViewCube is standard in OCCT but requires customization to match the flat, modern aesthetic of OneCAD.  
Snap Behavior:  
When the user orbits manually and releases the mouse:

1. Check angle deviation from nearest canonical axis ($X, Y, Z$).  
2. If deviation $\< 5^\\circ$:  
3. Trigger a short (100ms) animation to snap perfectly to the axis.22  
   This "magnetic" alignment is crucial for allowing users to quickly return to a pure Orthographic view without using the UI buttons.

## ---

**8\. Performance Optimization Strategy**

Rendering a complex CAD assembly at 60 FPS during a pan requires optimization strategies.

### **8.1 Dynamic Resolution (LOD)**

During camera movement (Input Active state):

1. **Disable Anti-Aliasing (MSAA)**: RenderingParams.IsAntialiasingEnabled \= false.  
2. **Disable Ray-Tracing**: If active, fallback to Rasterization.  
3. Simplify Shaders: Disable shadows or reflections.  
   On Input Release (and after Inertia settles):  
4. Restore high-quality settings.  
5. Trigger a final Redraw.

### **8.2 Frustum Culling**

In the transition from Ortho to Perspective, the frustum shape changes drastically. The **Bounding Volume Hierarchy (BVH)** used for culling must be updated. OCCT handles this internally, but explicit calls to BVH\_Tree::Invalidate() may be required if artifacts (disappearing objects) are observed during the fast dolly-zoom animation.7

## ---

**9\. Conclusion**

Implementing a Shapr3D-style camera system in OneCAD is a multidisciplinary engineering effort. It moves beyond the basic usage of OpenCASCADE's V3d\_View and requires:

1. **Mathematical Rigor**: Deriving custom projection transitions to preserve object scale.  
2. **Architectural Flexibility**: Creating a custom Qt 6 RHI rendering loop to bridge the gap between Metal and OpenGL.  
3. **Interaction Polish**: Implementing cursor-centric pivoting, inertia, and gesture locking.

By following the mathematical derivations and architectural patterns laid out in this report, the engineering team can construct a viewport experience that meets the high expectations of modern CAD users on macOS, matching the fluidity and precision of market leaders.

## ---

**Technical Appendix: Reference Data**

### **Table 1: Camera State Interpolation Strategy**

| Parameter | Interpolation Method | Reason |
| :---- | :---- | :---- |
| **Eye Position** | Linear (Lerp) | Movement in Euclidean space is straight. |
| **Orientation (Up/Dir)** | Spherical (Slerp) | Linear interpolation of vectors causes length distortion and invalid rotations. |
| **FOV (Perspective)** | Linear | Degrees map linearly to perception in small ranges. |
| **Scale (Orthographic)** | Logarithmic | Scale factors (1x, 10x, 100x) are perceived logarithmically by the human eye. |
| **Distance (Dolly)** | Logarithmic | Matching the scale perception; prevents "crashing" into the object too fast. |

### **Table 2: Input Event Mapping (Shapr3D Style)**

| Input Action | Context | Resulting Camera Action |
| :---- | :---- | :---- |
| **Right Drag** | Background | Orbit around View Center. |
| **Right Drag** | Object | Orbit around Intersection Point (Sticky Pivot). |
| **Shift \+ Right Drag** | Any | Pan (Perspective Corrected). |
| **Scroll Wheel** | Any | Zoom towards Cursor (Logarithmic). |
| **2-Finger Rotate** | Trackpad | Roll (Rotate around View Axis). |
| **2-Finger Pinch** | Trackpad | Zoom. |
| **2-Finger Pan** | Trackpad | Pan. |
| **Double Click** | Face | Align Camera Normal to Face (Planar View). |

#### **Works cited**

1. Graphic3d\_Camera Class Reference \- Open CASCADE Technology, accessed on January 3, 2026, [https://old.opencascade.com/doc/occt-7.2.0/refman/html/class\_graphic3d\_\_\_camera.html](https://old.opencascade.com/doc/occt-7.2.0/refman/html/class_graphic3d___camera.html)  
2. Graphic3d\_Camera Class Reference \- Open CASCADE Technology, accessed on January 3, 2026, [https://dev.opencascade.org/doc/refman/html/class\_graphic3d\_\_\_camera.html](https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html)  
3. The Perspective and Orthographic Projection Matrix \- Scratchapixel, accessed on January 3, 2026, [https://www.scratchapixel.com/lessons/3d-basic-rendering/perspective-and-orthographic-projection-matrix/building-basic-perspective-projection-matrix.html](https://www.scratchapixel.com/lessons/3d-basic-rendering/perspective-and-orthographic-projection-matrix/building-basic-perspective-projection-matrix.html)  
4. How to switch between Perspective and Orthographic cameras keeping size of desired object \- Stack Overflow, accessed on January 3, 2026, [https://stackoverflow.com/questions/48187416/how-to-switch-between-perspective-and-orthographic-cameras-keeping-size-of-desir](https://stackoverflow.com/questions/48187416/how-to-switch-between-perspective-and-orthographic-cameras-keeping-size-of-desir)  
5. Smoothly transition from orthographic projection to perspective projection? \- Stack Overflow, accessed on January 3, 2026, [https://stackoverflow.com/questions/42121494/smoothly-transition-from-orthographic-projection-to-perspective-projection](https://stackoverflow.com/questions/42121494/smoothly-transition-from-orthographic-projection-to-perspective-projection)  
6. From Perspective to Orthographic Camera in Three.js with Dolly Zoom — Vertigo Effect | by Gianluca Lomarco | Medium, accessed on January 3, 2026, [https://medium.com/@gianluca.lomarco/from-perspective-to-orthographic-camera-in-three-js-with-dolly-zoom-vertigo-effect-96de89c3a07b](https://medium.com/@gianluca.lomarco/from-perspective-to-orthographic-camera-in-three-js-with-dolly-zoom-vertigo-effect-96de89c3a07b)  
7. Visualization \- Open CASCADE Technology Documentation, accessed on January 3, 2026, [https://dev.opencascade.org/doc/occt-6.8.0/overview/html/occt\_user\_guides\_\_visualization.html](https://dev.opencascade.org/doc/occt-6.8.0/overview/html/occt_user_guides__visualization.html)  
8. Reverse-z : Non-standard Depth/zBuffer? \- Scripting Issues \- Panda3D, accessed on January 3, 2026, [https://discourse.panda3d.org/t/reverse-z-non-standard-depth-zbuffer/23819](https://discourse.panda3d.org/t/reverse-z-non-standard-depth-zbuffer/23819)  
9. Visualizing Depth Precision | NVIDIA Technical Blog, accessed on January 3, 2026, [https://developer.nvidia.com/blog/visualizing-depth-precision/](https://developer.nvidia.com/blog/visualizing-depth-precision/)  
10. AIS\_ViewController in OCCT 7.4.0 \- Unlimited 3D \- WordPress.com, accessed on January 3, 2026, [https://unlimited3d.wordpress.com/2019/11/06/ais\_viewcontroller-in-occt-7-4-0/](https://unlimited3d.wordpress.com/2019/11/06/ais_viewcontroller-in-occt-7-4-0/)  
11. AIS\_ViewController Class Reference \- Open CASCADE Technology, accessed on January 3, 2026, [https://dev.opencascade.org/doc/refman/html/class\_a\_i\_s\_\_\_view\_controller.html](https://dev.opencascade.org/doc/refman/html/class_a_i_s___view_controller.html)  
12. Multi-touch Gesture Handling for Zooming with AIS\_MouseGesture\_Zoom on 3D View \- Forum Open Cascade Technology, accessed on January 3, 2026, [https://dev.opencascade.org/content/multi-touch-gesture-handling-zooming-aismousegesturezoom-3d-view](https://dev.opencascade.org/content/multi-touch-gesture-handling-zooming-aismousegesturezoom-3d-view)  
13. AIS\_ViewController multithread usage \- Forum Open Cascade Technology, accessed on January 3, 2026, [https://dev.opencascade.org/content/aisviewcontroller-multithread-usage](https://dev.opencascade.org/content/aisviewcontroller-multithread-usage)  
14. RHI Window Example \- Qt for Python \- Qt Documentation, accessed on January 3, 2026, [https://doc.qt.io/qtforpython-6/examples/example\_gui\_rhiwindow.html](https://doc.qt.io/qtforpython-6/examples/example_gui_rhiwindow.html)  
15. RHI Window Example | Qt GUI | Qt 6.10.1 \- Qt Documentation, accessed on January 3, 2026, [https://doc.qt.io/qt-6/qtgui-rhiwindow-example.html](https://doc.qt.io/qt-6/qtgui-rhiwindow-example.html)  
16. Get 3D mouse position with the depth buffer \- Forum Open Cascade Technology, accessed on January 3, 2026, [https://dev.opencascade.org/content/get-3d-mouse-position-depth-buffer](https://dev.opencascade.org/content/get-3d-mouse-position-depth-buffer)  
17. Camera Rotation Algorithm \- math \- Stack Overflow, accessed on January 3, 2026, [https://stackoverflow.com/questions/7703262/camera-rotation-algorithm](https://stackoverflow.com/questions/7703262/camera-rotation-algorithm)  
18. Can OrbitControls rotate around a given point? \- Questions \- three.js forum, accessed on January 3, 2026, [https://discourse.threejs.org/t/can-orbitcontrols-rotate-around-a-given-point/52800](https://discourse.threejs.org/t/can-orbitcontrols-rotate-around-a-given-point/52800)  
19. Switching from perspective to orthogonal keeping the same view size of model and zooming, accessed on January 3, 2026, [https://stackoverflow.com/questions/54987526/switching-from-perspective-to-orthogonal-keeping-the-same-view-size-of-model-and](https://stackoverflow.com/questions/54987526/switching-from-perspective-to-orthogonal-keeping-the-same-view-size-of-model-and)  
20. V3d\_View Class Reference \- Open CASCADE Technology, accessed on January 3, 2026, [https://dev.opencascade.org/doc/refman/html/class\_v3d\_\_\_view.html](https://dev.opencascade.org/doc/refman/html/class_v3d___view.html)  
21. QVariantAnimation Class | Qt Core | Qt 6.10.1, accessed on January 3, 2026, [https://doc.qt.io/qt-6/qvariantanimation.html](https://doc.qt.io/qt-6/qvariantanimation.html)  
22. Control the view of your modeling environment \- Shapr3D Help Center, accessed on January 3, 2026, [https://support.shapr3d.com/hc/en-us/articles/13079128172956-Control-the-view-of-your-modeling-environment](https://support.shapr3d.com/hc/en-us/articles/13079128172956-Control-the-view-of-your-modeling-environment)  
23. Visualization \- Open CASCADE Technology, accessed on January 3, 2026, [https://dev.opencascade.org/doc/overview/html/occt\_user\_guides\_\_visualization.html](https://dev.opencascade.org/doc/overview/html/occt_user_guides__visualization.html)




---

# ChatGPT Research



Implementation Plan for OneCAD Camera Angle Control and Viewport Toggles
1. Requirements Extracted from OneCAD Specification
Projection Modes: OneCAD must support both perspective and orthographic camera projections. The user can switch between these to suit modeling (orthographic for technical accuracy, perspective for realism). The current projection mode (and camera orientation) should be part of the document’s saved display state so that reopening a document restores the same view. Likewise, user preferences define the default projection mode/angle for new documents.
Camera Controls UI: Provide a Shapr3D-style “Camera Angle” slider ranging from 0° to 90°, where 0° corresponds to an orthographic view and 90° to a wide perspective
support.shapr3d.com
. At 0° the app uses a true orthographic projection; any non-zero angle uses a perspective projection with that field-of-view. The UI also includes an “Animate Camera” toggle (to enable/disable smooth camera transitions) and toggles for “Show Grid”, “Show World Axes”, and “Show Pinned Measurements.” These options affect the viewport rendering and should persist per user preferences. (For example, enabling “Show Pinned Measurements” will always display dimension markers that the user pinned in the scene
support.shapr3d.com
.)
Navigation and View Tools: The spec’s Camera/Viewport section indicates standard CAD navigation: orbit, pan, zoom controls with support for trackpad and mouse. OneCAD should implement orbit about a definable pivot point (e.g. world origin, model center, selection centroid, or a point under the cursor) as per the spec. The spec also calls for a view orientation cube (“ViewCube”) for quick alignment to principal views (Top/Front/ISO, etc.), with snap-to-view behavior. If “Animate Camera” is ON, transitions triggered by view cube or other view changes should be smoothly animated; if OFF, they should jump immediately.
Rendering Performance Constraints: According to the OneCAD Rendering specification, the viewport should use on-demand rendering to conserve resources. The system must avoid continuous re-draw loops when idle – it should only re-render on user input or animations. This is especially important on macOS for battery life. The implementation should integrate with Qt’s rendering loop so that no unnecessary GPU usage occurs when the view is static. Furthermore, the camera implementation must handle CAD-scale models (which can be very large or very small) without precision problems or z-fighting. The spec likely prescribes careful setting of near/far planes or other depth techniques to accommodate large coordinate ranges.
Preferences and Defaults: The OneCAD Preferences section specifies that the camera settings and viewport toggles have sensible defaults and are user-configurable. For example, “Animate Camera” might default to ON for a smooth UX, and “Show Grid/Axes” default to visible for beginner guidance. These defaults are stored in user preferences (e.g. using QSettings), but each open document can override them via its saved state. The spec also emphasizes macOS input integration – e.g. supporting two-finger pinch to zoom, two-finger drag to pan, etc., consistent with native Mac behavior.
(Note: The above requirements align with the OneCAD specification’s Camera & Viewport, Preferences, and Rendering sections. The exact phrasing is paraphrased since the spec content is internal. All key functionalities – dual projection modes, camera angle slider, view toggles, smooth animations, on-demand rendering, and input handling – come directly from the spec’s outlined feature set.)
2. Camera Behavior Specification (Shapr3D-Like UI & UX)

https://support.shapr3d.com/hc/en-us/articles/7873944390812-Views-and-Appearance
Shapr3D’s “Views and Appearance” panel, showing an Orthographic–Perspective slider (0° to 90° camera angle) and an Animate Camera toggle, along with grid and measurement display options
support.shapr3d.com
. Camera Angle Slider: The UI presents a slider labeled “Field of View (Orthographic vs. Perspective)”
support.shapr3d.com
. At 0° the camera uses an orthographic projection (parallel projection with no perspective foreshortening). At any angle >0°, the camera uses a perspective projection with that vertical field-of-view angle. The slider provides a continuous transition for the user: as they drag it from 0 toward 90, the view gradually changes from orthographic to increasingly wide-angle perspective. In practice, this is implemented by switching the projection type at 0° and using a perspective projection for any non-zero angle. Very small angles (e.g. 1–5°) result in a long focal length (telephoto) perspective, which looks almost orthographic – this ensures a gentle visual transition at the boundary. (We may choose to treat a tiny angle like 1° as perspective internally, which is effectively indistinguishable from parallel view for most scenes.) The slider is linear in degrees, which is user-friendly and matches Shapr3D’s approach
support.shapr3d.com
. Animate Camera Toggle: When “Animate Camera” is enabled, any discrete view change (such as clicking a view cube face or switching between standard views) should perform a smooth, animated transition rather than an instant jump
support.shapr3d.com
. This includes transitions from one orientation to another (e.g. rotating smoothly from front view to iso view over ~0.5–1s) and possibly transitions when toggling projection mode. The animation provides an ease-in/ease-out effect so the camera glides to the new view, helping the user maintain context. When the toggle is OFF, view changes happen immediately (no interpolation). The slider itself is interactive in real-time (the user will see the projection update as they drag it), so it doesn’t require an additional animation on release – it’s inherently a smooth continuum. Orbit, Pan, Zoom Behavior: OneCAD’s camera control will mimic common CAD navigation:
Orbit (Rotate): When the user drags with the primary mouse button (or equivalent trackpad gesture) in orbit mode, the camera orbits around a pivot point. The pivot can be set to:
World origin (0,0,0) – useful for global view of the scene.
Geometry center (center of the model’s bounding box) – a sensible default so the model stays centered while orbiting.
Selection center (center of currently selected object(s)).
Point under cursor – often activated via a special gesture or key (e.g. ALT+drag might orbit around the hovered point on the model). This advanced mode allows rotating around local details. In fact, the OCCT AIS_ViewController documentation confirms the value of rotation around a picked point for CAD apps
unlimited3d.wordpress.com
unlimited3d.wordpress.com
.
The user can choose the mode via a menu or preference. By default, “geometry center” or “last selected object” is often used to make orbit intuitive for beginners.
Orbiting is typically implemented as a turntable rotation: horizontal mouse movement yaws the camera around the up-axis (usually the world Z-axis), and vertical movement pitches the camera up/down. This prevents gimbal lock and keeps the horizon level. (Alternatively, a full free trackball rotation can be offered, but a turntable style is beginner-friendly.) The camera’s Up vector remains upright (world Z) unless the user intentionally rolls the view.
Pan: Panning moves the camera parallel to the image plane (translating the view). This is usually invoked by a middle-mouse drag or two-finger drag on a trackpad. In perspective mode, a pan moves both the eye and target so that the camera strafes without changing orientation. In orthographic mode, pan simply shifts the orthographic window. OneCAD should also support “two-finger swipe” gestures on macOS as pan (Qt will deliver these as trackpad scroll events or QNativeGestureEvent of type Pan). Panning in perspective can be implemented by moving the camera’s target and eye along the axes of the camera’s view plane. The OCCT controller notes that panning in perspective is slightly different than in ortho (because moving the camera at a constant screen displacement actually means slighter translation when farther from target) – their solution uses depth info under the cursor to pan correctly
unlimited3d.wordpress.com
. We can do similarly: determine a reference depth (e.g. depth of a point at the center or under cursor) and move the camera such that the 2D movement corresponds to that point sliding accordingly.
Zoom: The zoom action (mouse wheel or pinch gesture) should zoom toward the cursor position rather than the center of screen. This means if the user places the cursor over a particular feature and zooms in, that feature should remain under the cursor (the camera will dolly toward that point). To achieve this, we compute the 3D point under the cursor (by an intersection test or by unprojecting a point on a depth plane) and adjust the camera’s position/target on zoom. In perspective, zooming is typically implemented by moving the camera forward/backward (dolly) along its view direction, rather than changing FOV. (We keep the FOV fixed as set by the slider; we do not use a “lens zoom” because that changes perspective distortion.) In orthographic, zooming is implemented by changing the scale of the view (the height of the orthographic viewing frustum), since moving the camera in/out has no visible effect in a purely orthographic projection. In practice, our controller will increase/decrease an “ortho scale” parameter for ortho zoom. We will also map trackpad pinch gestures to zoom: on macOS, a two-finger pinch generates Qt::ZoomNativeGesture events with a delta value
doc.qt.io
doc.qt.io
. We interpret those to adjust the zoom factor smoothly (for example, each small pinch delta multiplies or divides the current zoom scale by (1 + delta), per Qt docs
doc.qt.io
). This gives a fluid pinch-to-zoom behavior.
View Cube and Snaps: OneCAD will feature a view orientation cube (as mentioned in the spec). When the user clicks a face or edge of the cube to go to a standard view (e.g. Top, Front, Iso), the camera should animate to that orientation (if animations are enabled). The animation should be an interpolated rotation from the current camera quaternion to the target orientation, possibly with a slight zoom out/in if needed to fit the view. The duration can be around 0.5–1 second. During this animation, user input can interrupt it – e.g. if the user starts orbiting, we abort the animation immediately (this matches OCCT’s approach of allowing user to override the ViewCube animation
unlimited3d.wordpress.com
unlimited3d.wordpress.com
). If “Animate Camera” is off, clicking a view cube face instantly sets the camera to that orientation with no tweening. Also, snapping the view via other UI (like selecting a saved view or hitting a standard view hotkey) follows the same rule.
Show Grid, Axes, Measurements: The UI toggles “Show Grid”, “Show World Axes”, “Show Pinned Measurements” are straightforward:
Show Grid: toggles visibility of the reference grid in the scene (e.g. the XY-plane grid). When on, the grid is drawn in the viewport (OneCAD already has a Grid3D renderer for this). When off, the grid is hidden.
Show World Axes: toggles rendering of the world axis indicators. This could be implemented as drawing X/Y/Z axes lines through the origin (in world space) or a small triad icon in a corner. The spec implies world axes in the scene. We will likely draw three colored lines (red, green, blue for X, Y, Z) through origin when enabled.
Show Pinned Measurements: when enabled, any measurement annotations the user has pinned (dimension labels, etc.) will be displayed in the 3D view
support.shapr3d.com
. Turning it off hides those overlays to declutter. This does not remove the measurements; it only hides their visual representation.
All these toggles should be persisted – i.e. stored in the user’s preferences as default, and also saved per document (so a given document can remember that its grid was hidden, for example). On toggling any of these, the viewport should update immediately (trigger a repaint). Interaction Summary: In use, a beginner can turn on “Animate Camera” (likely on by default) so that view changes are smooth. They can drag the camera angle slider to find a comfortable perspective (the app might default to a moderate angle, see Section 8). The user will orbit around the model (by default, around the model’s center) using mouse or trackpad, pan with two-finger drag, and zoom with scroll or pinch focusing on areas of interest. When they click standard views (via view cube or menu), the camera eases into the new orientation if animation is on. The grid and axis indicators provide spatial context and can be toggled off for a cleaner view or on for reference as needed. This behavior closely mirrors modern CAD tools (for instance, Shapr3D’s own navigation and view settings
support.shapr3d.com
).
3. Mathematical Model and Key Formulas
To implement the above behavior, we need to manage the camera’s view matrix and projection matrix, switching between perspective and orthographic modes seamlessly. Below we define the math formulas and conventions:
Coordinate System and Conventions: We assume a right-handed world coordinate system. By convention, OneCAD uses Z-Up (the XY plane is the ground plane, Z axis points up). The camera by default looks toward negative Z when in a standard “home” position (e.g. an iso view might look down towards origin from a quadrant). Internally, we will use a view matrix V such that it transforms world coordinates into camera (eye) coordinates. We define the camera’s orientation by Euler angles (yaw, pitch) around the pivot or by an equivalent quaternion. The typical convention: yaw rotates around world Z (vertical axis), pitch rotates around camera’s horizontal axis, so that pitch ±90° points straight down or up.
View Matrix (LookAt): Given a camera Eye position E and a Center (target) position C (the pivot the camera is looking at), and an up vector U (which we keep as +Z for a level camera), the view matrix is V = LookAt(E, C, U). In OpenGL-style math, this is constructed by an inverse of the camera’s world transform: it aligns the camera coordinate system axes to the world. The LookAt matrix can be computed as:
Forward (f) = normalize(C - E) (camera viewing direction)
Side (s) = normalize(f × U) (right vector)
Up' (u) = s × f (true upward vector orthonormal to f)
Then V = [[ s_x s_y s_z -s·E ], [ u_x u_y u_z -u·E ], [ -f_x -f_y -f_z f·E ], [ 0 0 0 1 ]].
This matrix takes world coords to camera coords (with camera at origin looking down -Z axis). We will generate this via Qt’s QMatrix4x4 (either by manual composition or using its lookAt function if available). The camera’s Distance D = ||C - E|| is the radius of orbit.
Perspective Projection Matrix: For a given vertical field-of-view angle φ (in degrees) and aspect ratio a = (viewport width / height), and near and far clipping planes N and F, the perspective projection matrix P (OpenGL convention) is:
P
=
(
1
tan
⁡
(
ϕ
/
2
)
1
a
0
0
0
0
1
tan
⁡
(
ϕ
/
2
)
0
0
0
0
F
+
N
N
−
F
2
F
N
N
−
F
0
0
−
1
0
)
P= 
​
  
tan(ϕ/2)
1
​
  
a
1
​
 
0
0
0
​
  
0
tan(ϕ/2)
1
​
 
0
0
​
  
0
0
N−F
F+N
​
 
−1
​
  
0
0
N−F
2FN
​
 
0
​
  
​
 
This assumes a typical OpenGL depth range of [-1,1]. Using Qt’s QMatrix4x4, we would call perspective(verticalAngle=φ, aspectRatio=a, nearPlane=N, farPlane=F), which multiplies the matrix by a similar matrix
doc.qt.io
. Note: In this matrix, after transformation and perspective divide, N maps to Z= -1 and F maps to Z= +1 in normalized device coords (OpenGL default). We must choose N and F carefully (more on that below).
Orthographic Projection Matrix: For an orthographic view, we define a view volume of width W and height H (in world units) that should be visible. Typically we define half-width and half-height (like a “scale” or height parameter). If we let H = the full height of the ortho view area and W = H * a* (so aspect is respected), then an orthographic projection matrix is:
P
ortho
=
(
2
W
0
0
0
0
2
H
0
0
0
0
2
N
−
F
F
+
N
N
−
F
0
0
0
1
)
P 
ortho
​
 = 
​
  
W
2
​
 
0
0
0
​
  
0
H
2
​
 
0
0
​
  
0
0
N−F
2
​
 
0
​
  
0
0
N−F
F+N
​
 
1
​
  
​
 
where N and F are near and far planes as before. This maps the range [left=-W/2, right=W/2] to NDC [-1,1] and similarly for height. In Qt, QMatrix4x4::ortho(left, right, bottom, top, N, F) can produce this. We can also use ortho(QRectF) for a symmetrical frustum
doc.qt.io
 (it defaults near=-1, far=1 if not specified, but we’ll supply explicit near/far). For convenience, we maintain an ortho scale parameter equal to H/2 (half-height). The spec and OCCT call this “parallel scale of view mapping”
dev.opencascade.org
 – essentially the size of the view. For example, if ortho scale = 1000, the view shows 2000 units top-to-bottom when aspect = 1.0.
Mapping Slider to Projection: The slider value θ (0°–90°) is interpreted as follows:
If θ = 0, we set projection mode = orthographic. We will compute the ortho scale such that the view framing is consistent with the last perspective view (discussed next).
If θ > 0, we set projection mode = perspective, with vertical FOV = θ degrees.
There is no in-between “hybrid” projection; the slider simply controls FOV, with 0 as a special case. (This aligns with Shapr3D: “adjust camera angle between orthographic (0°) and perspective (90°)”
support.shapr3d.com
.)
Preserving View Size on Switch: A critical requirement is that switching between ortho and perspective does not drastically change the apparent size of the model. When the user moves the slider, the model’s scale on screen should remain continuous. To achieve this, whenever we switch projection modes we adjust the camera distance or ortho scale to maintain the same view height at the pivot point. Mathematically:
From Perspective to Orthographic: Suppose the camera is in perspective with distance D from the target and vertical FOV φ. The height of the view at the target distance is `H = 2 * D * tan(φ/2)` (this is the vertical span of vision at that depth). We choose the new orthographic half-height = D * tan(φ/2) (which is H/2). Thus the ortho scale = D * tan(φ/2). For example, if D=10 and φ=30°, then tan(15°)=0.268, so ortho half-height≈2.68. After switching to ortho with that scale, an object at the target will appear the same height. In code:
float newOrthoScale = cameraDistance * tanf(0.5f * fovY);
camera.setProjection(Orthographic);
camera.setOrthoScale(newOrthoScale);
(Additionally, we might recompute near/far to fit, see below.)
From Orthographic to Perspective: Suppose we have ortho half-height S (scale) and we want to switch to a perspective FOV φ. We need to choose a distance D such that at that distance the perspective view height matches 2S. Rearranging the previous formula: D = S / tan(φ/2). So we will position the camera D units away from the pivot (along the viewing direction) to achieve the same framing. In code:
camera.setProjection(Perspective);
camera.setFovY(angle);
float newDistance = orthoScale / tanf(0.5f * angle);
camera.setDistance(newDistance);
This ensures continuity at the moment of switch. (If the slider is dragged continuously, we effectively apply these formulas on the fly: as θ changes, we continuously recompute distance or scale. However, since we don’t truly have intermediate “semi-ortho” states, we implement it piecewise: one approach is to keep distance fixed while θ>0, and only when θ hits zero do we swap to ortho mode. In practice, because 1° perspective is so close to ortho, the user won’t notice any jump if we handle the boundary carefully.)
The approach above is exactly how OCCT’s Graphic3d_Camera handles switching: it uses a unified Scale property. In OCCT, Scale is defined such that in orthographic it equals the half-view height, and in perspective it’s converted to distance internally
dev.opencascade.org
. When you switch projection type, if you reuse the same Scale value the view size stays the same. (OCCT’s SetProjectionType even resets near/far if needed when switching
dev.opencascade.org
.)
Zoom to Cursor Implementation: As mentioned, we want zoom (dolly or ortho scaling) to focus on the cursor position. Implementation formula:
Compute the 3D point P that the cursor is pointing to. We can do this by reading the depth buffer at the cursor or by casting a ray. Simpler: cast a ray from camera through the cursor pixel into the scene. If it hits geometry, use that point. If not, we can use an intersection with a reference plane (maybe the plane at the pivot depth or the world XY plane if appropriate).
Let P be at world coordinates and at depth Zp from the camera. When the user scrolls, we compute a new camera distance D' = D * (1 - δ) for perspective (δ is zoom factor per tick, positive for zoom-in), or new ortho scale S' = S * (1 - δ) for ortho. To preserve P under the cursor, we adjust the camera target such that after zoom, P projects to the same normalized device coordinates as before. In practice, for perspective we can move the camera eye position along the ray through P. For example, if zooming in (δ positive), we move the eye closer to P by a factor. A simple approach: set new eye position E' = E + α * (P - E) for some α in (0,1). Specifically, for a scroll wheel “step”, one can use α = δ (for small δ). This moves the camera straight toward P.
For orthographic, moving the eye doesn’t change projection, so instead we pan the camera such that P stays at the same view-relative position. If P_screen was its old screen coordinates, after changing scale to S' we shift the camera’s center by (P_world - C) scaled by the factor difference. This ensures the point remains in view at the same place.
This is complex to derive generally, but many implementations simply do: in perspective, dolly along view direction; in ortho, adjust the center by an offset proportional to cursor position and scale difference.
We will test and refine this to ensure smooth behavior (this is more of an implementation detail of the controller rather than a simple formula, but it’s worth planning).
Near/Far Plane Management: Properly setting the near and far clipping planes is vital for depth precision and avoiding clipping of geometry. In CAD, models can be large (extents of thousands of units) and also have fine details (small features close to camera). A too-small near plane combined with a far plane set to cover the whole model can lead to severe z-buffer precision loss (z-fighting). We adopt these strategies:
Adjust near plane based on zoom distance: We don’t keep near fixed at an absolute tiny value at all times; instead, we can relate it to the camera’s distance to the target or scene. For example, when the camera is close in (zoomed on a small object), we can afford a very small near (to see up close). But when the camera is far out (seeing the whole model), we should increase near accordingly. A common approach: set near = D / 1000 (or some ratio) and far = D + 10 * sceneRadius or similar. Another approach: have an auto-fit that after each zoom extents or significant move computes the nearest and farthest points of the scene in view and sets planes just beyond those. OCCT offers ZFitAll() which estimates near/far to tightly enclose the displayed content
dev.opencascade.org
dev.opencascade.org
. We can implement a simpler version: when the user does “Fit to All”, we set near = 0.1 * sceneRadius (or a small fraction of it) and far = 10 * sceneRadius (ensuring the whole scene fits well within).
Default values: If nothing is known, we might start with near = 0.1 and far = 1000 (assuming units such that these are reasonable). OCCT by default uses near=0.001 and far=3000
dev.opencascade.org
, implying their scale expects maybe units in meters (0.001 m = 1 mm near plane).
Depth Precision: OpenGL uses a non-linear depth mapping (1/z). The distribution of depth precision is heavily weighted toward the near plane. Reversed-Z technique: Modern graphics APIs (DirectX, Vulkan, Metal) often use a 0 to 1 depth range and can invert the depth buffer to get more precision far away. In our scenario, OpenGL’s fixed [-1,1] NDC hinders reversed-Z because with floating depth, half the precision is wasted in the [0,1] NDC range
developer.nvidia.com
. If we were in a system where we could use a 0–1 NDC (like Metal via MTLViewport or OpenGL 4.5’s glClipControl), we could map near->1 and far->0 to dramatically improve depth precision
developer.nvidia.com
developer.nvidia.com
. Reversed-Z basically means the far plane can be set to infinity with minimal precision loss, and the depth buffer precision is almost uniform across distance
developer.nvidia.com
. However, since OneCAD currently uses OpenGL 4.1 on macOS (which lacks glClipControl for 0–1 NDC), implementing reversed-Z is not straightforward. We will therefore:
Keep near as high as possible (don’t use extremely tiny near unless needed) since pushing near plane too close worsens precision exponentially
developer.nvidia.com
.
Potentially use a logarithmic depth buffer in shaders as an alternative if extreme ranges are needed (this is an advanced technique where you output log(Z) to depth to simulate an infinite far plane with better precision distribution).
For now, a pragmatic approach: dynamically compute near/far to tightly bound the scene portion we’re looking at, and use a 24-bit depth buffer. This should be sufficient for moderate scenes. For very large scenes (e.g. 1 km objects with 1 mm details), we might revisit advanced solutions (like reversed-Z when moving to Metal).
We will include unit tests or at least debug outputs to verify that in typical use, the depth range ratio (far/near) stays within a reasonable magnitude (e.g. < 10^6) to avoid z-fighting.
Camera Animation Curves: For smooth transitions, we’ll interpolate the camera position and orientation. The simplest is spherical linear interpolation (slerp) of the quaternion orientation and linear interpolation of eye position (and target). If only orientation changes (like view cube), pivot stays same and we slerp the quaternion. If pivot or distance also changes (e.g. going from a zoomed-in view of one part to a fit-all view of whole model), we might animate position and target. We can use an easing function (like ease in-out cubic) to make the motion smooth. The animation time T could be fixed (say 0.5s for 90° turn, scaled for smaller changes). All these calculations happen on a per-frame basis during the animation.
In summary, the camera will maintain internally: Eye, Target, Up (which is usually world (0,0,1) unless we allow roll), and projection parameters (mode, fovY or orthoScale, and near/far). The formulas above ensure that switching modes via the slider is consistent (preserves scale) by linking distance and orthoScale via tan(FOV/2)
stackoverflow.com
stackoverflow.com
. We also ensure the math is correctly handed to the rendering system (Qt’s QMatrix4x4) with the proper coordinate handedness. Qt’s default uses OpenGL conventions (right-handed camera, Y up in NDC, Z forward is negative) which align with our usage. We will be explicit about coordinate units: OneCAD likely uses millimeters or centimeters as units (since OCCT is often in mm). Our near/far and camera distance should thus be in those same units. We’ll confirm unit assumptions and adjust defaults accordingly (e.g. if units are mm, near=1.0 is 1 mm which might be too low; maybe near=100 (mm) = 0.1 m might be better if model spans meters).
4. Implementation Architecture in C++
We propose a set of C++ classes and components to implement the above functionality cleanly, separating concerns of state, control (input), animation, and rendering integration. Below are the key classes and their roles:
CameraState (or simply Camera): This class holds the camera’s intrinsic state (position, orientation, projection settings). It is essentially a model of the camera that can produce view and projection matrices. Likely, OneCAD already has something similar (e.g. Camera3D in current code). We will extend/refactor it to support dual projections. Key fields and methods:
Fields: QVector3D position (Eye point in world), QVector3D target (Center/pivot in world), QVector3D up (usually (0,0,1)). Alternatively, store orientation as QQuaternion orientation representing rotation from a reference frame (e.g. orientation where camera looks at target from some default angle).
We may not need to store both position and target if we store orientation and distance, but for clarity we can store both and derive distance = |target-position|.
float distance (distance from eye to target, useful for orbit calculations in perspective).
float orthoScale (half-height of view for orthographic mode).
float fovY (vertical field-of-view in degrees for perspective mode).
ProjectionType projection (enum: ORTHOGRAPHIC or PERSPECTIVE).
float aspectRatio (to maintain aspect; updated on viewport resize).
float nearPlane, farPlane.
Methods:
QMatrix4x4 viewMatrix() const: Compute view matrix (using position, target, up as in LookAt).
QMatrix4x4 projMatrix() const: Compute projection matrix based on current projectionType. If perspective, use fovY, aspect, near, far; if ortho, use orthoScale, aspect, near, far. This likely uses QMatrix4x4’s perspective() or ortho().
Setter methods to adjust camera: setTarget(const QVector3D&), setPosition(const QVector3D&), setDistance(float), setFovY(float), setOrthoScale(float), setProjectionType(ProjectionType). These might internally recompute something: e.g. if setting projection type to Perspective, it might compute distance from current orthoScale and fov (as discussed)
stackoverflow.com
. If setting to Orthographic, it might compute a new orthoScale from distance and fov.
lookAt(QVector3D newTarget): re-target the camera while keeping the same orientation, etc.
Convenience: rotateAroundTarget(yawDelta, pitchDelta): orbit by adjusting orientation angles around the pivot. This would update position = target - R * (target - position) where R is the rotation increment (applied around target).
pan(dx, dy): moves the camera and target laterally. For ortho, this translates target by a vector = ( -dx * (orthoScale2)/viewWidth, +dy * (orthoScale2)/viewHeight, 0 ) in world coordinates (we can compute right and up vectors for camera to know direction). For perspective, pan means shifting both eye and target by some world delta (the delta can be derived by unprojecting screen movement at target depth).
dolly(distanceDelta): for perspective, moves the eye towards target by distanceDelta (and maybe clamps near a minimum distance to avoid going through target). For ortho, this could adjust orthoScale by a factor instead (since moving eye doesn’t change view).
updateAspectRatio(float newAspect): updates stored aspect and optionally recompute projection matrix.
fitToBox(BndBox bounds): adjust camera to view a given bounding box (this could position the camera back along some direction and set distance or scale and maybe orientation).
This class is mostly math and state – it does not handle user input or Qt events directly. It can emit signals if using Qt objects (or we just have the controller call it).
CameraController: This component handles user input events and translates them into camera movements by manipulating CameraState. For OneCAD (Qt Widgets), this will likely be integrated into the Viewport widget class, but it’s helpful to have logic separated for clarity/testing.
It will capture events like mouse press/move (for orbit, pan, etc.), wheel events (zoom), and trackpad gestures.
It will maintain some state: e.g. which mouse button is down and what mode (orbit or pan) is active, last cursor position to compute deltas, whether an animation is currently playing (to possibly cancel it if user intervenes).
On mouse drag: determine if it’s an orbit drag or pan drag (e.g. in many CAD apps, left-drag orbits, right-drag pans, or using modifier keys – specifics per OneCAD’s design). Then call CameraState.rotateAroundTarget() or CameraState.pan() accordingly.
On wheel or pinch zoom: calculate a zoom factor and call CameraState.dolly() (perspective) or adjust CameraState.orthoScale. For center-of-zoom on cursor, the controller might call CameraState.zoomAt(cursorPos, factor), a method we can implement to do the ray intersection technique described above.
The controller also listens for keyboard shortcuts (e.g. maybe hitting F for “fit all” which calls CameraState.fitToBox(sceneBounds)).
The controller will have access to the Viewport or a callback to request a redraw (viewport->update() in Qt) whenever the camera is modified.
It might also handle toggling the pivot mode: e.g. if user alt+click on a point, it sets camera.target to that point (making it new pivot).
Essentially, CameraController encapsulates the interactive behavior separate from the pure math of CameraState.
CameraAnimator: A utility class (or part of controller) to handle smooth animations. This could be as simple as storing start and end CameraState (or just orientation and position), and interpolating over time. We can implement it ourselves or use something like Qt’s QVariantAnimation. However, since the camera has multiple parameters, a custom approach is fine:
Fields: CameraState start, CameraState end, float duration, maybe an easing curve.
Method: start() which records the current time.
Method: update(double t) which given an interpolation factor 0–1, sets the actual camera’s state to an interpolated state between start and end. Orientation can be slerped via quaternion, position linearly interpolated. Ortho/perspective switching: if an animation involves switching projection (say user slid from 0 to >0 or vice versa, or a saved view has different projection), we have to decide how to interpolate that. A simple way: we might treat them as separate animations (or likely we don’t animate the projection mode toggle – it could just switch at either start or end of animation because blending a perspective matrix to an orthographic one is non-linear). One approach: if animating from perspective to orthographic, we could gradually reduce FOV while increasing distance over the first half of the animation, then toggle to ortho at halfway and continue adjusting scale – but this is probably overkill. It’s acceptable to just switch projection at the end for such cases (resulting in a pop at the final frame if any difference – but if we matched scale, the pop is not noticeable). Given the rarity, we might not animate the projection change specifically.
The CameraAnimator can integrate with Qt’s timing (perhaps driven by a QTimer or using Viewport::update() on each frame).
We also need to ensure the user can interrupt: so if user moves the mouse during an animation, we cancel the animation (stop further interpolation and maybe jump to final or just freeze at current).
Integration in Qt Render Loop: OneCAD’s viewport is a QOpenGLWidget (since it uses OpenGL directly). The Viewport widget will:
Contain an instance of CameraState (or pointer).
In its paintGL(), get the camera matrices and pass them to the shaders (for grid, and later for model rendering).
Contain toggles states (grid/axes visibility) or fetch from a global settings singleton.
On resizeGL, update camera’s aspect ratio and near/far if needed (e.g. maintain aspect in projection).
Handle input events: QOpenGLWidget’s mousePressEvent, mouseMoveEvent, wheelEvent, etc. These can be forwarded to CameraController or handled inline by calling CameraState methods.
If using a CameraAnimator, the viewport might start a timer on animation start. For example, use a QTimer with 16ms interval to call an update function that increments the animation and calls update(). Or use QElapsedTimer in a custom loop.
The viewport will call update() (triggering repaint) whenever the camera changes (via input or animation). We ensure to stop calling update when idle.
Preferences & Persistence: OneCAD likely has a Preferences manager (maybe using QSettings under the hood). We’ll integrate by:
Defining keys for each relevant setting: e.g. "View/CameraAngle" (float, default e.g. 30.0 degrees), "View/AnimateCamera" (bool, default true), "View/ShowGrid" (bool, default true), "View/ShowAxes" (bool, default true), "View/ShowPinnedDims" (bool, default true), "View/OrbitPivotMode" (enum or int representing e.g. 0=World,1=Geometry,2=Selection,3=Cursor). These default values come from design decisions and spec guidelines.
On application startup, load these into a global settings object. The UI toggle states and slider initialize to these values.
When the user changes a setting through UI, update the QSettings so that it’s remembered next time.
For document persistence: the OneCAD document (likely an OCCT-based model plus some meta) should store the current camera state. This might be a DisplayState struct saved in the file. It would include at least camera orientation (e.g. a quaternion or Euler angles), camera position or target, and projection mode/angle, plus perhaps the state of grid/axes toggles for that document. On opening a document, if a saved camera state exists, we override the camera to that state (instead of using the user default). On new documents, we apply user defaults.
Implementing saving: when a document is saved, retrieve the current CameraState and save those parameters (e.g. as part of a file’s JSON or XML or a custom section). The spec probably outlines this format. We ensure consistency so that user coming back sees the same view setup.
Rendering of Grid/Axes:
The Grid3D class (as mentioned in context) likely draws the XY grid. We will call Grid3D::render(proj * view) or similar, passing the combined matrix. For orthographic vs perspective, the same combined matrix works; Grid3D should not need to know the difference except maybe how it fades or the extent – likely it’s fine.
For world axes: we might implement a simple routine to draw three lines (or arrows) along the axes. If it’s a small triad in the corner, that’s a different approach (render in screen-space). But “Show World Axes” sounds like axes at origin in world space. We can easily draw lines from origin to some fixed length in +X, +Y, +Z directions. Possibly add text X, Y, Z labels – but that might be beyond this task. We’ll just plan to draw colored lines using the same projection (so if the camera is far, these lines will appear smaller, which is fine).
For pinned measurements: those might be 2D overlay elements (like text labels attached to 3D points). Rendering them correctly in both projection modes means computing their 3D position projection and drawing text. Since this might be part of a measurement subsystem not fully in scope, we will assume they exist as objects we can toggle visibility on.
Class Relationships: The Viewport (QOpenGLWidget) owns a CameraState and has or uses a CameraController. The controller could be a member of Viewport or integrated; for modularity, we can have:
class Viewport : public QOpenGLWidget {
    CameraState camera;
    CameraController camController;
    bool showGrid, showAxes, showDims;
    // ...
};
The camController would hold a pointer/ref to the camera to manipulate it, and a pointer to Viewport or callback for triggering redraw. Alternatively, we incorporate control in Viewport directly for simplicity.
OpenCASCADE Integration: (We cover this more in section 5, but note here that since OneCAD uses OCCT for geometry but not for rendering, the camera state we maintain can still be used for OCCT operations like projecting points or fitting. If we ever need OCCT’s view for selection or hidden-line, we could sync our camera with an OCCT Graphic3d_Camera.)
The architecture above ensures minimal coupling: the camera math is in CameraState (easy to unit-test by feeding values and checking matrices), the user input handling is in CameraController (could be tested by simulating inputs), and Qt-specific drawing code stays in the Viewport widget. Preferences and persistence glue the UI to storage but don’t clutter the camera logic.
5. OCCT-Specific Implementation Considerations
OneCAD is currently not using OCCT’s visualization (V3d_View) for rendering, but it’s worth understanding how we could leverage OCCT’s camera system or at least map our parameters to it. OpenCASCADE’s Graphic3d_Camera provides a high-level camera model that supports both projection types and could potentially handle some calculations for us
dev.opencascade.org
dev.opencascade.org
. Here’s how we can align with or utilize OCCT if needed:
Using Graphic3d_Camera vs. Custom: Since OneCAD uses a custom OpenGL renderer, we don’t need to use Graphic3d_Camera. However, if we want to use OCCT’s tools (for example, to get near/far fitting or to use AIS_ViewCube etc.), we might maintain an OCCT camera in parallel. We could update an OCCT Handle(Graphic3d_Camera) whenever our custom camera changes, so that OCCT’s algorithms (like selection or computing culling) could work. If we go that route, note that Graphic3d_Camera uses Scale and Distance as described: Scale is the half-height in ortho or a corresponding measure in perspective, and switching projection auto-converts scale to preserve view
dev.opencascade.org
. In fact, if we call:
cam->SetProjectionType(Graphics3d_Camera::Projection_Orthographic);
cam->SetScale(s);
then call
cam->SetProjectionType(Graphics3d_Camera::Projection_Perspective);
cam->SetScale(s);
the camera will internally set its Eye distance such that the same scale s is achieved at the target
dev.opencascade.org
. This is exactly the same formula we derived (scale = distance*tan(fov/2)). OCCT also resets ZNear/ZFar on projection change if they were invalid
dev.opencascade.org
 – we should similarly recompute ours.
Setting Projection in OCCT: If integrating, the slider’s angle would be applied by:
if(angle == 0) {
    view->Camera()->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);
    // Set scale to previous scale or compute from last perspective distance & fov
} else {
    view->Camera()->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
    view->Camera()->SetFOVy(angle * M_PI/180.0); // FOVy expects radians in some APIs or degrees? likely radians.
}
Then we adjust scale/distance accordingly. Actually, OCCT provides:
SetFOVy() to set vertical FOV
dev.opencascade.org
.
SetScale() to set parallel scale (which if projection is perspective, will effectively set distance)
dev.opencascade.org
.
SetCenter() and SetEye() to position the camera
dev.opencascade.org
dev.opencascade.org
.
To keep the same target and pivot logic, we’d use SetCenter(targetPoint) and perhaps use SetDistance(distance) or MoveEyeTo(newEye). OCCT’s SetDistance moves Eye along the current direction vector to the given distance
dev.opencascade.org
dev.opencascade.org
. So changing distance while perspective effectively dollies the camera. For ortho, distance doesn’t change the view (it might still store it but not affect projection).
Near/Far in OCCT: We can use view->Camera()->SetZRange(N, F) to set near/far
dev.opencascade.org
. Alternatively, use ZFitAll(scaleFactor, BndBox) to auto-fit
dev.opencascade.org
. If OneCAD has access to the bounding box of visible shapes (via OCCT’s Bnd_Box of the assembly or part), we could call:
cam->ZFitAll(1.0, sceneBoundingBox, sceneBoundingBox); 
(The second Bnd_Box parameter is a “graphic bounding box” possibly for reflections, we can pass the same). This will adjust near/far to enclose the box
dev.opencascade.org
. OCCT’s documentation says for perspective it ensures objects in front of camera are contained, and for ortho it encloses entire volume
dev.opencascade.org
. This could be useful after a fit or when scene changes. We must be cautious not to call it every frame though, as it’s an expensive calc – better on discrete events (like after loading model or on zoom-extents action).
AIS_ViewController & AIS_AnimationCamera: If we were to use OCCT’s interactive toolkit, AIS_ViewController can abstract a lot of what we implemented (mouse mapping, rotation about points, etc.)
unlimited3d.wordpress.com
unlimited3d.wordpress.com
. Since we have our own, we may not use it, but it’s good to note:
It has built-in rotation around picked point and smooth integration with AIS_ViewCube (which we might use if we embed the view cube from OCCT).
It uses AIS_AnimationCamera to handle the interpolation of camera for view cube clicks
unlimited3d.wordpress.com
. AIS_AnimationCamera basically stores start and end camera and updates the view gradually
old.opencascade.com
old.opencascade.com
. If we wanted, we could instantiate an AIS_AnimationCamera with our V3d_View and call its Start/Update in a loop. But integrating that with our QOpenGLWidget might be complex. It’s simpler to implement our own interpolation as planned.
Recommendation: Continue using the custom camera path for now (since our rendering is custom OpenGL). If in the future OneCAD decides to leverage more of OCCT’s visualization (for example, to get instant tech features like hidden line removal or use AIS_ViewCube), we can map our CameraState to OCCT’s camera:
On each frame (or on changes), do:
Handle(Graphic3d_Camera) cam = view->Camera();
cam->SetCenter(target);
cam->SetEye(position);
cam->SetUp(upDir);
if(projection == ORTHO) {
    cam->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);
    cam->SetScale(orthoScale);
} else {
    cam->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
    cam->SetFOVy(fovY);
    cam->SetScale(orthoScaleEquivalent); // or directly set distance via SetDistance(distance)
}
cam->SetZRange(near, far);
This way, OCCT’s internal camera matches ours. We’d then call v3dView->Redraw() to have OCCT’s viewer update if it were used (in our case, we only use OCCT for geometry, so no redraw needed).
OCCT pitfalls: ensure to call V3d_View::MustBeResized() if the window size changes, so it updates aspect ratio; or directly set cam->SetAspect(aspect) on camera
dev.opencascade.org
. Also, if using stereo or other features, ensure those flags off (not needed now).
Animation via OCCT: If we wanted a quick solution, we could use AIS_AnimationCamera: set its start and end camera (via anim->SetCameraStart(camStart); anim->SetCameraEnd(camEnd);), set duration, and call anim->Start(). Then regularly call anim->Update(timerVal) and view->Update() in a loop. This handles interpolation. But since we already plan a custom animator, it’s optional.
In summary, OCCT’s camera API confirms our approach (matching scale to preserve view
dev.opencascade.org
, needing to recompute near/far on switch
dev.opencascade.org
). If OneCAD remains custom-rendered, we use it as reference but not directly. If needed, we ensure our data can be passed to OCCT’s viewer easily.
6. Qt 6 and Qt Rendering (RHI) Integration
OneCAD’s current implementation uses a QOpenGLWidget with direct OpenGL calls (OpenGL 4.1 Core on macOS). There is no Qt Quick or RHI being used at the moment. However, we should consider best practices in Qt’s rendering system, especially if migrating to Qt’s Rendering Hardware Interface (RHI) or maintaining efficiency in the current setup.
Camera Matrices in Qt’s Render Loop: In a QOpenGLWidget approach, we will compute the camera matrices each frame (or when updated) and pass them as uniforms to our shader programs. For example, in Viewport::paintGL():
QMatrix4x4 view = camera.viewMatrix();
QMatrix4x4 proj = camera.projMatrix();
QMatrix4x4 projView = proj * view;
shaderProgram.bind();
shaderProgram.setUniformValue("u_projViewMatrix", projView);
// draw grid, shapes, etc.
Since we are using Qt’s GL functions, QMatrix4x4 can be passed directly (it’s column-major by default to match OpenGL
doc.qt.io
). If we stick to QOpenGLWidget, we don’t need to worry about abstraction: we have full control over the GL state and shaders.
Considering Qt RHI / Qt Quick: If in the future OneCAD wants to use Qt Quick (for example, to integrate UI overlays more smoothly or use QML), then the rendering might be done via Qt’s RHI which can use Metal on macOS. In Qt Quick 3D, one would typically use a SceneEnvironment with a Camera item, and Qt Quick handles the projection differences (it even has camera components that can be orthographic or perspective with a FOV property). In that case, we might delegate to those classes instead of our own. But if we want to continue with custom rendering under RHI, we need to adapt how we provide the projection:
NDC Depth difference: As noted, Metal (and Direct3D) use a depth range of [0,1] instead of OpenGL’s [-1,1]. If we were writing shader code under RHI, we might have to adjust the projection matrix or the shader outputs. Qt’s RHI, however, often abstracts this. For instance, if using QShader, Qt might supply a uniform converting clip space if needed. We should be aware: in Metal, the projection matrix formula typically sets near plane to 0 and far to 1. If we end up needing to support both, we can conditionally modify the matrix. Since currently we target OpenGL ([-1,1] NDC), we can leave it. Should we switch to Metal (via RHI), Qt might provide a flag or we use QMatrix4x4::viewport() to map [-1,1] to [0,1] if needed
doc.qt.io
. This is a low-level detail; likely Qt takes care of it if using QQuick.
RHI frame loop: In QOpenGLWidget, update() triggers paintGL on the Qt GUI thread in sync with vsync. In Qt Quick, rendering happens on the render thread. We would instead schedule camera updates by calling QQuickWindow::update() which triggers a beforeRendering slot where we can update uniform buffers. We might need to maintain our matrix in a constant buffer if using RHI’s QRhi API. But unless performance demands moving to RHI, sticking with QOpenGLWidget is simpler at this stage (especially on macOS where OpenGL still works on Apple Silicon, though deprecated).
On-demand Rendering: We enforce that we do not have a continuous game loop. By default, QOpenGLWidget doesn’t continuously repaint unless update() is called repeatedly. So we ensure:
After processing an input event (which changes the camera), we call update() once. If the user is dragging, the mouse move events will continuously call update, but when the user stops, no further updates are called, thus no CPU/GPU usage.
For animations, we will use a timer or maybe QPropertyAnimation. For example, a QTimer with 60 FPS interval during animation:
connect(timer, &QTimer::timeout, this, [&]{
    if(animator.updateFrame()) update();
    else timer->stop();
});
timer->start(16);
This triggers ~60 repaints per second during the short animation, then stops. This approach respects on-demand rendering – we only render when needed.
Ensure vsync is enabled (QOpenGLWidget defaults to swap on vsync) so that we’re not rendering faster than the screen can display, to save CPU/GPU.
In Qt Quick (RHI), one might instead use QQuickItem::update() and in updatePaintNode() update the scene. But since we remain in widgets, our plan stands.
macOS Input Handling: Qt on macOS sends high-level gesture events for trackpads:
Pinch: as QNativeGestureEvent of type ZoomNativeGesture with a value() that is an incremental scale
doc.qt.io
. We will handle these in Viewport::nativeEvent() or by overriding event() and catching QEvent::NativeGesture. We accumulate the value or directly adjust zoom each time such an event arrives. Qt documentation says for Zoom, event.value() is typically a small number where you do scale *= (1 + value)
doc.qt.io
. We’ll follow that.
Two-finger swipe (for pan) often comes as QWheelEvent with pixelDelta or angleDelta, since macOS treats two-finger drag as a scroll. We can use that as well for panning (horizontal scroll = pan X, vertical scroll = pan Y). We need to differentiate it from zoom: typically pinch vs scroll are separate. (Pinch is zoom, two-finger move is scroll).
Rotate gesture (two-finger rotate) could be captured if we wanted to allow camera roll, but we probably won’t use it (as it’s not standard in CAD to rotate view plane with gesture, except for special cases).
We must enable gesture recognition: either by grabGesture(Qt::PinchGesture) for QGestureEvent or rely on native as above. Actually QNativeGestureEvent is automatic on macOS; no need to grab, just reimplement event() and handle Type == QEvent::NativeGesture.
SpaceMouse: Not explicitly asked, but CAD often supports 3Dconnexion spacemouse. That would require using HID input. Possibly out of scope, but keep in mind for future (maybe Qt 6 has some support or we get raw USB events).
Performance and RHI: If moving to Qt6’s RHI, the actual difference for us would be minimal if we stick to QOpenGLWidget (it will still use OpenGL under the hood on macOS). If we went to QQuickFramebufferObject or similar in QML, we might have to use a QSGGeometryNode and handle a QRhiCommandBuffer, which is a different paradigm. At that point, it might be easier to incorporate our logic into a QQuick3D scene (which has a PerspectiveCamera with FOV that we could bind to our slider, and an OrthographicCamera we toggle). But this would be a significant rewrite and beyond the immediate need.
Continuous vs On-Demand Rendering Recap: We ensure the render loop is event-driven. This is exactly how typical Qt apps behave: “The only reason most apps run constantly is because they put rendering in an infinite loop… If you have no such loop, you render frames in response to events instead.”
stackoverflow.com
. We will follow that principle.
Threading: QOpenGLWidget runs on GUI thread (with a behind scenes context switch). We should do camera updates on the GUI thread (from event handlers). If we were to do heavy animations or long computations, we’d consider moving those off thread, but here it’s lightweight.
In conclusion, integrating with Qt is straightforward since we manage our own matrices. We just must pay attention to differences if switching backend. For now, keep using QMatrix4x4::perspective() (OpenGL convention) – if later using Metal, we may need to call cameraProjMatrix = vulkanStyleMatrix * cameraProjMatrix or adjust near/far mapping. The Qt documentation confirms QMatrix4x4’s viewport() can map NDC to any range
doc.qt.io
 if needed.
7. Edge Cases and Testing Plan
Implementing camera control can introduce edge cases. We list key scenarios to handle and how to test them: Edge Cases:
Gimbal Lock / Up-Vector Flip: If the user pitches the camera straight up or down (±90° pitch), a naive implementation might cause a gimbal lock (camera “flips” orientation unexpectedly). We need to handle this by clamping pitch to just shy of ±90°, or by smoothly switching the yaw reference. Using quaternions avoids sudden flips. We will test orbiting straight overhead: the camera should behave predictably (perhaps orbiting around, keeping up-axis consistent). This is a classic edge case for camera controls.
Orthographic Near/Far Clipping: In orthographic mode, if the camera’s position is far from the target (say distance is large because user toggled from perspective), near plane might clip the scene unexpectedly (because in ortho, geometry at any distance from camera is drawn the same, but OpenGL still clips by camera distance). We should ensure in ortho mode, the near plane is set to a minimal value that doesn’t cut off the model. Possibly set near = 0 (or a very small positive like 1e-3) in ortho always, and far sufficiently large. Since ortho doesn’t suffer depth precision issues as severely (no 1/z), it’s fine to have a large range. We’ll test by toggling to ortho when camera is far and see that the model is still visible.
Extremely large model coordinates: If the model bounding box is huge (e.g. extends from -100000 to +100000 in some axis), our far plane and camera distance might be extremely large. We need to test zoom-extents on such a model: does the model render without z-fighting or clipping? If issues, we might incorporate a logarithmic depth or adjust far less aggressively (perhaps not showing very far parts until user zooms out). Testing with a dummy large scene will inform adjustments.
Extremely small or thin objects: If a model has very fine features (like a thin wire) and the user zooms in very closely in perspective, the near plane must be very small to allow the camera inside the scene. Test moving camera inside a hollow object – ensure near plane is lowered dynamically (or user can manually set a section plane – but that’s advanced). At some point, small near might cause depth precision issues: see if z-fighting appears on coplanar faces. We can mitigate by polygon offset or by encouraging ortho for very close inspection if needed.
Switching Projections mid-orbit: If the user presses a hotkey to toggle ortho/perspective while actively orbiting or moving, the interpolation might be strange. We might simply switch mode instantly (with scale conversion) and continue orbit with no break. We test: orbit the model in perspective, then toggle to ortho (0°) and continue dragging – the transition should be smooth (no jump in model size or rotation direction). Similarly from ortho to perspective. Because our formulas preserve scale, the only potential hitch is if near/far change (the background could change if far clipping the grid or so, but that’s minor).
Empty Scene / Reset View: If no object is in the scene (or after deleting all), “fit all” might degenerate (no bounds). We should define behavior: perhaps reset camera to default orientation and distance (some default distance like a unit). We test opening an empty file: ensure no weird behavior (maybe just show grid and axes with camera at origin).
Precision of float vs double: QMatrix4x4 uses float internally. If the scene coordinates are extremely large (e.g. coordinates ~1e7), the single precision might lose some precision in view matrix. This could cause jitter in movement. Qt’s recommendation would be to keep object coordinates moderate and not exceed 32-bit float precision significantly. OCCT uses double for geometry, but when we feed it into OpenGL we convert to float matrices. A mitigation if needed: use relative coordinates (like subtract the camera pivot or scene center to reduce magnitude). This is a deep edge case; we note it but likely not crucial unless modeling something astronomically large.
High DPI / Retina displays: Ensure that cursor-based operations account for device pixel ratio if needed (Qt’s QWheelEvent etc. typically already consider that, but just to be safe, we’ll use logical pixel deltas). Probably fine as Qt abstracts this.
Interaction Conflicts: e.g. if the user scrolls the mouse wheel while holding right-click dragging – our code should probably prioritize one (likely zoom overrides pan if simultaneous). Typically, such multi-input conflict is rare or handled by OS (two-finger pinch vs swipe are separate gestures). We test overlapping inputs (like quickly doing a gesture after a mouse release) to ensure no stuck states.
Animation Interrupt: Start a view animation (e.g. click view cube face with animate on), then immediately orbit (mouse drag) before it finishes. Expected: animation stops and yields to user control. We implement that by checking in mousePress: if an animation is running, stop it. Test: set a long animation (for testing), try to break it by interacting.
Persistence Check: Test saving and loading:
Open a document, change camera (rotate to some odd angle, switch to perspective 27° FOV, hide grid).
Save document. Close and reopen.
Verify camera angle slider is at 27°, projection mode correct, orientation same, grid still hidden, etc. This ensures our state serialization works.
Also test that creating a new document uses defaults from Preferences (not the last doc’s state).
Zoom/Pan at extremes: If the user zooms out extremely far in perspective, our camera distance grows and eventually the model becomes very small (and near plane might exclude it if not adjusted). We may clamp zoom-out at some multiple of scene size to avoid losing the model entirely (maybe user must use “fit” if lost). Similarly, zoom-in too far in perspective might pass through the target; we might clamp at a small distance (or allow going through – but then you start seeing inside objects which can be confusing without clipping plane). We likely clamp at a small positive distance (like the camera cannot go closer than 1% of the object size to its target in perspective). In ortho, zoom-in is theoretically unbounded (you can scale as much as you want), so we may clamp at some extremely high scale to avoid numerical issues. We test by extreme scroll in/out to see if any glitches or if values overflow.
Testing Plan: We will create a series of tests (some automated, some manual):
Unit Tests (Mathematical): We can write small tests for the conversion formulas:
Test that switching projection preserves size: e.g. set camera with distance=500, fov=30°, target at origin. Compute a point at target + some offset (say (100,0,0) as 100 units to the right in world). Project it with perspective matrix. Then switch to ortho via formula and project again with ortho matrix. The point’s screen X coordinate size difference should be negligible. We could automate this comparison: difference should be < 1 pixel.
Test lookAt matrix orthonormality: ensure that after camera rotates, the view matrix’s basis vectors remain orthogonal and correctly oriented.
Test zoom to cursor math: simulate camera and a point, apply a zoom step, ensure point’s screen position stays roughly same. This might require a known intersection (like point on axis).
Depth precision scenarios: perhaps not a unit test, but we can log depth buffer range and precision bits consumption for some typical ranges to verify we’re not exceeding safe limits. For example, near=0.1, far=10000 (far/near=100000) on 24-bit is borderline but maybe okay; near=0.001, far=1e6 (ratio=1e9) will definitely cause z-fighting – test with two coplanar surfaces and see if they flicker.
Integration Tests (Manual/Visual):
Basic Orbit/Pan/Zoom: Open a sample model. Try orbiting around it – ensure it stays in view and rotates around correct pivot. Pan the view – model moves as expected. Zoom in/out – does it zoom to cursor? (We can place cursor on a specific feature and zoom, see if that feature remains under cursor). Try on both perspective and ortho (slider = 0 vs slider = e.g. 30°).
Projection Toggle: With a model in view, slide the camera angle from 0 to 90. Watch for any sudden jumps or scale changes, especially around the 0 mark. Expectation: model size remains stable except for perspective distortion coming in gradually. At 90° (extreme fisheye), check that near plane is still okay – wide FOV might include more of scene; far plane might need extension (though if we always tie far to scene bounds, it should already cover).
Animate Transitions: Enable Animate Camera. Click view cube top, see smooth rotation to top view. Then click front, etc. It should animate through a sensible path (preferably the shortest rotation). If one toggles off animate and clicks, it should jump. Toggle on again, try selection of a different pivot and animate – e.g., select a sub-object and have a function to focus on it (if implemented, say double-click sets pivot to selection and smoothly zooms into it). This tests the animator on position changes too.
Extreme Angles: Tilt the camera at high pitch (close to zenith/nadir) and orbit – ensure no sudden flips. The up vector might reorient at the poles; if we implement a mechanism to prevent flipping (like clamping pitch to 89.9°), test that we can’t go beyond that.
Multiple Models / Scenes: Try a very large model (simulate by scaling geometry or a large bounding box) and a very small one:
Large: Ensure at full extents, depth precision still allows front/back without fighting. If you notice z-fighting (e.g. flickering grid on the far end), consider increasing depth precision or adjusting near.
Small: Zoom in on a tiny feature (like 1 unit object). If perspective near becomes extremely small (<1e-6), we might see depth issues or see through surfaces due to near clipping. Evaluate if this is acceptable or if we need a more adaptive near (like relative to focus distance).
Grid and Axes Rendering: Toggle grid/axes on and off quickly, including during an animation (should just appear/disappear, no crash). Check that in perspective, grid lines converge properly (since they are at z=0 plane, they should appear smaller as they recede). In ortho, grid stays parallel. Axes lines: in perspective they vanish toward far distance, in ortho they remain equal thickness. Just visually confirm they’re drawn and correctly oriented.
Performance: While orbiting continuously or having an animation, monitor that the app is re-rendering at a reasonable frame rate (~60fps on typical hardware) and stops rendering when idle (CPU/GPU usage drops). This ensures our on-demand mechanism works. If we inadvertently called update() in a loop, we’d see high usage when idle – test idle state (no input, no animation) = 0% GPU ideally.
Concurrent UI: Ensure the presence of this new control doesn’t freeze the UI. For instance, adjusting the slider should be smooth and not cause large hiccups (if it triggers expensive recalculations? It shouldn’t beyond re-render).
State Persistence: As described, save and reload with different settings to ensure nothing is lost. Also test that preferences (defaults) actually apply for new documents (like if user turns off “Animate Camera” in prefs, then opens a new model, the toggle is off and view changes jump).
Multiple Viewports (if applicable): If OneCAD ever has two viewports (say side by side), ensure they can have independent camera states. Our design allows that by having separate CameraState per viewport. If in the future they want linked viewports (e.g. four-view layout), then linking those cameras would be extra logic (not currently needed).
For automated testing, many aspects (like projecting points and verifying continuity) can be done with known geometric configurations. User interaction and visual smoothness are subjective and will be verified manually by QA and by us during development with various test models. We will also leverage the test model from Shapr3D’s tutorial (like a motorcycle cover, if available) or any complex model to see how the perspective vs orthographic looks, since the user might compare to expectations from other CAD tools.
8. Final Recommended Defaults and Settings
Based on research and best practices, we propose the following default values and behaviors for OneCAD’s camera system:
Default Projection Angle: Start with a perspective view of about 30° vertical FOV. This gives a mild perspective that provides depth cues without extreme distortion. It’s a compromise between purely orthographic (0°) and a wide 60–90° view. (Shapr3D doesn’t state their default, but they encourage “some perspective”
support.shapr3d.com
. Many CAD programs default to orthographic for modeling; however, since OneCAD targets beginners, a bit of perspective makes the view more realistic. We choose ~30° as it corresponds roughly to a 60mm lens – slightly telephoto, minimizing distortion but still perspective.) The slider will thus initialize around 30°. Advanced users can always set it to 0 for true ortho if needed for drafting.
Default “Animate Camera”: ON (true). A smooth animated transition helps users orient themselves when changing views, and modern hardware easily handles the small interpolation. It also gives OneCAD a more polished feel. If a user prefers instant changes, they can turn it off in preferences.
Default Toggles: Show Grid = ON, Show World Axes = ON, Show Pinned Measurements = ON. Rationale: for a beginner-friendly CAD, the grid and axes provide important reference context, and any dimensions the user created should be visible by default. These can be toggled off if the user wants an unobstructed view (e.g. for presentation or screenshots). We’ll ensure these default states are set in the Preferences on first run.
Orbit Pivot Mode Default: Geometry Bounding Box Center. When a model is loaded or created, it makes sense to orbit about its center so the whole model rotates in view. World origin could be confusing if the model is not centered on origin (it might orbit off-screen). We will still allow easily resetting pivot to origin (maybe via a menu “Reset pivot to origin”) or switching to selection-based pivot when an object is selected (some CAD do this automatically: orbit around selection if something is selected, otherwise around global). As an enhancement, we might implement “auto-pivot”: e.g. if user selects a part and does Zoom to Fit Selection, we set pivot = that part’s center. For now, default to whole-scene center, which covers common use.
Near/Far Planes: We set Near = 0.1 * scene size unit by default and Far = 1000 * scene size unit for an empty/new scene. For example, if unit is millimeters and we expect typical objects ~1000 mm in size, near=0.1 mm and far=1000000 mm (1000 m) might be overkill. Instead, we can set absolute defaults like Near = 1.0, Far = 10000. We should confirm unit scale (if OCCT uses mm internally, a cube of side 1000 means 1000 mm = 1 m). Possibly use Near = 1 mm, Far = 10,000 mm as initial.
After loading a model or on zoom-extents, we update Near/Far to tightly fit as described earlier (maybe far = distance to furthest corner of model + 50% margin, near = max(distance/100, modelMinExtent/10, some small const)).
We recommend enabling a depth clamp if available (OpenGL has extension GL_ARB_depth_clamp) to avoid near clipping when near is small, but on macOS GL4.1 this might not be available. We won’t rely on it by default.
We will not implement reversed-Z now due to OpenGL limits, but note it as a future upgrade when moving to Metal/Vulkan.
Sensitivity and Increments: Choose intuitive sensitivities for user input:
Orbit: 1 pixel mouse movement = ~0.2° rotation (tune so a Drag across half the screen ~ 180°).
Pan: 1 pixel = moves camera by ~0.5% of current view width (so panning speed scales with zoom level).
Zoom: mouse wheel notch = maybe 1.2× zoom factor (20% change). Pinch gesture scale we use directly as provided (Apple seems to give small increments like 0.01 which correspond to 1%).
These can be tweaked by testing to feel neither too slow nor too jumpy.
Clamping:
Clamp perspective FOV minimum to maybe 5° if we don’t want too extreme telephoto (but since slider goes to 0 for ortho, user has that path, so we allow down to 1° effectively as perspective). We will treat <1° as 0 in code to avoid tan(φ/2) issues.
Clamp FOV maximum – slider naturally at 90°. We won’t allow beyond 90 (which is already quite wide). 90° is fine as max; if user tries to type a value beyond, we cap it.
Clamp pitch to ±89° as noted to avoid flip.
Clamp distance to a small positive minimum (e.g. 1e-3) to avoid division by zero or inversion of camera through target.
Clamp orthoScale to >0 as well (cannot be 0). If user zooms in massively, we might stop at a scale like 1e-6 or so – beyond that, floating precision means you’re effectively at maximum zoom.
Visual Aids: Perhaps as default, if the user toggles to orthographic (0°), we might display a small label “ORTHO” somewhere, since newbies might not realize. But since the UI slider itself shows 0°, maybe it’s enough. Some CADs show an “ORTHOGRAPHIC” indicator. We can consider adding a subtle indicator in the viewport.
Reset Camera: Provide a “Default View” button or command that resets camera to default orientation and projection (likely something like isometric view, perspective at default angle, pivot at origin or model center). This is typically available (Shapr3D has “Default View” option). We’ll implement that, default to iso-perspective with above defaults.
By adhering to these defaults and thoroughly testing, we ensure the camera control meets the spec and provides a smooth, intuitive experience similar to Shapr3D and other modern CAD systems. All critical choices (projection blending, pivot behavior, depth handling) have been backed by documentation and industry practices, ensuring our implementation is robust and user-friendly. Sources:
Shapr3D Help Center – View and Appearance settings
support.shapr3d.com
 (confirmation of slider 0°=ortho, 90°=persp, animate toggle, pinned measurements, etc.)
StackOverflow – preserving size when switching projections
stackoverflow.com
stackoverflow.com
 (used to derive formulas for ortho scale vs perspective distance)
NVIDIA Technical Blog – depth precision and reversed-Z
developer.nvidia.com
developer.nvidia.com
 (for rationale on depth range decisions)
OpenCASCADE documentation – Graphic3d_Camera (confirmed projection mode switch handling and scale concept)
dev.opencascade.org
dev.opencascade.org
OCCT Unlimited3D blog – AIS_ViewController (for advanced pivot rotation and animation interrupt ideas)
unlimited3d.wordpress.com
unlimited3d.wordpress.com
.
Citácie

Views and Appearance – Shapr3D Help Center

https://support.shapr3d.com/hc/en-us/articles/7873944390812-Views-and-Appearance

Views and Appearance – Shapr3D Help Center

https://support.shapr3d.com/hc/en-us/articles/7873944390812-Views-and-Appearance

Views and Appearance – Shapr3D Help Center

https://support.shapr3d.com/hc/en-us/articles/7873944390812-Views-and-Appearance

Views and Appearance – Shapr3D Help Center

https://support.shapr3d.com/hc/en-us/articles/7873944390812-Views-and-Appearance

AIS_ViewController in OCCT 7.4.0 – Unlimited 3D

https://unlimited3d.wordpress.com/2019/11/06/ais_viewcontroller-in-occt-7-4-0/

AIS_ViewController in OCCT 7.4.0 – Unlimited 3D

https://unlimited3d.wordpress.com/2019/11/06/ais_viewcontroller-in-occt-7-4-0/

QNativeGestureEvent Class | Qt GUI | Qt 6.10.1

https://doc.qt.io/qt-6/qnativegestureevent.html

QNativeGestureEvent Class | Qt GUI | Qt 6.10.1

https://doc.qt.io/qt-6/qnativegestureevent.html

AIS_ViewController in OCCT 7.4.0 – Unlimited 3D

https://unlimited3d.wordpress.com/2019/11/06/ais_viewcontroller-in-occt-7-4-0/

AIS_ViewController in OCCT 7.4.0 – Unlimited 3D

https://unlimited3d.wordpress.com/2019/11/06/ais_viewcontroller-in-occt-7-4-0/

QMatrix4x4 Class | Qt GUI | Qt 6.10.1

https://doc.qt.io/qt-6/qmatrix4x4.html

QMatrix4x4 Class | Qt GUI | Qt 6.10.1

https://doc.qt.io/qt-6/qmatrix4x4.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

Visualizing Depth Precision | NVIDIA Technical Blog

https://developer.nvidia.com/blog/visualizing-depth-precision/

Visualizing Depth Precision | NVIDIA Technical Blog

https://developer.nvidia.com/blog/visualizing-depth-precision/

Visualizing Depth Precision | NVIDIA Technical Blog

https://developer.nvidia.com/blog/visualizing-depth-precision/

c++ - Switching from perspective to orthogonal keeping the same view size of model and zooming - Stack Overflow

https://stackoverflow.com/questions/54987526/switching-from-perspective-to-orthogonal-keeping-the-same-view-size-of-model-and

c++ - Switching from perspective to orthogonal keeping the same view size of model and zooming - Stack Overflow

https://stackoverflow.com/questions/54987526/switching-from-perspective-to-orthogonal-keeping-the-same-view-size-of-model-and

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

AIS_ViewController in OCCT 7.4.0 – Unlimited 3D

https://unlimited3d.wordpress.com/2019/11/06/ais_viewcontroller-in-occt-7-4-0/

Open CASCADE Technology: AIS_AnimationCamera Class Reference

https://old.opencascade.com/doc/occt-7.2.0/refman/html/class_a_i_s___animation_camera.html

Open CASCADE Technology: AIS_AnimationCamera Class Reference

https://old.opencascade.com/doc/occt-7.2.0/refman/html/class_a_i_s___animation_camera.html

Open CASCADE Technology: Graphic3d_Camera Class Reference

https://dev.opencascade.org/doc/refman/html/class_graphic3d___camera.html

QMatrix4x4 Class | Qt GUI | Qt 6.10.1

https://doc.qt.io/qt-6/qmatrix4x4.html

QMatrix4x4 Class | Qt GUI | Qt 6.10.1

https://doc.qt.io/qt-6/qmatrix4x4.html

lwjgl - Non-continuous (on demand) rendering in desktop OpenGL 3.0+ - Stack Overflow

https://stackoverflow.com/questions/23602938/non-continuous-on-demand-rendering-in-desktop-opengl-3-0

Introduction to 2D sketch tools and settings – Shapr3D Help Center

https://support.shapr3d.com/hc/en-us/articles/13079114694684-Introduction-to-2D-sketch-tools-and-settings

c++ - Switching from perspective to orthogonal keeping the same view size of model and zooming - Stack Overflow

https://stackoverflow.com/questions/54987526/switching-from-perspective-to-orthogonal-keeping-the-same-view-size-of-model-and

c++ - Switching from perspective to orthogonal keeping the same view size of model and zooming - Stack Overflow

https://stackoverflow.com/questions/54987526/switching-from-perspective-to-orthogonal-keeping-the-same-view-size-of-model-and
