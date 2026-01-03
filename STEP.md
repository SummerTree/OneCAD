# ChatGPT Research

Introduction and STEP Application Protocols in CAD
STEP (ISO 10303) is a family of standards for 3D CAD data exchange. Modern CAD applications typically use three STEP Application Protocols (APs): AP203, AP214, and AP242. AP203 (first published mid-1990s) provides the basics – 3D geometry and assembly structure – but does not support colors or layers. AP214 extends AP203 by adding things like colors, layers, and GD&T (it includes most of AP203 plus visual attributes)
techsupport.sds2.com
. AP242 (newest, “Managed Model Based 3D Engineering”) merges AP203 and AP214 capabilities and is the preferred format going forward
techsupport.sds2.com
. In practice, AP242 is the first choice (supports geometry, assemblies, colors, PMI, etc.), followed by AP214 (if AP242 isn’t available), and AP203 is legacy
techsupport.sds2.com
. Most STEP import libraries (including OpenCASCADE’s) handle all three, but AP242 ensures the richest data (e.g. PMI, tessellated data, advanced metadata) for CAD workflows. When importing a STEP file, it’s important to handle the variety of data it may contain and map it correctly into your CAD system:
Geometry and Topology (B-Rep): STEP conveys precise B-Rep geometry (surfaces, curves) and topology (faces, edges, shells, solids). OpenCASCADE (OCCT) will reconstruct these as TopoDS_Shape objects (faces, edges, compounds, etc.). Expect a complex B-Rep structure with possibly tolerance values on edges/vertices to account for modeling inaccuracies
unlimited3d.wordpress.com
unlimited3d.wordpress.com
. You’ll typically get manifold solids (closed volumes), but STEP can also include open shells or surface models (e.g. AP242 “ShellBasedSurfaceModel” for non-solid geometry). OCCT can import these as open shells if present (they won’t form a solid until sewn). Keep an eye on TopoDS_Solid vs TopoDS_Shell – missing solids usually mean the STEP contained surfaces only.
Units: A STEP file defines the length units (e.g. millimeter vs inch) in its header. OCCT’s STEP reader will scale the geometry to a consistent system. By default, OCCT assumes the “working unit” is millimeters
discourse.salome-platform.org
. If the STEP file’s units differ, OCCT either scales the model or provides a scale factor. A common pitfall is unit mismatches – e.g. a model comes in 1000× too small or large. This is often because the STEP data (say in mm) was interpreted in meters (scaled by 0.001) or vice-versa
discourse.salome-platform.org
. OCCT’s XDE tools offer control: for example, you can disable automatic meter conversion by Interface_Static::SetCVal("xstep.cascade.unit", "MM") or use XCAFDoc_DocumentTool::SetLengthUnit() in newer OCCT
dev.opencascade.org
. Always verify scale after import – e.g. query the document’s unit: XCAFDoc_DocumentTool::GetLengthUnit() – to ensure a 100 mm part isn’t showing as 0.1 mm or 100,000 mm. If scaling issues appear (tiny or huge model), adjust the import conversion or your viewing transform accordingly.
Assemblies (Product Structure): STEP supports hierarchical assemblies: parts, sub-assemblies, and instances (occurrences) with transforms. In STEP, assembly structure is defined by products and placement relations. OCCT’s import will preserve this by separating shape definitions from their locations. In OCCT’s XDE (eXtended Data Exchange) model, a free shape corresponds to a top-level part or assembly, and an assembly node contains component references to other shapes with placement transformations
github.com
github.com
. There can be multiple root free shapes (STEP files can have more than one top-level object)
github.com
. Each assembly instance in STEP becomes an OCCT reference with a TopLoc_Location transform linking to a shared sub-shape. Your import pipeline must traverse this assembly tree to gather all parts. (OCCT provides tools for this – discussed below – so you don’t have to manually parse the STEP structure.)
Names and Metadata: STEP can include names for parts and sub-entities. AP242 even allows names on almost any entity. Typically, each Part or Assembly will have a name (e.g. the STEP PRODUCT name). OCCT XDE attaches these to the corresponding labels via TDataStd_Name attributes
old.opencascade.com
dev.opencascade.org
. There are also optional metadata like part numbers, product identifiers, or notes. AP242 extends this further with PMI (Product Manufacturing Information: GD&T, annotations) and things like saved views and layers. OCCT’s STEP reader (when using XDE) can import PMI (as separate XCAF structures) and layers. For example, XDE can capture GD&T (tolerances), stored under a separate section if present
old.opencascade.com
. These are advanced; if your focus is geometry and basic attributes, you can ignore PMI for now, but be aware AP242 files might contain them.
Colors and Layers: AP214 and AP242 support color attributes – either per part or per face. XDE (OCCT’s extended framework) will import these. Colors may be attached at different levels:
Part-level color: a color assigned to an entire part (all its faces inherit this unless overridden).
Face-level color: specific faces have color overrides.
XDE stores colors in a color table and links them to shape labels. It supports colors down to faces and edges
old.opencascade.com
. If a STEP file has a part with green housing and some faces painted red, the OCCT XDE document will reflect that (the part’s label gets a generic color, and the red faces get their own color attributes on sub-labels). Layers (AP214) are imported similarly, but layers are less used in modern STEP. Ensure your STEP reader is in “color mode” to get these — by default STEPCAFControl_Reader does enable color and layer reading (its constructor sets ColorMode/LayerMode true)
dev.opencascade.org
dev.opencascade.org
.
Common STEP Data Pitfalls: Be prepared for a few issues when bringing STEP data in:
Unit/Scale errors as mentioned – if the model’s size looks off by a factor (10, 25.4, 0.001 etc), it’s likely a unit conversion problem. Check the STEP units and adjust OCCT’s unit setting or your interpretation
discourse.salome-platform.org
.
Orientation and Normals: Sometimes STEP solids come in with inverted normals (e.g. a solid appears inside-out). This can happen if face orientation flags aren’t handled or if the CAD system had different conventions. OCCT generally orients solids correctly, but if you see weird lighting (all faces dark, interior showing), you may need to flip normals or disable backface culling. We’ll address this in the rendering section.
Tolerances and Sewing: STEP files often include tolerances (per edge/face). Large tolerances can mean a model that is not perfectly watertight (small gaps allowed). OCCT imports those tolerances; if a model has tiny gaps, the mesh might show cracks or holes. If you encounter this, consider running a sewing algorithm (BRepBuilderAPI_Sewing) to stitch gaps, or adjust mesh deflection (very fine mesh might slip through a gap smaller than tolerance). On the other hand, very small tolerances or tiny features might cause performance issues; OCCT’s meshing will try to honor them, possibly creating very dense meshes on tiny fillets, etc. It’s a balance: you might sometimes increase deflection (lower quality) to avoid over-meshing insignificant details.
Non-manifold geometry: Most STEP models are manifold solids, but if the file contains non-manifold configurations (e.g. a surface shared by more than two solids, or dangling faces), OCCT might import them as separate shapes or skip them. Non-manifold cases are rare in standard AP files, but you might see multiple solids that touch at a face or edge – they’ll come in as separate solids in an assembly (since STEP doesn’t support true non-manifold). Also, free surfaces (not closed) will import as TopoDS_Face or open shells; you may need to render two sides or cap them manually if needed.
STEP Import Pipeline in OCCT (XDE / OCAF)
OpenCASCADE provides a high-level data exchange framework called XDE (Extended Data Exchange) for importing STEP with geometry and associated attributes (names, colors, assembly structure, etc.). The core class for STEP import is STEPCAFControl_Reader, which reads a file into an OCAF document (OpenCASCADE Application Framework document). This is the recommended “correct way” because it handles assemblies, names, and colors automatically (using XDE)
dev.opencascade.org
. In contrast, the lower-level STEPControl_Reader can give you a raw TopoDS_Shape but will lose metadata – we want the richer pipeline. Setting up the OCAF Document: Before reading, you need an OCAF document ready to receive the data. In OCCT, an OCAF TDocStd_Document holds a tree of labels and attributes (this is where shapes and their metadata go). For XDE, you must initialize the document with XDE extensions:
Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
BinXCAFDrivers::DefineFormat(app);  // enable binary XCAF format (optional, for saving)
Handle(TDocStd_Document) doc;
app->NewDocument("BinXCAF", doc);   // or "XmlXCAF" for XML format
// Now 'doc' is an empty XDE document, with the XCAF structure initialized.
The DefineFormat calls register the XCAF persistence schema (so you could save to .xbf if needed)
unlimited3d.wordpress.com
unlimited3d.wordpress.com
. Even if you don’t plan to save, it’s good practice to call them. You can verify the doc is XCAF-ready by XCAFDoc_DocumentTool::IsXCAFDocument(doc)
github.com
. Reading the STEP file:
STEPCAFControl_Controller::Init();  // Initialize STEP (and XCAF) translators
STEPControl_Controller::Init();     // (Often recommended in OCCT docs to init controllers)
STEPCAFControl_Reader reader;
IFSelect_ReturnStatus stat = reader.ReadFile("model.step");
if(stat != IFSelect_RetDone) {
    std::cerr << "Error: cannot read STEP file\n";
    return;
}
// Transfer contents into the OCAF document
if(!reader.Transfer(doc)) {
    std::cerr << "Error: STEP transfer failed (no shapes)\n";
    return;
}
The ReadFile parses the STEP file into an internal STEP model, and Transfer(doc) converts it into the OCAF document structure
github.com
unlimited3d.wordpress.com
. By default, STEPCAFControl_Reader is created with Color/Name/Layer modes ON
dev.opencascade.org
dev.opencascade.org
, so it will bring those into the doc. (If for some reason you needed to disable color or metadata, there are SetColorMode(False) etc., but normally keep them True.) After Transfer, the TDocStd_Document now contains:
A Shape section (under label 0:1:1 by convention) managed by XCAFDoc_ShapeTool, which holds the assembly structure and all shapes.
Auxiliary sections for Colors (XCAFDoc_ColorTool), Layers (XCAFDoc_LayerTool), and others (Notes, Views, etc., if present).
Traversing the Assembly Structure: The XCAF shape tool will give you the top-level assembly components. For example:
Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
TDF_LabelSequence freeShapes;
shapeTool->GetFreeShapes(freeShapes);
for(int i=1; i <= freeShapes.Length(); ++i) {
    TDF_Label rootLabel = freeShapes.Value(i);
    TopoDS_Shape rootShape = shapeTool->GetShape(rootLabel);
    // ... process rootShape (could be an assembly compound or a single part)
}
“Free shapes” are essentially the root nodes of the assembly (each corresponds to a top-level part or assembly in the STEP file)
github.com
github.com
. There can be one or many. Each rootShape might be a compound representing an assembly, or a lone shape. You can check with shapeTool->IsAssembly(rootLabel) – if True, this label has children (components)
dev.opencascade.org
. To recurse into assemblies, you have options:
Use XCAFDoc_ShapeTool::GetComponents(rootLabel, compLabels) to get the immediate child components (if the API is available; in older OCCT, components might be obtained differently). Alternatively, you can use the lower-level OCAF traversal: iterate over the sub-labels of rootLabel – each sub-label that IsComponent() would be an instance. The XCAF assembly structure can be non-trivial to navigate manually, so OCCT provides XCAFDoc_AssemblyIterator and XCAFDoc_AssemblyTool::Traverse to walk it easily
dev.opencascade.org
dev.opencascade.org
.
For simplicity, if you just want all shapes (like a flat list of all part shapes), XCAF offers XCAFDoc_AssemblyTool::Traverse which can collect parts. For example, you could traverse with a filter to accept only part (leaf) nodes
dev.opencascade.org
. However, if you need to preserve hierarchy for, say, a tree view, you’ll build that by recursion: start at freeShapes, then for each, iterate its children, etc.
Each assembly instance in XCAF is represented by a label that has a reference to a shape definition and a TopLoc_Location. The actual TopoDS_Shape returned by GetShape(instanceLabel) already has the location applied (so rootShape above, if it’s an assembly compound, will have its sub-shapes positioned properly). If you need the transform of an instance, use shapeTool->GetLocation(label) to get the TopLoc_Location
old.opencascade.com
. Also, shapeTool->GetReferredShape(instanceLabel, refLabel) can tell you the label of the definition that an instance refers to
old.opencascade.com
 – usually not needed unless you want to identify repeated parts. Extracting Names: If the STEP file had part names, those are stored as a TDataStd_Name attribute on the part’s label. You can retrieve it like:
