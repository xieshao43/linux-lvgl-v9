cmake_minimum_required(VERSION 3.10)
project(lvgl)

set(CMAKE_C_STANDARD 99)#C99 # lvgl officially support C99 and above
set(CMAKE_CXX_STANDARD 17)#C17
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 添加模块化UI源文件
file(GLOB_RECURSE LVGL_UI_SOURCES 
    ${PROJECT_SOURCE_DIR}/lvgl_ui/*.c
    ${PROJECT_SOURCE_DIR}/lvgl_ui/core/*.c
    ${PROJECT_SOURCE_DIR}/lvgl_ui/modules/*.c
    ${PROJECT_SOURCE_DIR}/lvgl_ui/utils/*.c
)

# 添加包含目录
include_directories(
    ${PROJECT_SOURCE_DIR}/lvgl_ui
    ${PROJECT_SOURCE_DIR}/lvgl_ui/core
    ${PROJECT_SOURCE_DIR}/lvgl_ui/modules
    ${PROJECT_SOURCE_DIR}/lvgl_ui/utils
)

add_subdirectory(lvgl)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR})

#add_executable(main main.c mouse_cursor_icon.c) #the inital version
#the version used in quark: 
add_executable(main main.c mouse_cursor_icon.c ${LVGL_UI_SOURCES})

target_link_libraries(main lvgl lvgl::examples lvgl::demos lvgl::thorvg ${SDL2_LIBRARIES} m pthread)
add_custom_target (run COMMAND ${EXECUTABLE_OUTPUT_PATH}/main DEPENDS main)