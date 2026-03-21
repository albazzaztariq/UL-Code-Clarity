cmake_minimum_required(VERSION 3.20)
project(CodeClarity VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── Find Qt6 ─────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# ── Sources ──────────────────────────────────────────────────────────────
set(SOURCES
    main.cpp
    core/theme.cpp
    core/mainwindow.cpp
    core/mainwindow_menus.cpp
    core/mainwindow_layout.cpp
    core/mainwindow_signals.cpp
    core/cvemonitor.cpp
    core/newsticker.cpp
    core/aibackend.cpp
    core/setupwizard.cpp
    core/settingspanel.cpp
    core/aipermissions.cpp
    editor/editor.cpp
    editor/highlighter.cpp
    editor/filetree.cpp
    core/buildbar.cpp
    core/buildsystem.cpp
    core/runtimestrip.cpp
    panels/clarity.cpp
    panels/aichat.cpp
    panels/modelconfig.cpp
    panels/langguide.cpp
    panels/basics.cpp
    panels/expander.cpp
    panels/levels.cpp
    core/runtimeanalysis.cpp
    core/optimizer.cpp
    core/sast.cpp
    core/dast.cpp
    core/securitytesting.cpp
    core/securitylab.cpp
    core/tutorial.cpp
    core/memoryanalysis.cpp
    core/dependencyanalysis.cpp
    core/codehealth.cpp
    core/interactivebasics.cpp
    core/debugger.cpp
    core/debugbackend.cpp
    core/costvisualizer.cpp
    core/rubberduck.cpp
    core/whatif.cpp
    core/datatrace.cpp
    core/predictpanel.cpp
    core/errorjournal.cpp
    core/typeflow.cpp
    core/machineview.cpp
    core/favoritesbar.cpp
    core/buildchain.cpp
    core/jsonloader.cpp
    core/analysisframe.cpp
    core/fullreport.cpp
)


set(HEADERS
    core/theme.h
    core/mainwindow.h
    core/custompipeline.h
    core/cvemonitor.h
    core/newsticker.h
    core/workspace.h
    core/aibackend.h
    core/setupwizard.h
    core/settingspanel.h
    core/aipermissions.h
    editor/editor.h
    editor/highlighter.h
    editor/filetree.h
    core/buildbar.h
    core/buildsystem.h
    core/runtimestrip.h
    panels/clarity.h
    panels/aichat.h
    panels/modelconfig.h
    panels/langguide.h
    panels/basics.h
    panels/expander.h
    panels/levels.h
    core/runtimeanalysis.h
    core/optimizer.h
    core/sast.h
    core/dast.h
    core/securitytesting.h
    core/securitylab.h
    core/tutorial.h
    core/memoryanalysis.h
    core/dependencyanalysis.h
    core/codehealth.h
    core/interactivebasics.h
    core/debugger.h
    core/debugbackend.h
    core/costvisualizer.h
    core/rubberduck.h
    core/whatif.h
    core/datatrace.h
    core/predictpanel.h
    core/errorjournal.h
    core/typeflow.h
    core/machineview.h
    core/favoritesbar.h
    core/buildchain.h
    core/jsonloader.h
    core/analysisframe.h
    core/fullreport.h
)

# ── Resources ────────────────────────────────────────────────────────────
set(RESOURCES resources.qrc)

# ── Executable ───────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── Windows system libraries ────────────────────────────────────────────
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32 ole32 dwmapi psapi)
endif()

# ── Copy data/ directory next to executable ──────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data"
    COMMENT "Copying data/ to output directory"
)

# ── Strip release builds (MinGW/GCC) ────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Release>:-s>"
        "-static-libgcc" "-static-libstdc++"
    )
endif()