Handle(TDataStd_Name) nameAttr;
if(label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
    TCollection_ExtendedString name = nameAttr->Get();
    std::wcout << L"Part name: " << (const wchar_t*) name.ToExtString() << std::endl;
}
XCAF will have attached part-level names for PRODUCT entities (and assembly names)
dev.opencascade.org
. It does not automatically concatenate an assembly tree path or anything – each component has whatever name the STEP gave it. Entity names (names on individual faces/edges) are less common in STEP and are generally not imported unless specifically enabled (they tend to clutter the structure). The main names you’ll use are part and sub-assembly names. Extracting Colors: Use XCAFDoc_ColorTool. Get it via XCAFDoc_DocumentTool::ColorTool(doc->Main()). The color tool lets you query colors on shapes:
Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());
Quantity_Color col;
if(colorTool->GetColor(shape, XCAFDoc_ColorSurf, col)) {
    // 'shape' (TopoDS_Shape) has an explicitly assigned surface color in STEP
    Standard_Real r=col.Red(), g=col.Green(), b=col.Blue();
}
Here we try ColorSurf (surface color). GetColor(TopoDS_Shape, XCAFDoc_ColorType, Quantity_Color) returns True if a color is found
dev.opencascade.org
. You can call this on an entire part shape – if the part had an overall color, you’ll get it. And you can call it on face shapes (e.g. within a face loop) to see if a face had its own color. Important: The TopoDS_Shape you pass must be one owned by the XCAF document (it uses the shape as a key to find the label). So make sure you retrieve sub-shapes from the XCAFDoc_ShapeTool or from the TopoDS_Shape that came out of it. A typical pattern:
// After meshing or exploring faces:
for(TopExp_Explorer exp(rootShape, TopAbs_FACE); exp.More(); exp.Next()) {
    TopoDS_Face face = TopoDS::Face(exp.Current());
    Quantity_Color fcol;
    if(colorTool->GetColor(face, XCAFDoc_ColorSurf, fcol)) {
       // face has its own color
    } else if(colorTool->GetColor(rootShape, XCAFDoc_ColorSurf, fcol)) {
       // face doesn’t have specific color, but part has a color (use fcol)
    }
}
XCAF actually distinguishes surface color vs curve color vs generic color
old.opencascade.com
. Typically use XCAFDoc_ColorSurf for faces and XCAFDoc_ColorCurv for edges. (A “generic” color means a default applied to both if no specific overrides – often not needed to check if you handle Surf and Curv properly.) OCCT’s XCAF vs raw TopoDS: It’s worth noting that behind the scenes, XCAF is populating an assembly graph and linking to actual TopoDS_Shape geometry. Each label in the ShapeTool has a TopoDS_Shape (via attribute XCAFDoc_Shape) and optional attributes like name or color links. You could ignore XCAF and use STEPControl_Reader to just get a TopoDS_Shape for the whole model, but you would lose the assembly hierarchy and color info. For example, STEPControl_Reader will give you either one big compound or multiple root compounds (you’d have to manually separate), and colors would be missing or only on the compound as vertex colors. Use the XDE approach (STEPCAFControl_Reader) for a robust CAD application, as it cleanly separates assembly instances and preserves attributes
dev.opencascade.org
.
Rendering the Imported Model: OCCT Visualization vs Custom OpenGL
Once the STEP model is in OCCT’s data structures, you have two main approaches to render it in your Qt/OpenGL application:
Approach 1: Use OCCT’s Visualization Engine (AIS and V3d)
OpenCASCADE has a built-in 3D viewer system which can greatly simplify showing CAD models with relatively little OpenGL coding on your part. This involves:
Creating an AIS_InteractiveContext (Application Interactive Services) and a V3d_Viewer/V3d_View.
Wrapping your shapes in AIS interactive objects (typically AIS_Shape or the specialized XCAFPrs_AISObject for assembly structures with colors).
Letting OCCT handle the rendering (it will do OpenGL calls internally to draw the triangulated geometry, edges, etc.).
How it works: OCCT’s viewer is essentially an OpenGL engine optimized for CAD. The V3d_Viewer is the scene manager (holds lights, global settings), V3d_View is a camera into the scene, and AIS_InteractiveContext manages interactive objects and selection. For each TopoDS_Shape you want to display, you create an AIS_Shape (or if using XCAF, you can use XCAFPrs_AISObject::SetShape(label) to get an AIS object that knows about sub-shape colors). Then you call context->Display(myAISShape, true) to add it to the scene. OCCT will internally tessellate the shape (using BRepMesh) and create GPU buffers. It will also draw edges and provide trihedron (axes) and so on if you enable them. Integration with Qt: Normally, OCCT opens its own window (on Windows uses WNT, on Mac Cocoa, etc.) for rendering. To integrate with Qt, you have two choices:
Native Window in a QWidget: You can create a QWidget and obtain its window handle, then pass that to OCCT’s Graphic3d_GraphicDriver/V3d_View::SetWindow(). For example, on macOS you would get an NSView* from the QWidget and create a Cocoa_Window for OCCT
stackoverflow.com
. This “old school” approach lets OCCT manage OpenGL completely in that area of the widget. However, it conflicts with Qt’s drawing (Qt may not know about OCCT’s GL calls), often requiring Qt::WA_PaintOnScreen and other hacks. It’s generally not the preferred method in Qt5/Qt6.
Use QOpenGLWidget and share context: This is the modern approach. You let Qt create an OpenGL context (with a QOpenGLWidget or QOpenGLWindow), and then bind OCCT’s viewer to that context so it draws into the same surface. OCCT’s Aspect_NeutralWindow can wrap an existing OpenGL context & surface.
stackoverflow.com
 In practice, you create the QOpenGLWidget, make sure it’s using an OpenGL Core Profile context (OCCT 7.9 supports core profiles), and in initializeGL() you create the OCCT viewer:
// inside QOpenGLWidget subclass:
void initializeGL() override {
    Handle(OpenGl_GraphicDriver) driver = new OpenGl_GraphicDriver(Handle(Aspect_DisplayConnection)());
    myViewer = new V3d_Viewer(driver);
    myView = myViewer->CreateView();
    // Wrap current Qt context:
    Handle(Aspect_NeutralWindow) occWindow = new Aspect_NeutralWindow();
    Aspect_RenderingContext ctx = (Aspect_RenderingContext) QOpenGLContext::currentContext()->nativeHandle();
    occWindow->SetNativeHandle(ctx); // Pseudocode: actually need to provide OS-specific handle or FBO
    myView->SetWindow(occWindow);
    if(!occWindow->IsMapped()) occWindow->Map();
    myContext = new AIS_InteractiveContext(myViewer);
}
The above is conceptual – the actual code is a bit intricate. The OCCT Qt sample repository provides a working example of using QOpenGLWidget to host an OCCT view
github.com
github.com
. In OCCT 7.9+, there are utilities (in OcctQtTools) to simplify sharing the context
github.com
. Essentially, you have to ensure OCCT uses the same OpenGL context that Qt created, otherwise you’ll see nothing or flickering
stackoverflow.com
stackoverflow.com
.
On macOS, note that Qt6 by default might use Metal via a layer – you’ll need to force an OpenGL surface (e.g. by setting QSurfaceFormat::setDefaultFormat with GL, since OCCT doesn’t render through Metal). Qt6 on macOS still supports OpenGL 4.1 (which you’re using).
Pros of AIS/V3d approach:
Minimal rendering code: OCCT handles the heavy lifting. It automatically tessellates shapes (with default or user-set deflection), updates them when you zoom (if using adaptive LOD), and draws with correct orientation and lighting. You avoid writing a mesh loader – just hand OCCT the TopoDS_Shape.
Built-in CAD features: AIS has support for selection and highlighting out of the box. For example, you can activate selection modes (vertex, edge, face) and get callbacks or query context->SelectedShape() easily
dev.opencascade.org
. It manages highlighting (e.g. hover vs selection have different colors) and you can customize those via Prs3d_Drawer.
Visual quality tweaks: OCCT viewer supports a shaded with edges mode by default. You can call context->SetDisplayMode(AIS_Shaded, Standard_True) and also enable edges (AIS_Drawer::SetDrawEdges(True)). It will draw nice anti-aliased edges over the shaded model. It also has options for hidden line removal, transparency, etc., all through its API.
Performance tuning: You can globally set the deviation (deflection) for AIS shapes. For instance, myAISShape->SetDeviationAngle() or SetDeviationCoefficient() to control tessellation density. OCCT also can automatically compute different Level-Of-Detail if you enable it (there’s an autoTriangulation feature; by default it might not reduce LOD on zoom out, but you can implement something).
Selection performance: OCCT uses BVH structures internally for picking. It can efficiently pick thousands of triangles via its StdSelect_BRepSelectionTool. This saves you from writing your own BVH for ray intersection.
Cons / Considerations of AIS/V3d:
Integration complexity: As noted, hooking into Qt’s rendering loop requires care. Many have faced issues where the OCCT content doesn’t display because the context wasn’t shared properly
stackoverflow.com
stackoverflow.com
. Using the official Qt sample as a template is advised. You’ll need to forward events (mouse, keyboard) to the AIS context (e.g. on mouse move, call AIS_ViewController::MoveTo(); on click, call Select() etc., if you use AIS_ViewController helper). This is extra work but mostly one-time setup.
Less control over OpenGL: If you want fancy custom OpenGL effects (shaders with PBR materials, custom draw styles), it’s harder with OCCT’s pipeline. You are somewhat limited to what AIS offers (though you can derive custom AIS_InteractiveObject for special rendering, but that’s advanced).
Dependency and Footprint: Bringing in OCCT’s visualization means you rely on its OpenGL state. If your app already has an OpenGL engine, you have to ensure state changes (glEnable, shaders, etc.) don’t conflict. Usually OCCT will set up its own state for each frame, so it’s manageable. Also OCCT viewer adds to your app’s footprint slightly (it’s part of OCCT libs you’d link).
When to favor AIS/V3d: If you want a quick path to get a working CAD viewer with selection, highlighting, and don’t need to heavily customize the rendering, this is a good approach. It’s especially useful if down the line you want to leverage OCCT’s tools like measuring distances, dynamical highlighting of sub-shapes, etc. Many CAD applications (e.g. FreeCAD, OCCT’s own CAD Assistant) use the AIS viewer for convenience.
Approach 2: Custom OpenGL Renderer (using OCCT for tessellation)
In this approach, you use OCCT only as a geometry kernel to load the STEP and produce mesh data, and then you render that data yourself via OpenGL. This fits well if you already have a rendering pipeline (camera control, shaders, etc.) and just need the CAD data as another mesh. Workflow: After importing the STEP into OCCT (using XCAF as above), you will:
Traverse the shapes (e.g. each free shape or each part).
For each shape, use OCCT’s meshing algorithm to generate a triangulation (Poly_Triangulation) of its faces.
Extract vertex coordinates, normals, and indices from the triangulations.
Feed those into your OpenGL VBOs/VAOs and render with your shaders.
The key class for meshing is BRepMesh_IncrementalMesh. This triangulates a TopoDS_Shape (or you can mesh face by face) given deflection parameters. For example:
TopoDS_Shape shape = ...;
Standard_Real linDeflection = 0.1; // e.g. 0.1 mm
Standard_Real angDeflection = 0.5; // 0.5 rad ~ 30 degrees
BRepMesh_IncrementalMesh mesher;
mesher.SetShape(shape);
IMeshTools_Parameters params;
params.Deflection = linDeflection;
params.Angle = angDeflection;
params.Relative = Standard_False;   // False = deflection is absolute
params.InParallel = Standard_True;  // use multi-threading
mesher.ChangeParameters() = params;
mesher.Perform();
This will mesh the entire shape (all faces) in parallel
unlimited3d.wordpress.com
. The linear deflection is the maximum distance between the CAD surface and the mesh. Angular deflection limits the angle between adjacent triangle normals
stackoverflow.com
. Using relative deflection means the linear deflection is taken as a fraction of each object’s bounding box (so you get finer mesh on big objects automatically) – OCCT’s default for BRepMesh_IncrementalMesh(shape, defl, **relative**, ang) is often to set relative=true for convenience. You can decide; absolute deflection gives consistent accuracy across parts of similar scale, whereas relative = true will mesh a very large object more coarsely than a small one with the same deflection value
dev.opencascade.org
 (since deflection = defl * boundingBoxDiameter, internally
dev.opencascade.org
). Once meshed, OCCT stores the triangulation on each face of the shape. You retrieve it like:
for(TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
    TopoDS_Face face = TopoDS::Face(exp.Current());
    TopLoc_Location loc;
    Handle(Poly_Triangulation) poly = BRep_Tool::Triangulation(face, loc);
    if(poly.IsNull()) continue;
    // Get mesh data
    const TColgp_Array1OfPnt& nodes = poly->Nodes();
    const Poly_Array1OfTriangle& triangles = poly->Triangles();
    gp_Trsf tr = loc.Transformation(); // face location (assembly transform)
    for(int n = nodes.Lower(); n <= nodes.Upper(); ++n) {
        gp_Pnt p = nodes(n).Transformed(tr);
        meshVertices.push_back( (float)p.X() );
        meshVertices.push_back( (float)p.Y() );
        meshVertices.push_back( (float)p.Z() );
    }
    for(int t = triangles.Lower(); t <= triangles.Upper(); ++t) {
        int n1,n2,n3;
        triangles(t).Get(n1,n2,n3);
        // Poly_Triangles are 1-indexed
        meshIndices.push_back(n1 - 1 + vertexOffset);
        meshIndices.push_back(n2 - 1 + vertexOffset);
        meshIndices.push_back(n3 - 1 + vertexOffset);
    }
}
This is a simplified extraction. A few notes:
We applied the TopLoc_Location transform to each node
unlimited3d.wordpress.com
 so that if the face (or part) was positioned in an assembly, we get the correct world coordinates. You may instead accumulate a part’s transform at a higher level and apply it to all its faces’ nodes; just don’t forget it, or parts in assemblies will all overlap at the origin.
