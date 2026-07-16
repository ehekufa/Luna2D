cmake_minimum_required(VERSION 3.11)
project(Luna2D C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# 1. Подключаем Raylib
add_subdirectory(${THIRD_PARTY_DIR}/raylib ${CMAKE_CURRENT_BINARY_DIR}/raylib)

# 2. Подключаем Box2D
add_subdirectory(${THIRD_PARTY_DIR}/box2d ${CMAKE_CURRENT_BINARY_DIR}/box2d)

# 3. Находим правильную версию LuaJIT для текущей ABI
include_directories(${THIRD_PARTY_DIR}/LuaJIT/src)

if(NOT LUAJIT_LIB_DIR)
    set(LUAJIT_LIB_DIR ${THIRD_PARTY_DIR}/LuaJIT/src)
endif()

# Добавляем статическую библиотеку LuaJIT
add_library(luajit STATIC IMPORTED)
set_target_properties(luajit PROPERTIES
    IMPORTED_LOCATION "${LUAJIT_LIB_DIR}/libluajit.a"
)

# Главная библиотека для Android
add_library(luna2d SHARED src/main.cpp)

# Линкуем всё вместе
target_link_libraries(luna2d 
    raylib 
    box2d 
    luajit
    log 
    android 
    EGL 
    GLESv2
)

target_include_directories(luna2d PRIVATE 
    src 
    ${THIRD_PARTY_DIR}/raylib/src
    ${THIRD_PARTY_DIR}/box2d/include
    ${THIRD_PARTY_DIR}/LuaJIT/src
)
