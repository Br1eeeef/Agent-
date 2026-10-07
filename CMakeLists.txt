cmake_minimum_required(VERSION 3.16)
project(agent_memory_db VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(agent_memory_core
    src/Memory.cpp
    src/TextAnalysis.cpp
    src/Scoring.cpp
    src/MemoryRetriever.cpp
    src/MemoryHashTable.cpp
    src/LruCache.cpp
    src/JsonStorage.cpp
    src/MemoryManager.cpp
    src/Entity.cpp
    src/Edge.cpp
    src/GraphStore.cpp
    src/GraphJsonStorage.cpp
    src/GraphMemoryService.cpp
)
target_include_directories(agent_memory_core PUBLIC include)

add_executable(agent_memory_demo app/main.cpp)
target_link_libraries(agent_memory_demo PRIVATE agent_memory_core)

enable_testing()
add_executable(agent_memory_tests tests/test_main.cpp)
target_link_libraries(agent_memory_tests PRIVATE agent_memory_core)
add_test(NAME agent_memory_tests COMMAND agent_memory_tests)

# ---------------------------------------------------------------------------
# Qt Widgets 演示界面
# 使用 Qt6 的 MinGW kit 构建；未找到 Qt6 时仅跳过界面目标，核心库与测试照常构建。
# ---------------------------------------------------------------------------
option(AGENT_MEMORY_BUILD_GUI "Build the Qt Widgets GUI demo" ON)

if(AGENT_MEMORY_BUILD_GUI)
    find_package(Qt6 QUIET COMPONENTS Widgets Charts SvgWidgets Test)
    if(NOT Qt6_FOUND)
        message(WARNING
            "Qt6 未找到，跳过 agent_memory_gui。请配置 -DCMAKE_PREFIX_PATH=<Qt>/6.9.2/mingw_64")
        set(AGENT_MEMORY_BUILD_GUI OFF)
    endif()
endif()

if(AGENT_MEMORY_BUILD_GUI)
    qt_standard_project_setup()

    set(GUI_SOURCES
        gui/AppContext.cpp
        gui/Logger.cpp
        gui/MainWindow.cpp
        gui/core/GraphLayout.cpp
        gui/models/MemoryTableModel.cpp
        gui/models/EntityTableModel.cpp
        gui/models/EdgeTableModel.cpp
        gui/models/LogTableModel.cpp
        gui/widgets/ChipLabel.cpp
        gui/widgets/KpiCard.cpp
        gui/widgets/ScoreBar.cpp
        gui/widgets/TypePieChart.cpp
        gui/widgets/EmptyState.cpp
        gui/widgets/Toast.cpp
        gui/widgets/GraphView.cpp
        gui/pages/OverviewPage.cpp
        gui/pages/MemoryPage.cpp
        gui/pages/RecallPage.cpp
        gui/pages/GraphPage.cpp
        gui/pages/StatusPage.cpp
        gui/dialogs/MemoryEditDialog.cpp
        gui/dialogs/RememberDialog.cpp
        gui/dialogs/EntityEditDialog.cpp
        gui/dialogs/RelationEditDialog.cpp
    )

    add_library(agent_memory_gui_lib STATIC ${GUI_SOURCES})
    # gui/ 加入包含目录，界面内部使用 "models/..."、"theme/..." 这类相对根路径引用。
    target_include_directories(agent_memory_gui_lib PUBLIC . gui)
    # 源码统一 UTF-8；MSVC 下必须显式声明，否则中文字面量会按本地代码页解码。
    if(MSVC)
        target_compile_options(agent_memory_gui_lib PUBLIC /utf-8)
    endif()
    target_compile_definitions(agent_memory_gui_lib
        PUBLIC AGENT_MEMORY_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(agent_memory_gui_lib
        PUBLIC agent_memory_core Qt6::Widgets Qt6::Charts Qt6::SvgWidgets)

    qt_add_executable(agent_memory_gui gui/main_gui.cpp)
    target_link_libraries(agent_memory_gui PRIVATE agent_memory_gui_lib)
    set_target_properties(agent_memory_gui PROPERTIES WIN32_EXECUTABLE TRUE)
    qt_add_resources(agent_memory_gui "gui_resources"
        PREFIX "/"
        FILES
            resources/style.qss
            resources/chevron-up.svg
            resources/chevron-down.svg)

    qt_add_executable(agent_memory_gui_tests tests/gui/test_gui_main.cpp)
    target_link_libraries(agent_memory_gui_tests
        PRIVATE agent_memory_gui_lib Qt6::Test)
    add_test(NAME agent_memory_gui_tests COMMAND agent_memory_gui_tests)
    set_tests_properties(agent_memory_gui_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