Normals: Poly_Triangulation can contain normals. Check poly->HasNormals(). If true, you can call poly->Normal(nodeIndex). Often, however, OCCT does not automatically compute per-vertex normals during BRepMesh (it can, but you might need to enable a flag). If normals are missing, you have options:
Compute a flat normal for each triangle via cross-product (and duplicate it for the 3 vertices). This is easy but gives faceted appearance.
Compute smooth normals per vertex by averaging adjacent face normals. You’d need connectivity info: e.g., build a map from vertex->list of connected faces, then average. This can be done since you have the triangles.
Use the face’s surface to get exact normals: for each node’s UV on the face, evaluate the surface normal. OCCT provides BRepAdaptor_Surface surface(face, Standard_True) and surface.Normal(U,V). But you need UV coords of mesh nodes; Poly_Triangulation does have UV nodes (poly->UVNodes() if the face has a parametrization). This is the most accurate smooth normal, but computing it for every vertex is more involved.
For a high-quality CAD viewer, smooth normals are desirable on curved faces (so cylinders look round). OCCT’s AIS does this internally. For a custom pipeline, a reasonable approach is average normals: it’s simpler than per-vertex surface eval and usually looks fine if mesh is reasonably fine. Inverted normals: If you find normals pointing inward (e.g., a solid looks inside-out under lighting), check the face orientation. OCCT’s face orientation might be reversed but the triangulation normal doesn’t reflect it. A quick fix: you can flip the normal by face orientation. Or use BRepTools::OrientClosedSolid on the whole shape before meshing, which orients faces consistently outward.
Color per face: Since we can have different colors on faces, you might want to store a color array per triangle or per vertex. The XCAF color tool can tell you the face’s color before you add its geometry. For example, in the loop above, before pushing triangles, do:
Quantity_Color col;
bool faceColored = colorTool->GetColor(face, XCAFDoc_ColorSurf, col);
RGBA faceColor;
if(faceColored) faceColor = { (float)col.Red(), (float)col.Green(), (float)col.Blue(), 1.0f };
else faceColor = partColor; // use part’s color or default
// Assign this color to all vertices of this face’s triangles...
One way is to have a parallel vector of colors per vertex index. If you don’t want per-vertex color interpolation (CAD usually doesn’t interpolate face colors – each face is solid color), you can duplicate vertices per face so that each face’s triangle vertices all have the same color in your shader. That increases vertex count but is simpler. Alternatively, you can use a flat-shading shader that takes a uniform color per draw-call, but then you’d need to draw each face separately, which is inefficient. Better is a per-vertex color attribute and use flat coloring in the shader (GLSL flat qualifier on the color input will use the first vertex’s color for the whole triangle, achieving a uniform face color).
Edges (wireframe overlay): In CAD, we often draw silhouette or boundary edges over the shaded model. With your own renderer, you have to derive these. There are a couple of strategies:
Extract edges from the BRep: Iterate over TopExp_Explorer(shape, TopAbs_EDGE) and discretize each edge curve into line segments. OCCT can give a polygon for each edge corresponding to the triangulation: Handle(Poly_PolygonOnTriangulation) polyline = BRep_Tool::PolygonOnTriangulation(edge, face, loc) gives points along the edge on a particular face
unlimited3d.wordpress.com
. But it might be easier to directly sample the 3D curve. You can use BRep_Tool::Curve(edge, first, last) to get a Geom_Curve and then sample it (e.g. via GCPnts_UniformAbscissa or GCPnts_TangentialDeflection for adaptive). The sampled polyline can be stored as a series of GL line segments. Do this once per edge (avoid duplicating for two faces). You might also use the existing triangulation: the mesh’s boundary edges are those edges that belong to only one face or where two adjacent faces have a crease above some angle. Deriving silhouette dynamically (edges visible due to viewpoint) is more complex, so likely you’ll draw all boundary/feature edges in black.
After you have line vertex arrays for edges, you can render them on top of the model. To avoid z-fighting with the faces, enable polygon offset or depth bias when drawing either the faces or the lines. Commonly, one renders filled polygons normally, then enables glPolygonOffset with a small factor to push depth slightly, and renders the edges (lines) with the offset or with depthfunc=LEQUAL. Another trick is to draw edges in a second pass with the depth buffer from first pass but slightly adjust projection (not as straightforward). The simplest is glEnable(GL_POLYGON_OFFSET_FILL) for the face pass with e.g. glPolygonOffset(1.0, 1.0) to push them back a bit, so that when you draw lines (with polygon offset off), the lines appear on top without fighting. Fine-tune the offset to avoid making edges “float” too far forward.
OpenGL specifics: Use modern buffer techniques – combine your mesh vertices and indices into large VBOs if possible. Each part or assembly can be one VAO with its buffers. You might group the whole model into one VAO if the material is uniform, but here each part could have its own color. Typically, CAD models don’t have complex materials, so you might just have a simple shader with uniform or vertex color and Phong lighting.
Performance considerations (custom):
Tessellation quality vs speed: A finer linear deflection (small number) can explode the triangle count. For large assemblies, choose a reasonable default deflection (e.g. 0.1% of model bounding box size as absolute, or use relative= true with deflection ~0.001). You might also set an angular deflection ~ 20-30°; this allows coarse tessellation on gentle curves but finer on tight bends
unlimited3d.wordpress.com
. For very large models, you may allow a larger deflection (less detail) to keep performance interactive. It’s common to let users toggle a “Low/High quality” which just remeshes with a different deflection.
Mesh caching: Avoid re-meshing the same shape repeatedly. Once you’ve called BRepMesh_IncrementalMesh on a shape, the Poly_Triangulation is stored in it. Unless you call mesher again with a finer deflection, you can reuse it. If memory usage is a concern, you can drop triangulations via BRepTools::Clean(shape) to free memory, then re-mesh later if needed. A strategy for big assemblies is to mesh on-demand: e.g., only mesh what’s visible or what’s needed for display, especially if you support loading without immediately displaying every tiny screw. In an interactive app, you could start with a coarse mesh for everything, then refine in the background for parts the camera is close to (this is a complex LOD scheme, not trivial, but just note the possibility).
Batching and state changes: Each draw call has overhead. If you treat every part as a separate draw, thousands of parts might hurt performance. If they share the same shader and only color varies, consider using a single mesh buffer for many parts with a color per face or per part ID. However, merging everything makes highlighting a single part harder. A middle ground: group static sub-assemblies into one VBO. Also frustum culling – if your model is extremely large (in part count), implement a simple bounding-box cull per part/assembly to skip drawing those off-screen.
Acceleration for picking: Unlike AIS which has selection built-in, with custom rendering you need to do hit-testing. For picking one might:
Ray cast against the triangles. Brute force is O(n) per pick which can be slow if millions of tris. Better, build a BVH (Bounding Volume Hierarchy) on all triangles or on part BBoxes. OCCT actually provides BVH_Tree and BVH_LinearSorter that you could potentially use with the triangulations. Or use a library like Intel Embree for fast ray-tri intersections if you need very robust picking.
Alternatively, implement a color picking: render the scene in an offscreen buffer with each face (or part) assigned a unique flat color ID, then just read the pixel on click. This is often simpler for CAD models if you only need to pick whole parts. If per-face selection is needed, you could even encode face IDs. Just be mindful of the rendering cost and coordinate the ID<->object mapping.
Pros of Custom Rendering:
Full control: You can use your own shaders for advanced effects (e.g. real-time SSAO, custom lighting, etc. not available in OCCT’s engine).
Easy Qt integration: You simply use QOpenGLWidget’s normal paintGL – no special integration layers. OCCT is used only as a data source, so it won’t conflict with Qt’s GL state machine.
Potential performance optimizations: You can optimize for your specific use case (instance drawing for repeating bolts, occlusion culling, etc.) beyond what OCCT’s generic engine does.
Cons:
You must implement features like selection, highlighting, explode view, etc. that AIS would give for free. This means writing more code (for example, highlighting an object on hover: you’d need to detect it via picking and then maybe draw it with a different shader or outline).
Maintain the mesh validity: If geometry changes or user modifies the model (less likely in just a viewer scenario), you’d need to remesh and update buffers accordingly.
Initial development time: Getting all the details right (normals, edge drawing, transparency sorting if needed, etc.) is non-trivial. With AIS, a lot of that is already solved (e.g. AIS even has an built-in hidden line removal mode, etc.).
When to favor Custom OpenGL: If you already have an OpenGL framework or you need maximum flexibility in rendering (for example, integrating with game-engine-like visuals), this approach is suitable. It’s also beneficial if OCCT’s viewer has compatibility issues on your target (though OCCT’s is pretty cross-platform). Since you mentioned you have a “Qt6 + OpenGL core profile” viewport working, you might lean this way to avoid wrestling with OCCT’s context integration. Many smaller CAD viewers opt for custom rendering for this reason.
Comparison and Recommendation: AIS vs Custom
In summary, both approaches can achieve high-quality rendering of STEP data, but they serve different developer needs:
OCCT AIS/V3d approach is quick to get working CAD features – you’ll get correct shading, fast selection, and decent performance out-of-the-box. It’s ideal if you want a solid CAD viewer with minimal fuss, and you accept the “OCCT way” of doing things (which can limit some customization). The downside is the integration overhead (especially in Qt) and somewhat of a black box in terms of OpenGL (though you can still access low-level structures if needed).
Custom OpenGL approach requires more upfront coding, but you retain full control. You can tune the mesh processing, implement selection in a way that suits your app, and integrate seamlessly with your UI (Qt) since it’s just another GL draw routine. The risk is reimplementing things like highlight logic or cross-section displays that OCCT already has.
Recommendation: Given you already have a working OpenGL viewport and presumably are comfortable with GL, a Custom OpenGL pipeline may be the path of least resistance for rendering. It lets you use your existing camera control, and just feed in the mesh from OCCT. However, if down the line you need robust selection, infinite small features, etc., you might consider switching to AIS. A hybrid approach is also possible: use OCCT to get triangulations and maybe use some of its algorithms (like selection algorithms) without using AIS fully – but that can be complex. For a first implementation, you could start with custom rendering for simplicity, and keep in mind that AIS is an option if you hit limitations. Since your focus is on STEP workflows (likely displaying and maybe inspecting models, not heavy editing), custom rendering with carefully chosen tessellation should serve well. In contrast, if tomorrow you needed to support interactive modifications or very advanced picking, AIS might begin to shine. In the end, both are viable – many successful apps use AIS (e.g. FreeCAD uses OCCT viewer) and many use custom (e.g. some in-house viewers, game engines integrating CAD data).
“Correct” Rendering Best Practices
Whether you use AIS or custom, there are certain CAD rendering requirements to get a nice output:
Shaded + Edges Mode: This is the de-facto CAD view (sometimes called “Illustration” mode). Ensure that edges of parts are visible. With AIS, just enabling edge drawing is enough. With custom GL, as discussed, draw edges as overlay lines. Typically color edges in a contrasting color (black or dark gray) regardless of face color. You might also draw silhouette lines (outlines of the whole shape) for emphasis – this requires identifying outer boundary edges relative to view, which can be done via normal angles or using a shader edge detection. It’s an enhancement on top of basic edges.
Normals and Lighting: Use smooth normals for curved surfaces to avoid a faceted look. Make sure normal direction is consistent. If using custom GL, consider enabling two-sided lighting or drawing back-faces with a dimmer color if the model has thin shells. In CAD, usually back-face culling is turned off because you might have open shells or want to see interior if cut away. OCCT’s viewer by default does not cull back faces (so even if a surface’s normal is away from camera, it still draws – this avoids disappearing surfaces if a solid is accidentally inside-out or a sheet is single-sided). You can mimic this by either disabling culling or by drawing both sides in the shader. If you do enable backface culling (for performance), be absolutely sure all faces are oriented properly, otherwise you’ll see holes.
Depth precision: Large CAD models (extents of many kilometers) combined with very small features can strain depth buffer precision, causing z-fighting even when geometry is correctly modeled. On desktop GL with 24-bit depth, this is usually okay up to 10^5:1 ratio in scale. If you find z-fighting on coplanar faces (e.g. two surfaces coincident), that’s a modeling issue typically. But if it’s on edges vs faces, handle via polygon offset as noted. For very large scenes, use a logarithmic depth buffer or adjust the camera’s near/far planes dynamically. A good practice is to set the camera near plane as far out as possible (not too close to the camera) to maximize depth resolution on the model. If you have control, you could compute the model’s bounding box and set near = far/10000 or something heuristic. Qt’s camera (if you roll your own) should allow that. OCCT’s V3d has methods to set z-range or uses a default algorithm.
Fit to view: After loading, implement a “zoom extents” to show the whole model. Calculate the model bounding box (OCCT provides Bnd_Box via BRepBndLib::Add(shape, box) easily). Then position your camera target at the center of this box and set the camera distance such that the box fits in the frustum. If using perspective, you set distance = boxDiag/ (2 * tan(fov/2)). If using orthographic, set the zoom such that the largest dimension of the box fits in view. In OCCT’s viewer you just call V3d_View::FitAll()
unlimited3d.wordpress.com
. In Qt with custom GL, you’ll do similar calculations manually.
Highlighting selection: To mimic CAD behavior, when user hovers or clicks a part, draw it highlighted (e.g. yellow outline or translucent overlay). With AIS, this is a one-liner (context->HilightWithColor or SetSelected which changes color)
dev.opencascade.org
. With custom, you’d need to either change the part’s color in the VBO or draw an extra highlight effect (like draw its silhouette in bright color). A quick trick: you can keep an “highlighted object id” and in your fragment shader, if the vertex belongs to that id, mix in a highlight color. Or simpler, re-render the highlighted object with a wide GL_LINE polygon mode (to create an outline) in a bright color. Many options here; the goal is to give user feedback.
Transparency and ordering: If you will support transparent parts, remember to sort by depth and render transparents after opaque. OCCT handles this internally by sorting interactive objects by layer. In a custom pipeline, you’d sort your triangles or parts (at least sort parts by depth and use alpha blending). For correctness, you might need per-triangle sorting for complex overlap, or use techniques like depth-peeling. This only matters if you need semi-transparent components.
Performance tuning for big assemblies: For huge models (millions of triangles or thousands of parts), consider:
Using glDrawElements with indexed vertices (avoid duplicate vertices in VBO if smooth shading allows it – except where you need distinct normals).
Leverage instancing if there are repeated parts: if an assembly uses the same part multiple times (STEP often does), you can store one mesh and draw it with different instance transforms. Since XCAF gives you separate shapes for repeated parts, you’d have to detect identical geometry. XCAF does have an ID for shape definitions vs instances
github.com
 (you can check if two labels refer to same shape def). This is advanced but can save memory if say 100 bolts are identical – draw one bolt mesh 100 times with different matrices.
