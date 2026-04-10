cmake_minimum_required(VERSION 3.14)
project(Stewart CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies in deps/
set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/glfw)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui)
set(STB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/stb)
set(GLM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/glm)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${GLFW_DIR} EXCLUDE_FROM_ALL)

# GLM
add_subdirectory(${GLM_DIR} EXCLUDE_FROM_ALL)

# ImGui
file(GLOB IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Source files
file(GLOB_RECURSE SOURCES 
    "src/app/*.cpp"
    "src/core/*.cpp"
    "src/thirdparty/*.cpp"
    "src/resources/*.rc"
)

# Headers for IDE visibility
file(GLOB_RECURSE HEADERS
    "src/app/*.hpp"
    "src/core/*.hpp"
    "src/thirdparty/*.h"
    "src/resources/*.h"
)

# Build the main executable as a WIN32 app (no console)
add_executable(Stewart WIN32 ${SOURCES} ${HEADERS} ${IMGUI_SOURCES})

# Links
target_link_libraries(Stewart PRIVATE glfw glm::glm)

if(WIN32)
    target_link_libraries(Stewart PRIVATE opengl32)
endif()

# Search paths for headers
target_include_directories(Stewart PRIVATE 
    src/app
    src/core
    src/thirdparty
    src/resources
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
    ${STB_DIR}
    deps/pfd
)

if(MSVC)
    # Allows us to keep using 'int main()' while being a WIN32 windowed app
    set_target_properties(Stewart PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()
