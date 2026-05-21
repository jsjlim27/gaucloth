cmake_minimum_required(VERSION 3.16)
project(gaucloth LANGUAGES CXX C)

# ---- C++ standard -----------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(OpenGL_GL_PREFERENCE GLVND)

# ---- Dependencies -----------------------------------------------------------
include(FetchContent)

# GLFW: fetched and built from source, pinned to a specific release.
# No system package required - any user with CMake + a compiler + git gets it.
FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.4
)

# Turn off GLFW's own tests/examples/docs so we only build the library.
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# OpenGL: provided by the system GPU drivers (this is fine to require --
# every machine with working graphics already has it).
find_package(OpenGL REQUIRED)

# ---- Executable -------------------------------------------------------------
# GLAD is vendored into external/glad/ (generated once, commited to the repo)
# so every clone gets the exact same OpenGL version/profile. glad.c compiles
# directly into our target.
add_executable(gaucloth
	src/main.cpp
	external/glad/src/glad.c
)

target_include_directories(gaucloth PRIVATE
	external/glad/include
)

target_link_libraries(gaucloth PRIVATE
	glfw
	OpenGL::GL
)
