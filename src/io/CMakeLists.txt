# I/O module - native format and STEP import/export

# Find QuaZip for ZIP file handling
find_package(QuaZip-Qt6 QUIET)

if(NOT QuaZip-Qt6_FOUND)
    # Try alternative package name
    find_package(QuaZip QUIET)
endif()

# Source files
set(IO_SOURCES
    Package.cpp
    ZipPackage.cpp
    DirectoryPackage.cpp
    JSONUtils.cpp
    OneCADFileIO.cpp
    ManifestIO.cpp
    DocumentIO.cpp
    SketchIO.cpp
    ElementMapIO.cpp
    HistoryIO.cpp
    step/StepExporter.cpp
    step/StepImporter.cpp
)

set(IO_HEADERS
    Package.h
    ZipPackage.h
    DirectoryPackage.h
    JSONUtils.h
    OneCADFileIO.h
    ManifestIO.h
    DocumentIO.h
    SketchIO.h
    ElementMapIO.h
    HistoryIO.h
    step/StepExporter.h
    step/StepImporter.h
)

add_library(onecad_io STATIC ${IO_SOURCES} ${IO_HEADERS})

target_include_directories(onecad_io 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link dependencies
target_link_libraries(onecad_io 
    PUBLIC Qt6::Core
    PUBLIC onecad_core
    PUBLIC onecad_kernel
)

# Link QuaZip if found
if(QuaZip-Qt6_FOUND)
    target_link_libraries(onecad_io PRIVATE QuaZip::QuaZip)
    target_compile_definitions(onecad_io PRIVATE HAS_QUAZIP=1)
elseif(QuaZip_FOUND)
    target_link_libraries(onecad_io PRIVATE QuaZip::QuaZip)
    target_compile_definitions(onecad_io PRIVATE HAS_QUAZIP=1)
else()
    message(WARNING "QuaZip not found - ZIP package support will be disabled")
    target_compile_definitions(onecad_io PRIVATE HAS_QUAZIP=0)
endif()

# Link OCCT for STEP import/export
target_link_libraries(onecad_io PRIVATE ${OpenCASCADE_LIBRARIES})

# Link Eigen for vector serialization
target_link_libraries(onecad_io PUBLIC Eigen3::Eigen)
