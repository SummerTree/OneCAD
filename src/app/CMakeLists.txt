add_library(onecad_app STATIC
    Application.cpp
    commands/AddBodyCommand.cpp
    commands/DeleteBodyCommand.cpp
    commands/DeleteSketchCommand.cpp
    commands/ModifyBodyCommand.cpp
    commands/RenameBodyCommand.cpp
    commands/RenameSketchCommand.cpp
    commands/ToggleVisibilityCommand.cpp
    commands/CommandProcessor.cpp
    document/Document.cpp
    selection/SelectionManager.cpp
)

target_include_directories(onecad_app
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCASCADE_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(onecad_app
    PUBLIC
    Qt6::Core
    onecad_core
    onecad_render
)
