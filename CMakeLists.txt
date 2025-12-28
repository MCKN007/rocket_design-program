cmake_minimum_required(VERSION 3.16)
project(RocketEngineDesignCEA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt相关设置
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 查找Qt库（需要Charts模块）
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts)

# 源文件列表
set(SOURCES
    main.cpp
    mainwindow.cpp
    CEAEngine.cpp
)

# 头文件列表
set(HEADERS
    mainwindow.h
    CEAEngine.h
)

# UI文件
set(FORMS
    mainwindow.ui
)

# 创建可执行文件
add_executable(RocketEngineDesignCEA
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
)

# 链接Qt库
target_link_libraries(RocketEngineDesignCEA PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Charts
)

# Windows可执行文件设置
set_target_properties(RocketEngineDesignCEA PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# 包含当前目录
target_include_directories(RocketEngineDesignCEA PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 复制CEA目录到构建目录（如果存在）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cea)
    add_custom_command(TARGET RocketEngineDesignCEA POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/cea
        $<TARGET_FILE_DIR:RocketEngineDesignCEA>/cea
        COMMENT "复制CEA目录到构建目录"
    )
endif()

# 设置应用程序图标（可选）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/icon.ico)
    set_target_properties(RocketEngineDesignCEA PROPERTIES
        RC_ICONS ${CMAKE_CURRENT_SOURCE_DIR}/icon.ico
    )
endif()
