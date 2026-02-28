cmake_minimum_required(VERSION 3.21)

project(TailorDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# MSVC 编译选项
if(MSVC)
    add_compile_options(/utf-8)
    # 增加对象文件大小限制以支持大模板实例化
    add_compile_options(/bigobj)
elseif(MINGW)
    # MinGW GCC 使用大对象支持
    add_compile_options(-Wa,-mbig-obj)
endif()

# Clang/LLVM 编译选项（不包括 MSVC 的 clang-cl）
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND NOT MSVC)
    add_link_options(-Wl,--no-keep-memory)
endif()

# GCC 编译选项
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND NOT MINGW)
    # GCC 大对象支持（需要 linker 2.21+）
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "4.6")
        add_compile_options(-Wa,-mbig-obj)
    endif()
endif()

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(TailorDemo
    main.cpp
    Sketch2DView.h
    Sketch2DView.cpp
    FourViewContainer.h
    FourViewContainer.cpp
    BooleanOperations.h
    BooleanOperations.cpp
    PolygonIO.h
    PolygonIO.cpp
)

# 添加第三方库路径
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_include_directories(TailorDemo PRIVATE
    "${THIRD_PARTY_DIR}/tailor"
)

target_link_libraries(TailorDemo PRIVATE Qt6::Widgets)