Use multi-threading for load: The STEP reading is already separate from rendering. You can further mesh parts in parallel using TBB (OCCT uses it internally if you set InParallel). Also you can populate VBOs on background threads, then upload to GPU (still needs context, so usually do uploads in main thread). Qt’s render loop can be tied to main thread, so schedule heavy computations (like meshing 1000 faces) off-thread to keep UI responsive
github.com
.
In practice, start simple: mesh everything with a moderate deflection, draw all opaque, and iterate optimizations as needed.
Failure Modes & Debugging Guide
Even with the best pipeline, things can go wrong. Here’s a playbook for common issues:
Model is invisible or extremely small/large: Likely a unit conversion issue. For example, you load a model supposed to be in mm, but it’s 1000× smaller (meters). Check the STEP file’s unit (look for SI_UNIT(.MILLI.,.METRE.) in the STEP text or use OCCT’s XCAFDoc_DocumentTool::GetLengthUnit)
discourse.salome-platform.org
. If the conversion is wrong, adjust the import: e.g. call Interface_Static::SetCVal("xstep.cascade.unit","M") before reading to force meters, or "MM" for millimeters. In OCCT 7.6+, use XCAFDoc_DocumentTool::SetWorldwideLengthUnit(doc, UnitsMethods_LengthUnit_Millimeter) appropriately. Also verify your rendering camera scaling – if the object is technically there but your camera near/far or field of view is off, it might not show. Use bounding box and zoom to extents to confirm the object’s size and position.
All faces are dark/black (lighting issues): This could be normals pointing inward or no normals at all. Use a test: enable two-sided lighting (or temporarily disable backface culling) – if the inside of the model is lit, your normals are reversed. To fix normals in OCCT, you can flip the face orientations or simply invert the normal vectors in your buffer. If only some faces are dark, they might have inconsistent orientation relative to others. Running ShapeFix_Solid or BRepLib::OrientClosedSolid on the shape before meshing can unify normals orientation. If using AIS, you can also ask it to recompute normals by increasing deflection or using GProp surface normal calculations. Also check if your light is placed correctly (e.g., if using a directional light coming from same side as camera, and you accidentally have backface culling on reversed faces, they will all vanish or be unlit).
Jagged or faceted curved surfaces: This means tessellation is too coarse. Decrease the linear deflection (or angular deflection). For example, a sphere looking like a dodecahedron -> use smaller deflection (and ensure Relative was correctly set if used). Conversely, if performance is slow, increase deflection to reduce triangles.
Missing faces or holes: If entire faces are missing, possibly the mesher failed on those (e.g., extremely small or problematic surfaces). Check if BRep_Tool::Triangulation returned null for some faces. If so, you can attempt a re-mesh with a larger deflection (oddly, very small deflection on a tiny face might fail due to precision limits). If the shape is open (non-solid), missing faces might actually be because they weren’t in the STEP (e.g., one surface of an open shell). To diagnose, run OCCT’s shape checker:
BRepCheck_Analyzer ana(shape);
if(!ana.IsValid()) { /* iterate over ana to see faulty faces/edges */ }
Often, BRepCheck_NotClosed will appear for shells. If slight gaps cause holes, try sewing:
BRepBuilderAPI_Sewing sewing;
sewing.Add(shape);
sewing.Perform();
shape = sewing.SewedShape();
This can close gaps under a tolerance. But note, sewing might modify topology (faces merge, etc.), losing correspondence with original indices or metadata.
Colors not showing or all one color: If you expected colors, ensure you used STEPCAFControl_Reader and transferred into an XCAF doc. If you accidentally used STEPControl_Reader, you won’t get colors (OCCT’s plain STEP reader ignores color). Also ensure you apply the colors in rendering: if using AIS, use XCAFPrs_AISObject or call XCAFPrs::SetColours(context, doc) which assigns colors to shapes in the AIS context. In custom, double-check your color assignment logic. If a part has no face-specific color, you should use its part color. Also, check color spaces – OCCT’s Quantity_Color is in RGB [0,1]. If you interpret as 0-255 without conversion, colors will appear black (because 0.2 interpreted as 0/255). So be sure to multiply by 255 if needed when feeding to an 8-bit color structure.
Performance is poor (low FPS): Profile where the time is spent. If it’s CPU during import, consider that XCAF import and meshing are heavy – try loading in a background thread, then only rendering in the GUI thread when ready
github.com
. If it’s during rendering (GPU bound), check triangle count and fillrate. Perhaps you have too fine a mesh (try a coarser mesh to see if FPS improves). If the bottleneck is draw calls (CPU overhead), batch more geometry together. Also, ensure you’re using indexed drawing and not duplicating vertices excessively. For AIS, you can experiment with context->SetAutoHilight(false) and other settings that might reduce overhead if you don’t need selection. Also cull objects outside view if you manage your own scene graph.
Memory usage is high: CAD models can be memory hogs, especially with both B-Rep and triangulation in memory. XCAF will keep the B-Rep and all metadata. If you only need the mesh, you could free the B-Rep after meshing (but then picking & metadata linking becomes harder). OCCT’s triangulations can also use a lot of RAM for millions of triangles. If you need to reduce memory:
Use a slightly higher deflection to reduce triangle count.
If you know you won’t re-tessellate, you can call BRepTools::Clean(shape) after extracting the Poly_Triangulation. This removes the triangulation from the shape (freeing memory) – but then you’re responsible for keeping the mesh data you need.
Free unused parts: If the assembly has optional components you’re not showing, you can remove them (XCAFDoc_ShapeTool::RemoveShape) from the document to free memory.
Ensure you’re not accidentally storing multiple copies of geometry (like if you copy TopoDS_Shapes, they are handles to same internal data, so that’s fine. But if you clone shapes unnecessarily, that duplicates).
Crashes or exceptions on STEP import: This can happen with malformed files. OCCT’s reader might throw a Standard_Failure. That’s why the example wraps Transfer() in a try/catch
unlimited3d.wordpress.com
unlimited3d.wordpress.com
. Make sure to catch exceptions around STEP reading to avoid hard crashes. If a particular file crashes consistently, try updating OCCT if possible (7.9.1 is recent, but there are always bug fixes). You can also enable OCCT messages to see where it fails (Message::DefaultMessenger() to console).
In general, use OCCT’s built-in logging: call OSD::SetSignal(false) to let it catch fatal signals, and perhaps redirect the log (Message_Printer). This can give clues if something goes wrong deep in the kernel.
Minimal Implementation Outline (Pseudo-code)
Finally, let’s outline how all this comes together in a simplified C++ pseudo-code. This will assume we go with Approach 2 (Custom Rendering) for clarity, but you can adapt for AIS if needed:
// Pseudo-code structures for clarity (not full declarations)
struct MeshData { 
    std::vector<float> vertices; 
    std::vector<float> normals; 
    std::vector<float> colors;
    std::vector<uint32_t> indices;
};

