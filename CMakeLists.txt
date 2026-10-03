cmake_minimum_required(VERSION 3.16)

project(pc98fm VERSION 0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(PC98FM_SOURCES
  src/main.c
  src/core.c
  src/archive_zip.c
  src/ui_curses.c
  src/third_party/miniz.c
  src/third_party/miniz_tdef.c
  src/third_party/miniz_tinfl.c
  src/third_party/miniz_zip.c
)

if(WIN32)
  list(APPEND PC98FM_SOURCES src/fs_windows.c)

  option(PC98FM_PDCURSES "Build the curses UI with PDCurses" ON)
  if(NOT PC98FM_PDCURSES)
    message(FATAL_ERROR "The Windows build currently requires PDCurses")
  endif()

  find_path(PDCURSES_INCLUDE_DIR curses.h)
  find_library(PDCURSES_LIBRARY NAMES pdcurses PDCurses)

  if(NOT PDCURSES_INCLUDE_DIR OR NOT PDCURSES_LIBRARY)
    message(FATAL_ERROR
      "PDCurses was not found. Set PDCURSES_INCLUDE_DIR and PDCURSES_LIBRARY.")
  endif()

  set(PC98FM_UI_INCLUDE_DIRS ${PDCURSES_INCLUDE_DIR})
  set(PC98FM_UI_LIBRARIES ${PDCURSES_LIBRARY})
else()
  list(APPEND PC98FM_SOURCES src/fs_posix.c)
  find_package(Curses REQUIRED)
  set(PC98FM_UI_INCLUDE_DIRS ${CURSES_INCLUDE_DIRS} ${CURSES_INCLUDE_DIR})
  set(PC98FM_UI_LIBRARIES ${CURSES_LIBRARIES})
endif()

add_executable(pc98fm ${PC98FM_SOURCES})

target_include_directories(pc98fm PRIVATE
  src
  ${PC98FM_UI_INCLUDE_DIRS}
)

target_link_libraries(pc98fm PRIVATE ${PC98FM_UI_LIBRARIES})

if(WIN32)
  target_compile_definitions(pc98fm PRIVATE PC98FM_PDCURSES)
endif()

if(NOT WIN32 AND CURSES_CFLAGS_OTHER)
  target_compile_options(pc98fm PRIVATE ${CURSES_CFLAGS_OTHER})
endif()

target_compile_definitions(pc98fm PRIVATE MINIZ_NO_ZLIB_COMPATIBLE_NAMES)

if(MSVC)
  target_compile_options(pc98fm PRIVATE /W4)
else()
  target_compile_options(pc98fm PRIVATE -Wall -Wextra -pedantic)
endif()
