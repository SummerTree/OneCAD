# OneCAD Tests
enable_testing()

# OCAF Prototype
add_executable(proto_tnaming prototypes/proto_tnaming.cpp)
target_link_libraries(proto_tnaming 
    PRIVATE 
    ${OpenCASCADE_LIBRARIES}
)
target_include_directories(proto_tnaming PRIVATE ${OpenCASCADE_INCLUDE_DIR})

# Custom Map Prototype
add_executable(proto_custom_map prototypes/proto_custom_map.cpp)
target_link_libraries(proto_custom_map 
    PRIVATE 
    ${OpenCASCADE_LIBRARIES}
)
target_include_directories(proto_custom_map PRIVATE ${OpenCASCADE_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src)

# ElementMap Rigorous Tests
add_executable(proto_elementmap_rigorous prototypes/proto_elementmap_rigorous.cpp)
target_link_libraries(proto_elementmap_rigorous
    PRIVATE
    ${OpenCASCADE_LIBRARIES}
)
target_include_directories(proto_elementmap_rigorous PRIVATE ${OpenCASCADE_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src)

# Sketch Geometry Prototype
add_executable(proto_sketch_geometry prototypes/proto_sketch_geometry.cpp)
target_link_libraries(proto_sketch_geometry
    PRIVATE
    onecad_core
)

# Sketch Constraints Prototype
add_executable(proto_sketch_constraints prototypes/proto_sketch_constraints.cpp)
target_link_libraries(proto_sketch_constraints
    PRIVATE
    onecad_core
)

# Sketch Snap Prototype
add_executable(proto_sketch_snap prototypes/proto_sketch_snap.cpp)
target_link_libraries(proto_sketch_snap
    PRIVATE
    onecad_core
)

# Sketch Solver Adapter Prototype
add_executable(proto_sketch_solver prototypes/proto_sketch_solver.cpp)
target_link_libraries(proto_sketch_solver
    PRIVATE
    onecad_core
)

# Sketch Trim Prototype
add_executable(proto_sketch_trim prototypes/proto_sketch_trim.cpp)
target_link_libraries(proto_sketch_trim
    PRIVATE
    onecad_core
)

# Sketch constraint applicability prototype
add_executable(proto_sketch_constraint_applicability prototypes/proto_sketch_constraint_applicability.cpp)
target_link_libraries(proto_sketch_constraint_applicability
    PRIVATE
    onecad_ui
    onecad_app
    Qt6::Core
)
target_include_directories(proto_sketch_constraint_applicability PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Sketch Group Drag Regression Prototype
add_executable(proto_sketch_group_drag prototypes/proto_sketch_group_drag.cpp)
target_link_libraries(proto_sketch_group_drag
    PRIVATE
    onecad_core
)
target_include_directories(proto_sketch_group_drag PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Sketch Drag Undo Prototype
add_executable(proto_sketch_drag_undo prototypes/proto_sketch_drag_undo.cpp)
target_link_libraries(proto_sketch_drag_undo
    PRIVATE
    onecad_app
)
target_include_directories(proto_sketch_drag_undo PRIVATE ${CMAKE_SOURCE_DIR}/src)

# PlaneGCS Integration Prototype
add_executable(proto_planegcs_integration prototypes/proto_planegcs_integration.cpp)
target_link_libraries(proto_planegcs_integration
    PRIVATE
    planegcs
)

# Loop Detector Prototype
add_executable(proto_loop_detector prototypes/proto_loop_detector.cpp)
target_link_libraries(proto_loop_detector
    PRIVATE
    onecad_core
)

# Face Builder Prototype
add_executable(proto_face_builder prototypes/proto_face_builder.cpp)
target_link_libraries(proto_face_builder
    PRIVATE
    onecad_core
)

# Fixed constraint and translatePlaneInSketch prototype
add_executable(proto_sketch_fixed_and_move prototypes/proto_sketch_fixed_and_move.cpp)
target_link_libraries(proto_sketch_fixed_and_move
    PRIVATE
    onecad_core
)

# Sketch draft tool dimension prototype
add_executable(proto_sketch_tool_dimensions prototypes/proto_sketch_tool_dimensions.cpp)
target_link_libraries(proto_sketch_tool_dimensions
    PRIVATE
    onecad_core
)

# Sketch region candidate prototype
add_executable(proto_sketch_region_candidates prototypes/proto_sketch_region_candidates.cpp)
target_link_libraries(proto_sketch_region_candidates
    PRIVATE
    onecad_core
)
target_include_directories(proto_sketch_region_candidates PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Sketch plane viewport regression prototype
add_executable(proto_sketch_plane_viewport prototypes/proto_sketch_plane_viewport.cpp)
target_link_libraries(proto_sketch_plane_viewport
    PRIVATE
    onecad_core
    onecad_render
    onecad_ui
    Qt6::Core
    Qt6::Gui
)
target_include_directories(proto_sketch_plane_viewport PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Viewport Compilation Test
add_executable(test_compile test_compile.cpp)
target_link_libraries(test_compile
    PRIVATE
    onecad_app
    onecad_ui
    onecad_io
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)
target_include_directories(test_compile PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Model Picker Prototype
add_executable(proto_model_picker prototypes/proto_model_picker.cpp)
target_link_libraries(proto_model_picker
    PRIVATE
    onecad_ui
    onecad_io
    onecad_app
    Qt6::Gui
    Qt6::Core
)
target_include_directories(proto_model_picker PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Viewport drag-state regression prototype
add_executable(proto_viewport_drag_state prototypes/proto_viewport_drag_state.cpp)
target_link_libraries(proto_viewport_drag_state
    PRIVATE
    onecad_ui
    onecad_app
    onecad_io
    onecad_core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)
target_include_directories(proto_viewport_drag_state PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(proto_viewport_drag_state PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

add_executable(proto_viewport_drag_guide_capture prototypes/proto_viewport_drag_guide_capture.cpp)
target_link_libraries(proto_viewport_drag_guide_capture
    PRIVATE
    onecad_ui
    onecad_app
    onecad_io
    onecad_core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)
target_include_directories(proto_viewport_drag_guide_capture PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(proto_viewport_drag_guide_capture PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

# Viewport group drag feedback prototype
add_executable(proto_viewport_group_drag_feedback prototypes/proto_viewport_group_drag_feedback.cpp)
target_link_libraries(proto_viewport_group_drag_feedback
    PRIVATE
    onecad_ui
    onecad_app
    onecad_io
    onecad_core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)
target_include_directories(proto_viewport_group_drag_feedback PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(proto_viewport_group_drag_feedback PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

# Tessellation Cache Prototype
add_executable(proto_tessellation_cache prototypes/proto_tessellation_cache.cpp)
target_link_libraries(proto_tessellation_cache
    PRIVATE
    onecad_app
    Qt6::Core
)
target_include_directories(proto_tessellation_cache PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Pick Mesh Integration Prototype
add_executable(proto_pickmesh_integration prototypes/proto_pickmesh_integration.cpp)
target_link_libraries(proto_pickmesh_integration
    PRIVATE
    onecad_ui
    onecad_io
    onecad_app
    Qt6::Core
    Qt6::Gui
)
target_include_directories(proto_pickmesh_integration PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Pick Topology Promotion Prototype
add_executable(proto_pick_topology_promotion prototypes/proto_pick_topology_promotion.cpp)
target_link_libraries(proto_pick_topology_promotion
    PRIVATE
    onecad_ui
    onecad_io
    onecad_app
    Qt6::Core
    Qt6::Gui
)
target_include_directories(proto_pick_topology_promotion PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Selection Topology Resolver Prototype
add_executable(proto_selection_topology_resolver prototypes/proto_selection_topology_resolver.cpp)
target_link_libraries(proto_selection_topology_resolver
    PRIVATE
    onecad_app
    onecad_core
    Qt6::Core
)
target_include_directories(proto_selection_topology_resolver PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Model selection resolution prototype
add_executable(proto_model_selection_resolution prototypes/proto_model_selection_resolution.cpp)
target_link_libraries(proto_model_selection_resolution
    PRIVATE
    onecad_app
    Qt6::Core
)
target_include_directories(proto_model_selection_resolution PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Document lifecycle (autosave document-swap safety)
add_executable(proto_document_lifecycle prototypes/proto_document_lifecycle.cpp)
target_link_libraries(proto_document_lifecycle
    PRIVATE
    onecad_app
    onecad_core
    onecad_io
    Qt6::Core
)
target_include_directories(proto_document_lifecycle PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Regeneration Engine Prototype
add_executable(proto_regeneration prototypes/proto_regeneration.cpp)
target_link_libraries(proto_regeneration
    PRIVATE
    onecad_app
    onecad_core
    onecad_io
    Qt6::Core
)
target_include_directories(proto_regeneration PRIVATE ${CMAKE_SOURCE_DIR}/src)

# History I/O Compatibility Prototype
add_executable(proto_history_io_compat prototypes/proto_history_io_compat.cpp)
target_link_libraries(proto_history_io_compat
    PRIVATE
    onecad_io
    onecad_app
    Qt6::Core
)
target_include_directories(proto_history_io_compat PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Document roundtrip compatibility Prototype
add_executable(proto_document_roundtrip_compat prototypes/proto_document_roundtrip_compat.cpp)
target_link_libraries(proto_document_roundtrip_compat
    PRIVATE
    onecad_io
    onecad_app
    onecad_core
    Qt6::Core
)
target_include_directories(proto_document_roundtrip_compat PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Timeline rollback + dirty propagation prototype
add_executable(proto_timeline_rollback_dirty prototypes/proto_timeline_rollback_dirty.cpp)
target_link_libraries(proto_timeline_rollback_dirty
    PRIVATE
    onecad_app
    onecad_core
    Qt6::Core
)
target_include_directories(proto_timeline_rollback_dirty PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Integration test: document workflow (create sketch, extrude, save, reload, verify)
add_executable(test_document_workflow integration/test_document_workflow.cpp)
target_link_libraries(test_document_workflow
    PRIVATE
    onecad_io
    onecad_app
    onecad_core
    Qt6::Core
)
target_include_directories(test_document_workflow PRIVATE ${CMAKE_SOURCE_DIR}/src)

# --- CTest registration ---
add_test(NAME proto_custom_map COMMAND proto_custom_map)
add_test(NAME proto_tnaming COMMAND proto_tnaming)
add_test(NAME proto_elementmap_rigorous COMMAND proto_elementmap_rigorous)
add_test(NAME proto_sketch_geometry COMMAND proto_sketch_geometry)
add_test(NAME proto_sketch_constraints COMMAND proto_sketch_constraints)
add_test(NAME proto_sketch_snap COMMAND proto_sketch_snap)
add_test(NAME proto_sketch_solver COMMAND proto_sketch_solver)
add_test(NAME proto_sketch_trim COMMAND proto_sketch_trim)
add_test(NAME proto_sketch_constraint_applicability COMMAND proto_sketch_constraint_applicability)
add_test(NAME proto_sketch_group_drag COMMAND proto_sketch_group_drag)
add_test(NAME proto_sketch_drag_undo COMMAND proto_sketch_drag_undo)
add_test(NAME proto_planegcs_integration COMMAND proto_planegcs_integration)
add_test(NAME proto_loop_detector COMMAND proto_loop_detector)
add_test(NAME proto_face_builder COMMAND proto_face_builder)
add_test(NAME proto_sketch_fixed_and_move COMMAND proto_sketch_fixed_and_move)
add_test(NAME proto_sketch_tool_dimensions COMMAND proto_sketch_tool_dimensions)
add_test(NAME proto_sketch_region_candidates COMMAND proto_sketch_region_candidates)
add_test(NAME proto_sketch_plane_viewport COMMAND proto_sketch_plane_viewport)
add_test(NAME proto_model_picker COMMAND proto_model_picker)
add_test(NAME proto_tessellation_cache COMMAND proto_tessellation_cache)
add_test(NAME proto_pickmesh_integration COMMAND proto_pickmesh_integration)
add_test(NAME proto_pick_topology_promotion COMMAND proto_pick_topology_promotion)
add_test(NAME proto_selection_topology_resolver COMMAND proto_selection_topology_resolver)
add_test(NAME proto_model_selection_resolution COMMAND proto_model_selection_resolution)
add_test(NAME proto_document_lifecycle COMMAND proto_document_lifecycle)
add_test(NAME proto_regeneration COMMAND proto_regeneration)
add_test(NAME proto_history_io_compat COMMAND proto_history_io_compat)
add_test(NAME proto_document_roundtrip_compat COMMAND proto_document_roundtrip_compat)
add_test(NAME proto_timeline_rollback_dirty COMMAND proto_timeline_rollback_dirty)
add_test(NAME test_document_workflow COMMAND test_document_workflow)

# --- Assert liveness ---
# Keep assert() live in every test binary regardless of build type.
# CI builds Release (NDEBUG), which would otherwise turn the assert-based
# prototypes into unconditional passes. Target COMPILE_OPTIONS are emitted
# after CMAKE_CXX_FLAGS_<CONFIG>, so -UNDEBUG wins on GCC/Clang/AppleClang.
get_property(_onecad_test_targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
foreach(_t IN LISTS _onecad_test_targets)
    target_compile_options(${_t} PRIVATE -UNDEBUG)
endforeach()
unset(_onecad_test_targets)