struct Model {
    Handle(TDocStd_Document) doc;
    Handle(XCAFDoc_ShapeTool) shapeTool;
    Handle(XCAFDoc_ColorTool) colorTool;
    std::vector<MeshData> meshes;  // one mesh per part (or assembly node)
    std::vector<glm::mat4> transforms; // instance transforms for each mesh (identity if already applied)
};

// STEP Import function
Model ImportSTEP(const std::string& filePath) {
    Model model;
    // Initialize XDE document
    Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
    BinXCAFDrivers::DefineFormat(app);
    app->NewDocument("BinXCAF", model.doc);
    // Read STEP
    STEPCAFControl_Controller::Init();
    STEPControl_Controller::Init();
    STEPCAFControl_Reader reader;
    IFSelect_ReturnStatus stat = reader.ReadFile(filePath.c_str());
    if(stat != IFSelect_RetDone) {
        throw std::runtime_error("STEP read failed");
    }
    if(!reader.Transfer(model.doc)) {
        throw std::runtime_error("STEP transfer failed (no data)");
    }
    // Get tools
    model.shapeTool = XCAFDoc_DocumentTool::ShapeTool(model.doc->Main());
    model.colorTool = XCAFDoc_DocumentTool::ColorTool(model.doc->Main());
    return model;
}

