cmake_minimum_required(VERSION 3.20)
project(FarmFrenzyPort C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")
endif()

# ---------------------------------------------------------------- options
option(FF_VENDORED_SDL "Always download and build SDL3 statically (self-contained binary)" OFF)
set(FF_GAME_DIR "" CACHE PATH "Original game folder (with Data/ and farm.wrp.exe) to copy next to the build")
set(FF_HD_PACK "" CACHE FILEPATH "Optional hd.pack with upscaled textures (tools/make_hd_pack.py)")
set(FF_ICON "" CACHE FILEPATH "Optional icon: .icns on macOS, .ico on Windows")

# ---------------------------------------------------------------- SDL3
if(NOT FF_VENDORED_SDL)
  find_package(SDL3 CONFIG QUIET)
endif()
if(NOT SDL3_FOUND)
  message(STATUS "SDL3 not found on the system: fetching it")
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.18.tar.gz)
  FetchContent_MakeAvailable(SDL3)
endif()

# ---------------------------------------------------------------- game
file(GLOB SRC CONFIGURE_DEPENDS src/*.cpp)
set(THIRD third_party/pugixml.cpp third_party/stb_vorbis_impl.c)

set(APP_NAME "Farm Frenzy")
set(ICON_SRC "")
if(FF_ICON AND EXISTS "${FF_ICON}")
  if(APPLE)
    set(ICON_SRC "${FF_ICON}")
    set_source_files_properties("${FF_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  elseif(WIN32)
    set(ICON_RC "${CMAKE_BINARY_DIR}/icon.rc")
    file(TO_CMAKE_PATH "${FF_ICON}" ICON_PATH)
    file(WRITE "${ICON_RC}" "IDI_ICON1 ICON \"${ICON_PATH}\"\n")
    set(ICON_SRC "${ICON_RC}")
  endif()
endif()

add_executable(FarmFrenzy ${SRC} ${THIRD} ${ICON_SRC})
target_include_directories(FarmFrenzy PRIVATE src third_party)
target_link_libraries(FarmFrenzy PRIVATE SDL3::SDL3)
set_source_files_properties(third_party/stb_vorbis_impl.c third_party/pugixml.cpp PROPERTIES COMPILE_OPTIONS
  "$<IF:$<OR:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,/w,-w>")

if(MSVC)
  # sources contain UTF-8 (Russian) literals
  target_compile_options(FarmFrenzy PRIVATE /utf-8 /W3)
  target_compile_definitions(FarmFrenzy PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  target_compile_options(FarmFrenzy PRIVATE -Wall -Wno-unused-function -Wno-sign-compare)
endif()
if(WIN32)
  target_compile_definitions(FarmFrenzy PRIVATE NOMINMAX)
endif()

if(APPLE)
  set(DATA_DIR "$<TARGET_BUNDLE_CONTENT_DIR:FarmFrenzy>/Resources/Data")
  get_filename_component(ICON_NAME "${FF_ICON}" NAME)
  set_target_properties(FarmFrenzy PROPERTIES
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "${APP_NAME}"
    MACOSX_BUNDLE_BUNDLE_NAME "Веселая ферма"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.farmfrenzy-port"
    MACOSX_BUNDLE_ICON_FILE "${ICON_NAME}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/res/Info.plist.in")
else()
  set(DATA_DIR "$<TARGET_FILE_DIR:FarmFrenzy>/Data")
  if(WIN32)
    set_target_properties(FarmFrenzy PROPERTIES WIN32_EXECUTABLE TRUE)
    if(SDL3_FOUND)
      # a system (shared) SDL3 needs its DLL next to the exe; the fetched one is static
      add_custom_command(TARGET FarmFrenzy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:FarmFrenzy> $<TARGET_FILE_DIR:FarmFrenzy>
        COMMAND_EXPAND_LISTS)
    endif()
  endif()
endif()

# ---------------------------------------------------------------- game data (not part of the repository)
if(FF_GAME_DIR)
  set(GD "${FF_GAME_DIR}")
  if(EXISTS "${GD}/Data/data.pack")
    set(GD "${GD}/Data")
  endif()
  if(NOT EXISTS "${GD}/data.pack")
    message(FATAL_ERROR "FF_GAME_DIR: data.pack not found in ${FF_GAME_DIR} or its Data/ folder")
  endif()
  set(DATA_CMDS
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DATA_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GD}/data.pack" "${GD}/ru.pack" "${DATA_DIR}/")
  # tutorial tip texts live in the original executable
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(EXISTS "${GD}/../farm.wrp.exe" AND Python3_FOUND)
    list(APPEND DATA_CMDS COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/extract_tips.py"
      "${GD}/../farm.wrp.exe" "${DATA_DIR}/tips.txt")
  elseif(EXISTS "${GD}/tips.txt")
    list(APPEND DATA_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GD}/tips.txt" "${DATA_DIR}/")
  else()
    message(WARNING "No farm.wrp.exe (with Python 3) or tips.txt: tutorial tips will be disabled")
  endif()
  if(FF_HD_PACK AND EXISTS "${FF_HD_PACK}")
    list(APPEND DATA_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FF_HD_PACK}" "${DATA_DIR}/hd.pack")
  elseif(EXISTS "${GD}/hd.pack")
    list(APPEND DATA_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GD}/hd.pack" "${DATA_DIR}/")
  endif()
  add_custom_command(TARGET FarmFrenzy POST_BUILD ${DATA_CMDS} VERBATIM)
endif()

# ---------------------------------------------------------------- dev tool: dump textures to PNG
add_executable(dumptex EXCLUDE_FROM_ALL tools/dumptex.cpp src/vfs.cpp src/image.cpp)
target_include_directories(dumptex PRIVATE src third_party)
if(MSVC)
  target_compile_options(dumptex PRIVATE /utf-8)
endif()
