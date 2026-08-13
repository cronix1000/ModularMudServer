cmake_minimum_required(VERSION 3.14)
project(ModularMudServer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
else()
    add_definitions(-DPLATFORM_LINUX)
endif()

# Find packages
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Lua REQUIRED)

# Find sol2 (header-only)
find_path(SOL2_INCLUDE_DIR sol/sol.hpp
    PATHS 
        /usr/include
        /usr/local/include
        ${CMAKE_PREFIX_PATH}/include
)

if(NOT SOL2_INCLUDE_DIR)
    message(FATAL_ERROR "sol2 not found. Install with: sudo apt-get install sol2 or vcpkg install sol2")
endif()

# Source files (automatically collect all .cpp files)
file(GLOB SOURCES 
    "${CMAKE_SOURCE_DIR}/*.cpp"
)

# Remove any .cpp files that are actually headers (as seen in vcxproj)
list(REMOVE_ITEM SOURCES 
)

# Header files (for IDE support)
file(GLOB HEADERS
    "${CMAKE_SOURCE_DIR}/*.h"
    "${CMAKE_SOURCE_DIR}/*.hpp"
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${SQLITE3_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIR}
    ${SOL2_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    SQLite::SQLite3
    nlohmann_json::nlohmann_json
    ${LUA_LIBRARIES}
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
else()
    # POSIX socket library is usually part of libc on Linux, but link pthread
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy data files to build directory
set(DATA_FILES
    world_data.json
    global_terrain.json
    interactables.json
    items.json
    loot_drops.json
    mobs.json
    skills.json
    mud.db
)

foreach(data_file ${DATA_FILES})
    if(EXISTS "${CMAKE_SOURCE_DIR}/${data_file}")
        configure_file(
            "${CMAKE_SOURCE_DIR}/${data_file}"
            "${CMAKE_BINARY_DIR}/bin/${data_file}"
            COPYONLY
        )
    endif()
endforeach()

# Copy scripts directory
if(EXISTS "${CMAKE_SOURCE_DIR}/scripts")
    file(COPY "${CMAKE_SOURCE_DIR}/scripts" DESTINATION "${CMAKE_BINARY_DIR}/bin")
endif()

# Install target
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(FILES ${DATA_FILES} DESTINATION share/${PROJECT_NAME})
install(DIRECTORY scripts DESTINATION share/${PROJECT_NAME} OPTIONAL)
