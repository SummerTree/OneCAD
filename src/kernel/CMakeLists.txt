# Kernel module - OCCT wrappers and ElementMap/topology helpers
add_library(onecad_kernel STATIC
    geometry/EdgeChainer.cpp
    modeling/FaceExtrudeProfileBuilder.cpp
    topology/CoplanarFacePatch.cpp
    topology/FacePatchResolver.cpp
    topology/SelectionTopologyResolver.cpp
)

target_include_directories(onecad_kernel
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCASCADE_INCLUDE_DIR}
)

target_link_libraries(onecad_kernel
    PUBLIC
    ${OpenCASCADE_LIBRARIES}
)