// Build renderable mesh data (tessellate shapes)
void TessellateAll(Model& model, double linDef=0.1, double angDef=M_PI/6, bool relative=false) {
    TDF_LabelSequence roots;
    model.shapeTool->GetFreeShapes(roots);
    for(int i=1; i<= roots.Length(); ++i) {
        TDF_Label rootLabel = roots.Value(i);
        TopoDS_Shape shape = model.shapeTool->GetShape(rootLabel);
        // Optionally skip empty shapes
        if(shape.IsNull()) continue;
        // Mesh the shape (could also mesh each face in loop, but meshing whole shape uses parallelism)
        BRepMesh_IncrementalMesh(shape, linDef, relative, angDef, true);
        // Extract mesh data
        MeshData mesh;
        Quantity_Color partColor;
        bool partHasColor = model.colorTool->GetColor(shape, XCAFDoc_ColorSurf, partColor);
        gp_Trsf loc; // identity for free shape (free shapes usually have no parent transform)
        // If rootLabel is an assembly, you'll iterate sub-shapes; but here we flatten everything:
        TopExp_Explorer exp;
        for(exp.Init(shape, TopAbs_FACE); exp.More(); exp.Next()) {
            TopoDS_Face face = TopoDS::Face(exp.Current());
            TopLoc_Location location;
            Handle(Poly_Triangulation) poly = BRep_Tool::Triangulation(face, location);
            if(poly.IsNull()) continue;
            // Combined transformation: location (if any) and parent loc (none at root here)
            gp_Trsf tr = loc * location.Transformation();
            // If the part is an instance with its own transform, include that in loc.
            // (For free shape, loc is identity.)
            int vertOffset = mesh.vertices.size() / 3;
            const TColgp_Array1OfPnt& nodes = poly->Nodes();
            const Poly_Array1OfTriangle& tris = poly->Triangles();
            // Normals array exists?
            bool hasNormals = poly->HasNormals();
            for(int n = nodes.Lower(); n <= nodes.Upper(); ++n) {
                gp_Pnt p = nodes(n).Transformed(tr);
                mesh.vertices.push_back((float)p.X());
                mesh.vertices.push_back((float)p.Y());
                mesh.vertices.push_back((float)p.Z());
                if(hasNormals) {
                    gp_Vec vn = poly->Normal(n);
                    vn.Transform(tr);
                    vn.Normalize();
                    mesh.normals.push_back((float)vn.X());
                    mesh.normals.push_back((float)vn.Y());
                    mesh.normals.push_back((float)vn.Z());
                }
                // Assign color per vertex (flat by face will be handled in shader or by duplicating vertices per face)
                Quantity_Color col;
                if(model.colorTool->GetColor(face, XCAFDoc_ColorSurf, col)) {
                    mesh.colors.push_back((float)col.Red());
                    mesh.colors.push_back((float)col.Green());
                    mesh.colors.push_back((float)col.Blue());
                    mesh.colors.push_back(1.0f);
                } else if(partHasColor) {
                    mesh.colors.push_back((float)partColor.Red());
                    mesh.colors.push_back((float)partColor.Green());
                    mesh.colors.push_back((float)partColor.Blue());
                    mesh.colors.push_back(1.0f);
                } else {
                    // default gray
                    mesh.colors.push_back(0.8f); mesh.colors.push_back(0.8f);
                    mesh.colors.push_back(0.8f); mesh.colors.push_back(1.0f);
                }
            }
            for(int t = tris.Lower(); t <= tris.Upper(); ++t) {
                int v1,v2,v3;
                tris(t).Get(v1,v2,v3);
                mesh.indices.push_back(vertOffset + v1 - 1);
                mesh.indices.push_back(vertOffset + v2 - 1);
                mesh.indices.push_back(vertOffset + v3 - 1);
            }
            // (Optional: compute normals if not provided, here we’d do flat normals by triangle or accumulate for smooth.)
        }
        model.meshes.push_back(std::move(mesh));
        model.transforms.push_back(glm::mat4(1.0f)); // identity matrix for root shape
    }
}
The above pseudo-code does a flat list of meshes for each free shape. If you wanted assembly hierarchy, you would recurse into assembly labels, accumulate transformations, and perhaps create one mesh per leaf part with an instance transform. For example, if root is an assembly containing part A twice, you’d mesh part A once, store its mesh, and have two transform matrices for the two instances. XCAF provides ways to detect that (by checking if a sub-label is an instance referencing another label’s shape). Implementing that fully is more complex, so the above flattening works but duplicates geometry for repeated parts. Depending on model complexity, duplication might be acceptable or not. Rendering (Qt/OpenGL): In your QOpenGLWidget’s paintGL():
glEnable(GL_DEPTH_TEST);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
// Set camera projection * view matrix (from your camera class)
glm::mat4 MVP = camera.projection * camera.view;
shader.use();
shader.setUniform("uMVP", MVP);
shader.setUniform("uModelMat", glm::mat4(1.0f)); // if already in world coords
for(size_t i = 0; i < model.meshes.size(); ++i) {
    // if using instancing and repeated parts, you’d draw instanced here
    MeshData& mesh = model.meshes[i];
    // bind VAO for this mesh (assuming you created one and uploaded data)
    glBindVertexArray(mesh.vao);
    // Draw faces
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawElements(GL_TRIANGLES, mesh.indicesCount, GL_UNSIGNED_INT, 0);
    // Draw edges overlay
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    // (Option 1: draw wireframe via glPolygonMode)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.0f);
    edgeShader.use();
    edgeShader.setUniform("uMVP", MVP);
    edgeShader.setUniform("uColor", glm::vec3(0,0,0));
    glDrawElements(GL_TRIANGLES, mesh.indicesCount, GL_UNSIGNED_INT, 0);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
This simplistic approach draws all triangle edges (including diagonals) as wireframe. A more refined way is to have an edge index list (or a separate GL_LINE loop over unique edges). But even drawing all edges in wireframe often looks acceptable for visualization, aside from seeing triangulation lines on curved surfaces. If that’s undesirable, generating true edges as discussed is the remedy. Qt event handling: In your QOpenGLWidget or window, you’d map mouse events to camera moves or picking:
For rotating/panning, use your existing camera logic.
For selection, on mouse click, you can do the color-pick or ray intersection. If ray, you can use OCCT’s BRepExtrema_DistShapeShape or BVH classes to help, or brute force test triangle by triangle (with BVH optimization ideally). If color-pick, render an ID buffer as described.
Qt integration notes: Ensure your QOpenGLWidget has format set to CoreProfile 4.1 (for macOS). Since you specify core, use modern GL calls (no fixed pipeline). OCCT’s triangulation just gives data; it’s up to your GL code to use VAOs, VBOs, and GLSL. The pseudo above assumes you have a compiled shader program with at least a VS/FS that handles position, normal, color. For example, a simple fragment shader might do vec3 color = flat ? inColor (with flat qualifier) : (inColor * (ambient + diffuse*dot(N,L) + specular*...)). Testing on Mac: Mac’s OpenGL can be finicky with line widths >1 (many GPUs only support width=1 in core). So line drawing might not be very thick – consider drawing edges as thin lines but you can do a screen-space stroke if needed for emphasis.
By following this structured approach – robust STEP import via OCCT XDE, and a carefully implemented rendering pipeline (either via OCCT’s viewer or your custom OpenGL), you can reliably bring STEP models into your Qt application and display them with high quality. Start with a few sample STEP files (one simple, one with an assembly and colors, one large) to validate the pipeline. Use the OCCT references and tools (like Draw Harness for quick experiments) to troubleshoot issues as they arise. With the combination of OCCT’s powerful data translation and your control over rendering, you’ll have a solid CAD viewing capability in your application. Key References (by topic):
STEP Application Protocols & Data:
Capvidia, “Best STEP File to Use: AP203, AP214, and AP242” – differences in protocols
techsupport.sds2.com
SDS/2 Tech Support, “Export Model – STEP” (on AP203 vs AP214 vs AP242)
techsupport.sds2.com
Steptools.com, “STEP Application Protocols” – overview of APs and scope
steptools.com
steptools.com
OpenCASCADE XDE (STEP Import) Usage:
OCCT Official Documentation: XDE User’s Guide (occt_user_guides__xde) – details on XCAF document structure, assemblies, colors
github.com
old.opencascade.com
OCCT GitHub Wiki (deprecated but useful), “Extended Data Exchange (XDE)” – code examples for reading STEP into XCAF
github.com
Open CASCADE Forum: “Work with assemblies” – example of using STEPCAFControl_Reader and XCAF tools for assembly access
dev.opencascade.org
dev.opencascade.org
Unlimited3D (K. Gavrilov’s blog), “Colored STEP model in OCCT Viewer” – detailed tutorial on using STEPCAFControl_Reader, XCAFDoc tools, preserving colors
unlimited3d.wordpress.com
unlimited3d.wordpress.com
OCCT Visualization (AIS) and Qt Integration:
OCCT Official Documentation: Visualization User’s Guide – covers AIS, interactive context, selection (old version on OpenCascade.com)
dev.opencascade.org
GitHub – occt-samples-qt by gkv311 – Qt samples for OCCT (especially occt-qopenglwidget)
github.com
github.com
, showing how to integrate V3d_View with QOpenGLWidget
StackOverflow: “How to visualize OpenCascade on QOpenGLWidget (Qt6)” – discusses context sharing attempts and solutions
stackoverflow.com
stackoverflow.com
Tessellation and Custom Rendering:
OCCT Doxygen: BRepMesh_IncrementalMesh class reference – usage of deflection parameters
stackoverflow.com
Unlimited3D blog, “BRepMesh intro” – excellent overview of OCCT’s meshing process, deflection, and code example
unlimited3d.wordpress.com
unlimited3d.wordpress.com
StackOverflow: “What is linear/angular deflection in OCC?” – definition of deflection parameters
stackoverflow.com
OCCT Doxygen: XCAFDoc_ColorTool – methods to get colors from shapes
dev.opencascade.org
 (color assignment in XDE)
OCCT Forum: “STEP unit conversion and meshing” – notes on unit handling and precision (Kirill Gavrilov’s comments)
dev.opencascade.org
dev.opencascade.org
Troubleshooting & Performance:
SALOME Forum: “Units on STEP import” – explanation of OCCT default units (mm vs scaling to SI)
discourse.salome-platform.org
OCCT Forum: “STEPCAFControl_Reader thread safety” – indicates using newest OCCT for stability (if relevant to multi-threading)
Unlimited3D blog, “OCCT Viewer Common Tips” – (in occt-samples-qt readme) covers using background threads for heavy operations
github.com
 and other best practices.
OpenCascade forum, “Can not get colors of faces” – discussions confirming usage of XCAFDoc_ColorTool for face colors.
Citácie

Export Model - STEP

https://techsupport.sds2.com/sds2_2023/Topics/step_ex.htm

BRepMesh intro – Unlimited 3D

https://unlimited3d.wordpress.com/2024/03/17/brepmesh-intro/

BRepMesh intro – Unlimited 3D

https://unlimited3d.wordpress.com/2024/03/17/brepmesh-intro/

Units on STEP import - Use of the platform - SALOME PROJECT

https://discourse.salome-platform.org/t/units-on-step-import/564

Units on STEP import - Use of the platform - SALOME PROJECT

https://discourse.salome-platform.org/t/units-on-step-import/564

STEP unit conversion and meshing - Forum Open Cascade Technology

https://dev.opencascade.org/content/step-unit-conversion-and-meshing

xde · Open-Cascade-SAS/OCCT Wiki · GitHub

https://github.com/Open-Cascade-SAS/OCCT/wiki/xde

xde · Open-Cascade-SAS/OCCT Wiki · GitHub

https://github.com/Open-Cascade-SAS/OCCT/wiki/xde

xde · Open-Cascade-SAS/OCCT Wiki · GitHub

https://github.com/Open-Cascade-SAS/OCCT/wiki/xde

Open CASCADE Technology: Extended Data Exchange (XDE)

https://old.opencascade.com/doc/occt-7.5.0/overview/html/occt_user_guides__xde.html

Work with assemblies - Forum Open Cascade Technology

https://dev.opencascade.org/content/work-assemblies

Open CASCADE Technology: Extended Data Exchange (XDE)

https://old.opencascade.com/doc/occt-7.5.0/overview/html/occt_user_guides__xde.html

Open CASCADE Technology: Extended Data Exchange (XDE)

https://old.opencascade.com/doc/occt-7.5.0/overview/html/occt_user_guides__xde.html

Open CASCADE Technology: STEPCAFControl_Reader Class Reference

https://dev.opencascade.org/doc/refman/html/class_s_t_e_p_c_a_f_control___reader.html

Open CASCADE Technology: STEPCAFControl_Reader Class Reference

https://dev.opencascade.org/doc/refman/html/class_s_t_e_p_c_a_f_control___reader.html

Open CASCADE Technology: STEPCAFControl_Reader Class Reference

https://dev.opencascade.org/doc/refman/html/class_s_t_e_p_c_a_f_control___reader.html

Colored STEP model in OCCT Viewer – Unlimited 3D

https://unlimited3d.wordpress.com/2022/09/09/colored-step-model-in-occt-viewer/

Colored STEP model in OCCT Viewer – Unlimited 3D

https://unlimited3d.wordpress.com/2022/09/09/colored-step-model-in-occt-viewer/

xde · Open-Cascade-SAS/OCCT Wiki · GitHub

https://github.com/Open-Cascade-SAS/OCCT/wiki/xde

xde · Open-Cascade-SAS/OCCT Wiki · GitHub

https://github.com/Open-Cascade-SAS/OCCT/wiki/xde

Colored STEP model in OCCT Viewer – Unlimited 3D

https://unlimited3d.wordpress.com/2022/09/09/colored-step-model-in-occt-viewer/

xde · Open-Cascade-SAS/OCCT Wiki · GitHub

https://github.com/Open-Cascade-SAS/OCCT/wiki/xde

Work with assemblies - Forum Open Cascade Technology

https://dev.opencascade.org/content/work-assemblies

Open CASCADE Technology: XCAFDoc_AssemblyTool Class Reference

https://dev.opencascade.org/doc/refman/html/class_x_c_a_f_doc___assembly_tool.html

Open CASCADE Technology: XCAFDoc_AssemblyTool Class Reference

https://dev.opencascade.org/doc/refman/html/class_x_c_a_f_doc___assembly_tool.html

Open CASCADE Technology: XCAFDoc_AssemblyTool Class Reference

https://dev.opencascade.org/doc/refman/html/class_x_c_a_f_doc___assembly_tool.html

Open CASCADE Technology: Extended Data Exchange (XDE)

https://old.opencascade.com/doc/occt-7.5.0/overview/html/occt_user_guides__xde.html

Open CASCADE Technology: XCAFDoc_ColorTool Class Reference

https://dev.opencascade.org/doc/refman/html/class_x_c_a_f_doc___color_tool.html

Open CASCADE Technology: Extended Data Exchange (XDE)

https://old.opencascade.com/doc/occt-7.5.0/overview/html/occt_user_guides__xde.html

c++ - How to visualize OpenCascade on QOpenGLWidget or QQuickFramebufferObject (qt6)? - Stack Overflow

https://stackoverflow.com/questions/78922288/how-to-visualize-opencascade-on-qopenglwidget-or-qquickframebufferobject-qt6

c++ - How to visualize OpenCascade on QOpenGLWidget or QQuickFramebufferObject (qt6)? - Stack Overflow

https://stackoverflow.com/questions/78922288/how-to-visualize-opencascade-on-qopenglwidget-or-qquickframebufferobject-qt6

GitHub - gkv311/occt-samples-qt: Open CASCADE Technology sample - 3D Viewer within Qt Widgets window

https://github.com/gkv311/occt-samples-qt

GitHub - gkv311/occt-samples-qt: Open CASCADE Technology sample - 3D Viewer within Qt Widgets window

https://github.com/gkv311/occt-samples-qt

GitHub - gkv311/occt-samples-qt: Open CASCADE Technology sample - 3D Viewer within Qt Widgets window

https://github.com/gkv311/occt-samples-qt

c++ - How to visualize OpenCascade on QOpenGLWidget or QQuickFramebufferObject (qt6)? - Stack Overflow

https://stackoverflow.com/questions/78922288/how-to-visualize-opencascade-on-qopenglwidget-or-qquickframebufferobject-qt6

c++ - How to visualize OpenCascade on QOpenGLWidget or QQuickFramebufferObject (qt6)? - Stack Overflow

https://stackoverflow.com/questions/78922288/how-to-visualize-opencascade-on-qopenglwidget-or-qquickframebufferobject-qt6

AIS_InteractiveContext Class Reference - Open CASCADE ...

https://dev.opencascade.org/doc/occt-6.9.0/refman/html/class_a_i_s___interactive_context.html

c++ - How to visualize OpenCascade on QOpenGLWidget or QQuickFramebufferObject (qt6)? - Stack Overflow

https://stackoverflow.com/questions/78922288/how-to-visualize-opencascade-on-qopenglwidget-or-qquickframebufferobject-qt6

BRepMesh intro – Unlimited 3D

https://unlimited3d.wordpress.com/2024/03/17/brepmesh-intro/

computational geometry - What is a linear / angular deflection in OpenCASCADE's Incremental Mesh? - Stack Overflow

https://stackoverflow.com/questions/45274793/what-is-a-linear-angular-deflection-in-opencascades-incremental-mesh

STEP unit conversion and meshing - Forum Open Cascade Technology

https://dev.opencascade.org/content/step-unit-conversion-and-meshing

STEP unit conversion and meshing - Open CASCADE Technology

https://dev.opencascade.org/content/step-unit-conversion-and-meshing

BRepMesh intro – Unlimited 3D

https://unlimited3d.wordpress.com/2024/03/17/brepmesh-intro/

BRepMesh intro – Unlimited 3D

https://unlimited3d.wordpress.com/2024/03/17/brepmesh-intro/

BRepMesh intro – Unlimited 3D

https://unlimited3d.wordpress.com/2024/03/17/brepmesh-intro/

Colored STEP model in OCCT Viewer – Unlimited 3D

https://unlimited3d.wordpress.com/2022/09/09/colored-step-model-in-occt-viewer/

AIS_InteractiveContext Class Reference - Open CASCADE Technology

https://dev.opencascade.org/doc/refman/html/class_a_i_s___interactive_context.html

xde · Open-Cascade-SAS/OCCT Wiki · GitHub

https://github.com/Open-Cascade-SAS/OCCT/wiki/xde

GitHub - gkv311/occt-samples-qt: Open CASCADE Technology sample - 3D Viewer within Qt Widgets window

https://github.com/gkv311/occt-samples-qt

Units on STEP import - Use of the platform - SALOME PROJECT

https://discourse.salome-platform.org/t/units-on-step-import/564

GitHub - gkv311/occt-samples-qt: Open CASCADE Technology sample - 3D Viewer within Qt Widgets window

https://github.com/gkv311/occt-samples-qt

Colored STEP model in OCCT Viewer – Unlimited 3D

https://unlimited3d.wordpress.com/2022/09/09/colored-step-model-in-occt-viewer/

STEP Application Protocols

https://www.steptools.com/stds/step/step_2.html

STEP Application Protocols

https://www.steptools.com/stds/step/step_2.html

Work with assemblies - Forum Open Cascade Technology

https://dev.opencascade.org/content/work-assemblies

Work with assemblies - Forum Open Cascade Technology

https://dev.opencascade.org/content/work-assemblies

Colored STEP model in OCCT Viewer – Unlimited 3D

https://unlimited3d.wordpress.com/2022/09/09/colored-step-model-in-occt-viewer/

GitHub - gkv311/occt-samples-qt: Open CASCADE Technology sample - 3D Viewer within Qt Widgets window

https://github.com/gkv311/occt-samples-qt

GitHub - gkv311/occt-samples-qt: Open CASCADE Technology sample - 3D Viewer within Qt Widgets window

https://github.com/gkv311/occt-samples-qt

BRepMesh intro – Unlimited 3D

https://unlimited3d.wordpress.com/2024/03/17/brepmesh-intro/

BRepMesh intro – Unlimited 3D

https://unlimited3d.wordpress.com/2024/03/17/brepmesh-intro/

GitHub - gkv311/occt-samples-qt: Open CASCADE Technology sample - 3D Viewer within Qt Widgets window

https://github.com/gkv311/occt-samples-qt


---

# Gemini Research

Architectural Implementation of STEP Data Exchange and High-Fidelity Rendering Pipelines in OpenCASCADE TechnologyAbstractThe integration of industrial-grade Computer-Aided Design (CAD) data into interactive software applications represents a fundamental challenge in computational geometry and computer graphics. Specifically, the ingestion of data formatted according to ISO 10303—the Standard for the Exchange of Product model data (STEP)—requires a sophisticated understanding of both the file format's EXPRESS-defined schema and the transient data structures used by modeling kernels. This report provides an exhaustive, evidence-backed implementation guide for software architects and engineers utilizing OpenCASCADE Technology (OCCT) within a C++ and Qt6 environment. The analysis deconstructs the pipeline into three critical stages: the preservation of semantic data through the Extended Data Exchange (XDE) framework, the discretization of Boundary Representation (B-Rep) geometry into renderable primitives via BRepMesh, and the orchestration of high-performance visualization using both the Application Interactive Services (AIS) and custom OpenGL implementations. Special emphasis is placed on overcoming the architectural disjunctions introduced by Qt6’s Rendering Hardware Interface (RHI) and ensuring the correct handling of metadata, assembly hierarchies, and visual attributes inherent in the STEP Application Protocols AP203, AP214, and AP242.1. The ISO 10303 STEP Standard: Protocol Analysis and Data IngestionThe foundation of any industrial data exchange pipeline is a nuanced understanding of the source format. While often treated as a monolithic file type, STEP is a collection of Application Protocols (APs) that dictate how geometric and metadata entities are structured. A robust import implementation in OCCT must distinguish between these protocols to optimize data extraction strategies.1.1 Application Protocol Taxonomy and Implementation ImplicationsThe lineage of STEP protocols reflects the evolving requirements of the aerospace, automotive, and manufacturing sectors. For a C++ developer implementing an importer, identifying the AP version is critical for setting expectations regarding data fidelity.AP203: Configuration Controlled 3D DesignHistorically the baseline for the aerospace industry, AP203 focuses primarily on the explicit definition of 3D geometry and topology (B-Rep solids) and configuration management data (versioning, authorship). In its initial iterations, AP203 lacked robust support for visual attributes such as color and layering. Consequently, legacy archives in AP203 format often import as "gray" geometry, necessitating fallback rendering strategies in the visualization pipeline. If an application requires visual fidelity from AP203 sources, it must rely on supplementary geometry validation properties rather than explicit style definitions.1AP214: Core Data for Automotive Mechanical DesignDeveloped to address the shortcomings of AP203 for the automotive sector, AP214 introduces comprehensive support for visual attributes. It includes entities for presentation layers, RGB colors, and material definitions. From an implementation standpoint, the presence of AP214 suggests that the OCAF (Open CASCADE Application Framework) document structure will need to handle complex color inheritance trees, where colors may be assigned to root assemblies, sub-assemblies, or individual faces. Neglecting this hierarchy results in incorrect visual representation, a common failure point in naive importers.1AP242: Managed Model-Based 3D EngineeringThe current gold standard, AP242, merges the scope of AP203 and AP214 while adding support for Model-Based Definition (MBD). This includes semantic Product Manufacturing Information (PMI) such as Geometric Dimensioning and Tolerancing (GD&T) and 3D annotations. Crucially, AP242 supports tessellated geometry for lightweight visualization and long-term archiving. An OCCT importer targeting AP242 must be configured to parse not just the B-Rep shapes (TopoDS_Shape) but also the semantic annotations linked to topological entities, mapping them to TDataStd attributes within the XDE document structure. This protocol is increasingly the default for modern PLM interoperability.21.2 The XDE Framework: Architectural NecessityStandard shape translation using the STEPControl_Reader class is insufficient for comprehensive CAD applications. The basic reader converts STEP entities directly into a TopoDS_Shape (often a TopoDS_Compound). This process "flattens" the assembly structure, discarding the hierarchical relationships between components and stripping away non-geometric metadata such as names, colors, and layers.To preserve the full richness of the STEP data, the Extended Data Exchange (XDE) framework—often referenced in OCCT documentation as STEPCAF—is mandatory. XDE utilizes OCAF to store the imported data in a hierarchical document structure (TDocStd_Document). This document acts as a persistent container for labels (TDF_Label), which serve as generic data nodes. Attributes containing geometry, visual styles, and names are attached to these labels rather than being embedded in the topological shapes themselves. This separation of concerns mirrors the structure of the STEP file itself, allowing for a lossless translation of the product structure.61.2.1 Initialization and Format DefinitionThe initialization of an XDE-capable application involves registering the necessary storage drivers. This is a prerequisite step that ensures the TDocStd_Document can interpret and serialize the semantic data types found in STEP files.C++// Architectural Pattern: XDE Initialization
// This sequence must occur before any file I/O operations.
Handle(TDocStd_Document) doc;
Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();

// Register drivers for binary and XML persistence of XCAF documents
BinXCAFDrivers::DefineFormat(app);
XmlXCAFDrivers::DefineFormat(app);

// Create a new document instance. "BinXCAF" denotes the internal storage format.
app->NewDocument("BinXCAF", doc); 
The XCAFApp_Application singleton manages the session's document list. By defining the formats, the application prepares the internal schema to accept the complex attribute types (like XCAFDoc_ColorTool and XCAFDoc_ShapeTool) that will be populated during the import process.71.2.2 Configuring the STEPCAFControl_ReaderThe STEPCAFControl_Reader is the specialized tool for populating an XDE document. Unlike the basic reader, it requires explicit configuration to ensure all metadata categories are processed. Default settings often prioritize speed over completeness, so enabling specific modes is required for a high-fidelity import.Critical Configuration Parameters:Color Mode (SetColorMode): Enables the translation of PRESENTATION_STYLE_ASSIGNMENT entities from STEP to XCAFDoc_ColorTool attributes.Name Mode (SetNameMode): Extracts PRODUCT and NEXT_ASSEMBLY_USAGE_OCCURRENCE names, mapping them to TDataStd_Name attributes.Layer Mode (SetLayerMode): Preserves layer assignments, critical for filtering geometry (e.g., hiding construction geometry).Validation Properties (SetPropsMode): Imports centroid, volume, and area validation properties, allowing the receiving system to verify import integrity against the source system's calculations.6Table 1 summarizes the critical flags and their impact on the OCAF data structure.Parameter FunctionOCAF Attribute AffectedImpact on VisualizationSTEP Entity SourceSetColorMode(true)XCAFDoc_ColorToolGeometry renders with source colors; critical for visual differentiation.STYLED_ITEM, PRESENTATION_STYLE_ASSIGNMENTSetNameMode(true)TDataStd_NameAssembly tree in UI displays meaningful component names.PRODUCT_DEFINITION, PRODUCT_DEFINITION_SHAPESetLayerMode(true)XCAFDoc_LayerToolEnables visibility toggling based on functional layers.PRESENTATION_LAYER_ASSIGNMENTSetPropsMode(true)XCAFDoc_DimTolToolAllows display of GD&T and validation of mass properties.SHAPE_REPRESENTATION_WITH_PARAMETERSTable 1: Configuration mapping for STEPCAFControl_Reader.1.3 Navigating the OCAF Assembly StructureOnce the Transfer method populates the TDocStd_Document, the application must traverse the label hierarchy to construct its internal scene graph. The XDE structure distinguishes between Prototypes (the definition of a part) and Instances (the usage of a part in an assembly with a location transformation). This distinction is vital for memory efficiency; a bolt used 1,000 times is stored as one geometry prototype and 1,000 lightweight reference labels.1.3.1 The Recursive Traversal AlgorithmThe traversal logic typically begins by identifying "Free Shapes"—top-level entities that are not components of another assembly. The XCAFDoc_ShapeTool API provides the GetFreeShapes method for this purpose. From these roots, the algorithm must recursively descend the tree, accumulating transformations and identifying the nature of each node.Implementation Logic for Robust Traversal:Retrieve Root Labels: Call XCAFDoc_ShapeTool::GetFreeShapes() to get the sequence of top-level labels.Determine Node Type: For each label, verify if it is a reference using XCAFDoc_ShapeTool::IsReference().If Reference: This label represents a component instance. It contains a TopLoc_Location attribute defining its position relative to the parent. The geometry resides in the referred label (the prototype), accessible via XCAFDoc_ShapeTool::GetReferredShape().If Assembly: The label acts as a container. Use XCAFDoc_ShapeTool::GetComponents() to retrieve its children.If Simple Shape: The label contains a TopoDS_Shape attribute (Solid, Shell, Face) and is a leaf node in the graph.Transformation Accumulation: As the traversal descends, the transformation matrix of the current node must be multiplied by the accumulated transformation of its parent. This ensures that leaf nodes are positioned correctly in global space.Cycle Detection: Industrial assemblies may contain circular references (invalid but possible). A robust implementation tracks visited labels to prevent infinite recursion.101.3.2 Metadata Extraction and AssociationMetadata extraction must occur in tandem with graph traversal. The semantic meaning of an object is often distributed across the instance and the prototype.Name Resolution: The name of a component instance (e.g., "Front_Left_Wheel") is stored on the reference label. If this name is empty, the application should fall back to the name of the prototype (e.g., "Wheel_Assembly_v4") stored on the referred label. This fallback logic ensures the user interface is always populated with identifiable text.6Color Inheritance: Managing colors is notoriously complex in STEP. A color can be assigned to:The specific instance in the assembly (overriding all other colors).The prototype shape.Individual sub-shapes (faces) of the prototype.The XCAFDoc_ColorTool manages these assignments. The retrieval logic must check strictly in that order. Furthermore, XDE distinguishes between ColorGen (generic/volume color), ColorSurf (surface color), and ColorCurv (curve/edge color). An effective renderer must query ColorSurf for shading and ColorCurv for wireframe rendering. If ColorSurf is missing, ColorGen serves as the fallback. Crucially, if a parent assembly node has a color assignment, it typically overrides the colors of its children, necessitating a top-down state propagation during traversal.122. The Geometry Pipeline: From B-Rep to Visualization MeshThe TopoDS_Shape objects retrieved from XDE are mathematical descriptions (NURBS, Bezier curves, analytic primitives). To be rendered by a GPU, these must be discretized into polygonal meshes. This process is handled by the BRepMesh package.2.1 BRepMesh_IncrementalMesh: Mathematical DiscretizationThe BRepMesh_IncrementalMesh class is the standard tool for generating triangulations. It populates the Poly_Triangulation structure attached to each TopoDS_Face. The quality of this mesh is governed by "deflection"—the maximum deviation allowed between the mesh and the true surface.2.1.1 Deflection Parameters and Visual FidelityChoosing the correct deflection parameters is a trade-off between visual fidelity and performance (memory and render time).Linear Deflection (theLinDeflection): This absolute value (in model units) defines the maximum chordal error. A value of 0.1mm means the center of a mesh triangle can be at most 0.1mm away from the curved surface it approximates. Using a fixed linear deflection is problematic for assemblies with mixed scales (e.g., a massive aircraft fuselage and a tiny rivet). The rivet would look blocky, or the fuselage would have millions of unnecessary triangles.16Angular Deflection (theAngDeflection): This parameter limits the angle between the normals of adjacent triangles along a curved surface. It is scale-independent. A setting of roughly $12^{\circ}$ to $20^{\circ}$ (0.2 - 0.35 radians) ensures that highly curved surfaces (like small fillets) are tessellated smoothly regardless of their physical size.Relative Deflection: Setting isRelative to true allows the linear deflection to be calculated as a coefficient of the shape's bounding box diagonal. This creates a "Level of Detail" (LOD) effect where smaller objects get finer meshes relative to their size.Optimization Strategy: For high-quality visualization, a hybrid approach is recommended. Use a relative deflection (e.g., 0.001 of the bounding box) combined with a strict angular deflection (e.g., 0.5 radians). This ensures that large planar surfaces are not over-meshed (saving triangles) while small, tight curves retain their roundness.18C++// C++ Optimization: Hybrid Deflection Strategy
// BRepMesh is computationally expensive; perform this once per unique prototype.
IMeshTools_Parameters params;
params.Deflection = 0.001;          // Relative linear deflection
params.Angle = 0.35;                // ~20 degrees angular deflection
params.Relative = Standard_True;    // Enable relative mode
params.InParallel = Standard_True;  // Enable multi-threading for meshing

BRepMesh_IncrementalMesh mesher(shape, params); 
// Perform() is called automatically in the constructor with newer OCCT versions
The InParallel flag is critical for loading large STEP files, as it distributes the meshing of independent faces across available CPU cores.192.2 Extraction of Poly_TriangulationThe BRepMesh algorithm does not return a mesh object directly; it modifies the TopoDS_Face entities in place. To extract the data for rendering:Iterate over the faces of the shape using TopExp_Explorer.Use BRep_Tool::Triangulation(face, location) to retrieve the Handle(Poly_Triangulation).The location object returned is vital. It represents the transformation of the face relative to the shape's local origin. The mesh nodes are stored in the face's local space; they must be transformed by this location to align correctly with the rest of the geometry.C++// Data Extraction Pattern
for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
    const TopoDS_Face& face = TopoDS::Face(ex.Current());
    TopLoc_Location loc;
    Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, loc);
    
    if (triangulation.IsNull()) continue;

    // Access nodes and transform them
    for (int i = 1; i <= triangulation->NbNodes(); ++i) {
        gp_Pnt node = triangulation->Node(i).Transformed(loc.Transformation());
        //... push node to vertex buffer...
    }
}
2.3 Normal Generation and Surface SmoothnessA common issue in CAD visualization is the "faceted" look where curved surfaces appear as flat polygons. This occurs because BRepMesh prioritizes topological connectivity over normal generation. By default, Poly_Triangulation may lack vertex normals.To achieve smooth shading (Gouraud or Phong), vertex normals must be calculated. While generic algorithms can compute normals by averaging the face normals of adjacent triangles, this is an approximation that can smooth out sharp edges intended by the design.The Accurate Solution: Use BRepLib_ToolTriangulatedShape::ComputeNormals. This tool leverages the underlying analytical surface (B-Spline, Cylinder, etc.) to compute the exact normal at each mesh node. This results in mathematically perfect shading where light reflects correctly off the curved surface, distinguishing between smooth blends and sharp creases. This step should be performed immediately after meshing.182.4 Wireframe and Edge RenderingRendering the "edges" of a CAD model is distinct from rendering the wireframe of the underlying mesh. CAD users expect to see the boundary curves (face boundaries) and feature lines, not the internal triangulation lines.To render these edges efficiently and accurately:Iterate over the TopAbs_EDGE entities in the face.Use BRep_Tool::PolygonOnTriangulation(edge, triangulation, loc). This function returns a Poly_PolygonOnTriangulation, which is a list of indices into the Poly_Triangulation node array.By using these existing mesh nodes, the wireframe is guaranteed to correspond exactly to the mesh vertices. This prevents "stitching errors" or "Z-fighting" where the edge line might drift away from the surface due to floating-point discrepancies if the curve were tessellated independently.213. High-Performance Rendering ArchitecturesOnce the data is prepared, the pipeline splits based on the chosen visualization strategy: using OCCT's native AIS or implementing a custom renderer.3.1 Pipeline A: OCCT Application Interactive Services (AIS)AIS is the high-level visualization framework provided by OCCT. It abstracts the low-level OpenGL commands and manages user interaction (selection, highlighting).XCAFPrs_AISObject: For XDE workflows, the XCAFPrs_AISObject is the primary workhorse. It is designed to consume a TDF_Label directly. When instantiated, it automatically traverses the label's attributes, extracts the geometry, applies the colors found in XCAFDoc_ColorTool, and handles material properties. This significantly reduces boilerplate code compared to manually constructing AIS_Shape objects and assigning aspects.10Performance Optimization in AIS:Instancing: For assemblies with thousands of identical parts (e.g., screws), AIS_ConnectedInteractive should be used. This class references a single source AIS_InteractiveObject but applies a unique transformation. This maps to hardware instancing on the GPU, drastically reducing draw calls and memory footprint.25Frustum Culling: The V3d_View automatically employs Bounding Volume Hierarchies (BVH) to cull objects outside the camera frustum. Ensuring that the bounding boxes of custom objects are calculated correctly is essential for this mechanism to function.25Polygon Offsets: To prevent Z-fighting (shimmering) when wireframe edges are drawn on top of shaded faces, AIS employs SetPolygonOffsets. This applies a depth bias in the OpenGL rasterizer, pushing the shaded faces slightly back to ensure the lines remain visible. The default settings in OCCT are usually sufficient, but complex assemblies with coincident faces may require tuning.193.2 Pipeline B: Custom OpenGL with Qt6 IntegrationFor applications requiring integration with modern UI frameworks like Qt Quick, or those needing specialized shading pipelines, a custom OpenGL renderer embedded in Qt6 is often preferred. This approach bypasses AIS, taking raw mesh data directly to the GPU.3.2.1 The Qt6 RHI ChallengeQt6 introduced the Rendering Hardware Interface (RHI), abstracting the underlying graphics API (Vulkan, Metal, Direct3D). However, OCCT's OpenGl_GraphicDriver expects a native OpenGL context. Bridging this gap requires bypassing the RHI abstraction layer.The QOpenGLWidget Strategy:QOpenGLWidget is the standard integration point. However, it renders to an off-screen Framebuffer Object (FBO), not the window surface.Context Management: The OpenGl_GraphicDriver must be initialized using the OpenGL context created by Qt. This allows OCCT to share resources and render command streams.Window Wrapping: OCCT requires an Aspect_Window. Since QOpenGLWidget does not own a native window in the traditional sense (it's a widget inside a window), developers must use Aspect_NeutralWindow. This class acts as a proxy, accepting the native handle (winId()) and the dimensions of the widget.27The Rendering Loop: In the paintGL() method of the widget, the application must trigger the OCCT viewer's redraw.Synchronization: The state of the OpenGL context must be carefully managed. Qt may modify the OpenGL state (blending, depth test) between frames. Before calling V3d_View::Redraw(), the application might need to reset specific GL states or inform OCCT that the state is "dirty".Buffer Swapping: OCCT's Redraw() typically attempts to swap buffers. In QOpenGLWidget, Qt manages the buffer swap. The OCCT driver must be configured with buffersNoSwap = true to prevent it from interfering with Qt's compositing pass.293.2.2 The "Airspace" and Compositing IssueA significant challenge in Qt integration is the "Airspace" problem. If OCCT renders directly to a native window handle (using QWidget::createWindowContainer), the 3D view sits on top of the Qt widget hierarchy, making it impossible to overlay Qt UI elements (like floating toolbars) with transparency. The QOpenGLWidget approach solves this by rendering to an FBO, which Qt then composites with the rest of the UI. This allows for semi-transparent UI overlays on top of the 3D model, a critical feature for modern UX design.30Code Pattern for Qt6 Integration:C++// In initializeGL()
Handle(OpenGl_GraphicDriver) driver = new OpenGl_GraphicDriver(displayConnection, false);
driver->ChangeOptions().buffersNoSwap = true; // Critical for QOpenGLWidget
driver->ChangeOptions().useSystemBuffer = false; // Render to FBO

