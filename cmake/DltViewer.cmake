# Locates dlt-viewer's qdlt library and headers and exposes them as the
# imported target dlt::qdlt.
#
# Lookup order:
#   1. DLT_VIEWER_SOURCE_DIR set: headers from <source>/qdlt, library from
#      DLT_VIEWER_BUILD_DIR (default <source>/build).
#   2. pkg-config module "qdlt", whose qtmajor variable is used when present.
#   3. An installed dlt-viewer in the default search paths (or
#      CMAKE_PREFIX_PATH): <prefix>/include/qdlt and libqdlt.
#
# Plugins must be built against the same qdlt the viewer runs with, so this
# always links an existing libqdlt instead of compiling a second copy.

set(DLT_VIEWER_SOURCE_DIR "" CACHE PATH "Path to a dlt-viewer source tree (optional)")
set(DLT_VIEWER_BUILD_DIR "" CACHE PATH "Path to the dlt-viewer build tree (defaults to <source>/build)")

if(DLT_VIEWER_SOURCE_DIR)
    if(NOT EXISTS "${DLT_VIEWER_SOURCE_DIR}/qdlt/plugininterface.h")
        message(FATAL_ERROR "DLT_VIEWER_SOURCE_DIR is not a dlt-viewer checkout: ${DLT_VIEWER_SOURCE_DIR}")
    endif()
    if(NOT DLT_VIEWER_BUILD_DIR)
        set(DLT_VIEWER_BUILD_DIR "${DLT_VIEWER_SOURCE_DIR}/build" CACHE PATH
            "Path to the dlt-viewer build tree (defaults to <source>/build)" FORCE)
    endif()

    set(DLT_QDLT_INCLUDE_DIR "${DLT_VIEWER_SOURCE_DIR}/qdlt")
    find_library(DLT_QDLT_LIBRARY
        NAMES qdlt
        HINTS "${DLT_VIEWER_BUILD_DIR}/bin" "${DLT_VIEWER_BUILD_DIR}/qdlt"
        NO_DEFAULT_PATH)
    set(_dlt_qdlt_source "source tree ${DLT_VIEWER_SOURCE_DIR}")
else()
    # Only read qdlt.pc's variables: resolving the module, as
    # pkg_check_modules() does, also needs the .pc files of every Qt module it
    # requires, which plugins do not use.
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_get_variable(_dlt_qdlt_pc_includedir qdlt includedir)
        pkg_get_variable(_dlt_qdlt_pc_libdir qdlt libdir)
        pkg_get_variable(_dlt_qdlt_qtmajor qdlt qtmajor)
    endif()

    if(_dlt_qdlt_pc_includedir)
        set(_dlt_qdlt_source "pkg-config qdlt")
    else()
        set(_dlt_qdlt_source "installed dlt-viewer")
    endif()

    # dlt-viewer installs headers to include/qdlt, and with DLT_INSTALL_SDK
    # also to sdk/include/qdlt.
    find_path(DLT_QDLT_INCLUDE_DIR
        NAMES plugininterface.h
        HINTS ${_dlt_qdlt_pc_includedir}
        PATH_SUFFIXES qdlt sdk/include/qdlt)
    find_library(DLT_QDLT_LIBRARY
        NAMES qdlt
        HINTS ${_dlt_qdlt_pc_libdir}
        PATH_SUFFIXES sdk/lib)
endif()

if(NOT DLT_QDLT_INCLUDE_DIR OR NOT DLT_QDLT_LIBRARY)
    message(FATAL_ERROR
        "qdlt not found (headers: '${DLT_QDLT_INCLUDE_DIR}', library: '${DLT_QDLT_LIBRARY}').\n"
        "Install dlt-viewer, add its prefix to CMAKE_PREFIX_PATH, or set "
        "DLT_VIEWER_SOURCE_DIR to a built dlt-viewer checkout.")
endif()

# Use the Qt major version libqdlt was linked against; mixing Qt versions in
# one process fails at plugin load time.
if(NOT QT_PREFIX AND _dlt_qdlt_qtmajor MATCHES "^[56]$")
    set(QT_PREFIX "Qt${_dlt_qdlt_qtmajor}")
elseif(NOT QT_PREFIX)
    file(STRINGS "${DLT_QDLT_LIBRARY}" _dlt_qdlt_qtcore REGEX "^libQt[56]Core\\.so" LIMIT_COUNT 1)
    if(_dlt_qdlt_qtcore MATCHES "^libQt([56])Core")
        set(QT_PREFIX "Qt${CMAKE_MATCH_1}")
    else()
        message(FATAL_ERROR "Could not tell which Qt ${DLT_QDLT_LIBRARY} uses; set QT_PREFIX to Qt5 or Qt6")
    endif()
endif()
set(QT_PREFIX "${QT_PREFIX}" CACHE STRING "Qt package to use (Qt5 or Qt6), must match dlt-viewer")
# qdlt's headers use Core and Gui only; plugins add Widgets.
find_package(${QT_PREFIX} REQUIRED COMPONENTS Core Gui Widgets)

message(STATUS "qdlt from:    ${_dlt_qdlt_source}")
message(STATUS "qdlt headers: ${DLT_QDLT_INCLUDE_DIR}")
message(STATUS "qdlt library: ${DLT_QDLT_LIBRARY}")
message(STATUS "Qt:           ${QT_PREFIX} ${${QT_PREFIX}Core_VERSION}")

if(NOT TARGET dlt::qdlt)
    add_library(dlt::qdlt SHARED IMPORTED)
    # The parent directory lets plugins write <qdlt/...>; qdlt's own headers
    # include each other without the prefix, so the directory itself is
    # needed too.
    get_filename_component(_dlt_qdlt_include_parent "${DLT_QDLT_INCLUDE_DIR}" DIRECTORY)
    set_target_properties(dlt::qdlt PROPERTIES
        IMPORTED_LOCATION "${DLT_QDLT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${DLT_QDLT_INCLUDE_DIR};${_dlt_qdlt_include_parent}")
    target_link_libraries(dlt::qdlt INTERFACE ${QT_PREFIX}::Core ${QT_PREFIX}::Gui)
endif()
