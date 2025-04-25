cmake_minimum_required(VERSION 3.10)
project(lvgl)


set(CONF_PATH "${PROJECT_SOURCE_DIR}/lv_conf.h")

foreach(BACKEND_NAME "SDL" "LINUX_DRM" "LINUX_FBDEV" "X11" "WAYLAND" "OPENGLES" "EVDEV")

    execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                    COMMAND "scripts/backend_conf.sh" ${BACKEND_NAME} ${CONF_PATH} OUTPUT_VARIABLE IS_BACKEND_ENABLED)
    set("LV_USE_${BACKEND_NAME}" ${IS_BACKEND_ENABLED})

endforeach()

# Uncomment if the program needs debugging
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb")

set(CMAKE_C_STANDARD 99) # LVGL officially supports C99 and above
set(CMAKE_CXX_STANDARD 17) #C17
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

if (LV_USE_EVDEV)
    message("Including EVDEV support")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EVDEV REQUIRED libevdev)

    list(APPEND PKG_CONFIG_LIB ${EVDEV_LIBRARIES})
    list(APPEND PKG_CONFIG_INC ${EVDEV_INCLUDE_DIRS})
    list(APPEND LV_LINUX_BACKEND_SRC src/lib/indev_backends/evdev.c)

endif()

# Configuration
if (LV_USE_LINUX_DRM)

    message("Including DRM support")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBDRM REQUIRED libdrm)

    list(APPEND PKG_CONFIG_LIB ${LIBDRM_LIBRARIES})
    list(APPEND PKG_CONFIG_INC ${LIBDRM_INCLUDE_DIRS})
    list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/drm.c)

endif()

if (LV_USE_SDL)

    message("Including SDL2 support")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)

    list(APPEND PKG_CONFIG_LIB ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})
    list(APPEND PKG_CONFIG_INC ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})

    list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/sdl.c)
endif()


if (LV_USE_WAYLAND)

    message("Including Wayland support")

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
    pkg_check_modules(WAYLAND_CURSOR REQUIRED wayland-cursor)
    pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)

    list(APPEND PKG_CONFIG_LIB ${WAYLAND_CLIENT_LIBRARIES})
    list(APPEND PKG_CONFIG_LIB ${WAYLAND_CURSOR_LIBRARIES})
    list(APPEND PKG_CONFIG_LIB ${XKBCOMMON_LIBRARIES})

    # Wayland protocols
    pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols>=1.25)
    pkg_get_variable(WAYLAND_PROTOCOLS_BASE wayland-protocols pkgdatadir)

    execute_process(COMMAND "scripts/gen_wl_protocols.sh" OUTPUT_VARIABLE WAYLAND_PROTOCOLS_SRC)

    list(APPEND PKG_CONFIG_INC "${PROJECT_SOURCE_DIR}/wl_protocols")
    list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/wayland.c wl_protocols/wayland_xdg_shell.c)

endif()

if (LV_USE_X11)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11 REQUIRED x11)

    message("Including X11 support")

    list(APPEND PKG_CONFIG_INC ${X11_INCLUDE_DIRS})
    list(APPEND PKG_CONFIG_LIB ${X11_LIBRARIES})
    list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/x11.c)

endif()

if (LV_USE_LINUX_FBDEV)

    # FBDEV has no dependencies
    message("Including FBDEV support")
    list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/fbdev.c)

endif()

if (LV_USE_OPENGLES)

    message("Including OPENGLES support")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW3 REQUIRED glfw3)
    pkg_check_modules(GLEW REQUIRED glew)
    
    # 始终添加X11依赖，因为GLFW3需要它
    pkg_check_modules(X11 REQUIRED x11)
    list(APPEND PKG_CONFIG_LIB ${X11_LIBRARIES})
    
    # 确保直接链接libX11，解决符号未找到的问题
    link_libraries(X11)
    
    # 添加libdl链接，用于解决dlsym符号未定义问题
    link_libraries(dl)

    list(APPEND PKG_CONFIG_LIB ${GLFW3_LIBRARIES})
    list(APPEND PKG_CONFIG_LIB ${GLEW_LIBRARIES})
    list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/glfw3.c)

endif()

foreach(arg ${PKG_CONFIG_LIB})
    string(APPEND LVGL_PKG_CONFIG_EXT_LIB " -l${arg}")
endforeach()

string(APPEND LVGL_PKG_CONFIG_LIB "-llvgl_linux")

file(GLOB LV_LINUX_SRC src/lib/*.c)
set(LV_LINUX_INC src/lib)

# 修改 lvgl_ui 目录的源文件收集方式 - 更加灵活处理文件变更
function(collect_existing_sources out_var pattern)
    file(GLOB potential_srcs ${pattern})
    set(existing_srcs "")
    foreach(src ${potential_srcs})
        if(EXISTS ${src})
            list(APPEND existing_srcs ${src})
        endif()
    endforeach()
    set(${out_var} ${existing_srcs} PARENT_SCOPE)
endfunction()

# 使用新方法收集源文件
collect_existing_sources(LVGL_UI_CORE_SRC "lvgl_ui/core/*.c")
collect_existing_sources(LVGL_UI_MODULES_SRC "lvgl_ui/modules/*.c")
collect_existing_sources(LVGL_UI_UTILS_SRC "lvgl_ui/utils/*.c")
collect_existing_sources(LVGL_UI_LOTTIE_SRC "lvgl_ui/lotties/*.c") # 明确收集lottie文件夹中的源文件
collect_existing_sources(LVGL_UI_ROOT_SRC "lvgl_ui/*.c")

# 合并所有存在的源文件
set(LVGL_UI_SRC 
    ${LVGL_UI_ROOT_SRC}
    ${LVGL_UI_CORE_SRC}
    ${LVGL_UI_MODULES_SRC}
    ${LVGL_UI_UTILS_SRC}
    ${LVGL_UI_LOTTIE_SRC} # 添加lottie源文件到编译列表
)

# 将被删除文件的引用问题输出为警告而不是错误
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=missing-include-dirs")

add_subdirectory(lvgl)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/lib ${PKG_CONFIG_INC})
add_library(lvgl_linux STATIC ${LV_LINUX_SRC} ${LV_LINUX_BACKEND_SRC})
target_include_directories(lvgl_linux PRIVATE ${LV_LINUX_INC} ${PROJECT_SOURCE_DIR})

# 修改可执行文件的构建，添加lvgl_ui源文件
add_executable(lvglsim src/main.c ${LV_LINUX_SRC} ${LV_LINUX_BACKEND_SRC} ${LVGL_UI_SRC})

# 添加lvgl_ui到包含路径
target_include_directories(lvglsim PRIVATE 
    ${PROJECT_SOURCE_DIR}/lvgl_ui
    ${PROJECT_SOURCE_DIR}/lvgl_ui/lottie # 添加lottie目录到包含路径
)

target_link_libraries(lvglsim lvgl_linux lvgl lvgl::examples lvgl::demos lvgl::thorvg m pthread ${PKG_CONFIG_LIB})

# Install the lvgl_linux library and its headers
install(DIRECTORY src/lib/
    DESTINATION include/lvgl
    FILES_MATCHING
    PATTERN "backends*" EXCLUDE
    PATTERN "*.h")

install(TARGETS lvgl_linux
    ARCHIVE DESTINATION lib
)

add_custom_target (run COMMAND ${EXECUTABLE_OUTPUT_PATH}/lvglsim DEPENDS lvglsim)