// In paintGL()
// Ensure the Qt context is current
if (myView) {
    myView->Invalidate(); // Mark content as stale
    myView->Redraw();     // Execute rendering commands
}
4. Advanced Troubleshooting and Optimization4.1 Resolving Z-Fighting and Depth PrecisionLarge-scale CAD scenes often span orders of magnitude (millimeters to kilometers). This challenges the precision of the Z-buffer, leading to Z-fighting artifacts where distant or coincident surfaces flicker.Depth Buffer Precision: Ensure the OpenGL context is requested with a 24-bit depth buffer.Z-Fit: OCCT's V3d_View::ZFitAll() analyzes the scene's bounding box and dynamically adjusts the Near and Far clipping planes. This concentrates the available depth precision on the visible geometry. This method should be called after camera movements or scene updates.334.2 Handling Inverted NormalsA persistent issue in STEP import is "missing faces." This is often a rendering artifact caused by back-face culling. If a face's normal is inverted (pointing inwards), the graphics card discards it as invisible.Diagnosis: STEP files may contain shells where the topological orientation (TopAbs_REVERSED) conflicts with the geometric surface normal.Remediation: The BRepMesh algorithm generally respects the topological orientation. However, if visual artifacts persist, disabling back-face culling (vsuppressculling in Draw or Graphic3d_Culling_Back in code) can confirm the diagnosis. The permanent fix involves using ShapeFix_Shell to unify the orientation of faces within the solid during the import phase.345. ConclusionThe development of a robust STEP import and rendering pipeline in OpenCASCADE is a multidisciplinary engineering task. It moves beyond simple API calls to require a structural understanding of the STEP protocol's intent and the mathematical realities of mesh generation. By leveraging the XDE framework for data integrity, employing rigorous deflection strategies for mesh quality, and navigating the complexities of the Qt6/OpenGL integration, developers can build CAD applications that are both visually high-fidelity and performant. The architecture described herein prioritizes correctness—ensuring that the digital twin displayed on the screen is a faithful representation of the engineering definition—while providing the flexibility to scale from single components to massive industrial assemblies.Citations:1 - STEP Application Protocols (AP203/214/242).6 - XDE Framework, OCAF, and Reader Configuration.10 - Assembly Traversal, Prototypes, and Instances.12 - Metadata, Color Tools, and Attribute Management.16 - BRepMesh Parameters and Deflection.18 - Poly_Triangulation and Normal Computation.21 - Edge Discretization and Wireframe Rendering.25 - AIS Architecture and Optimization.27 - Qt6 Integration, QOpenGLWidget, and RHI.19 - Z-Fighting, Polygon Offsets, and Depth Management.34 - Inverted Normals and Missing Faces.