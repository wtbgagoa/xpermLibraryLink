# Paclet staging needs only the compiler and LibraryLink SDK. The separate
# archive/install targets invoke a licensed host Wolfram kernel on demand.
function(xperm_add_paclet_targets)
    set(XPERM_PACLET_VERSION "0.1.0" CACHE STRING "Version of the xPermLibraryLink paclet")
    set(XPERM_WOLFRAM_SYSTEM_ID "" CACHE STRING "Target Wolfram SystemID (normally inferred from the target OS and architecture)")
    set(XPERM_PACLET_BASE_DIRECTORY "" CACHE PATH "Override the Wolfram paclet base directory for install-paclet; empty uses the normal user repository")
    if(NOT XPERM_PACLET_VERSION MATCHES "^[0-9]+(\\.[0-9]+)+$")
        message(FATAL_ERROR "XPERM_PACLET_VERSION must be a dotted numeric version, for example 0.1.0")
    endif()

    # Use the target architecture, not the architecture of the host kernel.
    # CMAKE_OSX_ARCHITECTURES can override the host processor on macOS.
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _xperm_processor)
    if(APPLE AND CMAKE_OSX_ARCHITECTURES)
        list(LENGTH CMAKE_OSX_ARCHITECTURES _xperm_arch_count)
        if(_xperm_arch_count GREATER 1 AND NOT XPERM_WOLFRAM_SYSTEM_ID)
            message(FATAL_ERROR "Build one macOS architecture per paclet, or set XPERM_WOLFRAM_SYSTEM_ID explicitly")
        elseif(_xperm_arch_count EQUAL 1)
            string(TOLOWER "${CMAKE_OSX_ARCHITECTURES}" _xperm_processor)
        endif()
    endif()
    if(CMAKE_GENERATOR_PLATFORM)
        string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" _xperm_processor)
    endif()
    if(NOT XPERM_WOLFRAM_SYSTEM_ID)
        set(_xperm_system_id "")
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            if(_xperm_processor MATCHES "^(x86_64|amd64|x64)$" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(_xperm_system_id "Linux-x86-64")
            elseif(_xperm_processor MATCHES "^(aarch64|arm64)$" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(_xperm_system_id "Linux-ARM64")
            elseif(_xperm_processor MATCHES "^arm" AND CMAKE_SIZEOF_VOID_P EQUAL 4)
                set(_xperm_system_id "Linux-ARM")
            elseif(_xperm_processor MATCHES "^(i[3-6]86|x86|x86_64|amd64)$" AND CMAKE_SIZEOF_VOID_P EQUAL 4)
                set(_xperm_system_id "Linux")
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            if(_xperm_processor MATCHES "^(aarch64|arm64)$")
                set(_xperm_system_id "MacOSX-ARM64")
            elseif(_xperm_processor MATCHES "^(x86_64|amd64)$")
                set(_xperm_system_id "MacOSX-x86-64")
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
            if(_xperm_processor MATCHES "^(aarch64|arm64)$" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(_xperm_system_id "Windows-ARM64")
            elseif(_xperm_processor MATCHES "^(x86_64|amd64|x64)$" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(_xperm_system_id "Windows-x86-64")
            elseif(_xperm_processor MATCHES "^(i[3-6]86|x86|x86_64|amd64|win32)$" AND CMAKE_SIZEOF_VOID_P EQUAL 4)
                set(_xperm_system_id "Windows")
            endif()
        endif()
        if(NOT _xperm_system_id)
            message(WARNING "Paclet targets disabled: cannot infer the target Wolfram SystemID. Set XPERM_WOLFRAM_SYSTEM_ID to enable them.")
            return()
        endif()
        # Keep the cache override empty, so a changed target architecture is
        # inferred again on reconfiguration instead of retaining a stale value.
        set(XPERM_WOLFRAM_SYSTEM_ID "${_xperm_system_id}")
    endif()
    if(NOT XPERM_WOLFRAM_SYSTEM_ID MATCHES "^[A-Za-z0-9]+(-[A-Za-z0-9]+)*$")
        message(FATAL_ERROR "Invalid XPERM_WOLFRAM_SYSTEM_ID: ${XPERM_WOLFRAM_SYSTEM_ID}")
    endif()

    find_program(WOLFRAM_KERNEL_EXECUTABLE
        NAMES WolframKernel math wolfram
        HINTS "${WolframLanguage_INSTALL_DIR}" "$ENV{MATHEMATICA_HOME}"
        PATH_SUFFIXES Executables Contents/MacOS MacOS
        NO_CMAKE_FIND_ROOT_PATH
        DOC "Host Wolfram kernel used by the paclet and install-paclet targets")

    set(_xperm_paclet_parent "${CMAKE_CURRENT_BINARY_DIR}/paclet")
    if(CMAKE_CONFIGURATION_TYPES)
        string(APPEND _xperm_paclet_parent "/$<CONFIG>")
    endif()
    set(_xperm_paclet_root "${_xperm_paclet_parent}/xPermLibraryLink")
    set(_xperm_library_dir "${_xperm_paclet_root}/LibraryResources/${XPERM_WOLFRAM_SYSTEM_ID}")
    configure_file("${PROJECT_SOURCE_DIR}/PacletInfo.wl.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/PacletInfo.wl" @ONLY)

    # Recreate only the generated staging directory, including on repeat builds,
    # to avoid archiving stale binaries after changing target platform or version.
    add_custom_target(stage-paclet
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${_xperm_paclet_root}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_xperm_paclet_root}/Kernel" "${_xperm_library_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/PacletInfo.wl" "${_xperm_paclet_root}/PacletInfo.wl"
        COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/Kernel/init.m" "${_xperm_paclet_root}/Kernel/init.m"
        COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/Kernel/Implementation.wl" "${_xperm_paclet_root}/Kernel/Implementation.wl"
        COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/LICENSE" "${_xperm_paclet_root}/LICENSE"
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:xpermLL>" "${_xperm_library_dir}/$<TARGET_FILE_NAME:xpermLL>"
        DEPENDS xpermLL
        COMMENT "Staging xPermLibraryLink ${XPERM_PACLET_VERSION} for ${XPERM_WOLFRAM_SYSTEM_ID}"
        VERBATIM)

    foreach(_xperm_operation IN ITEMS paclet install-paclet)
        add_custom_target(${_xperm_operation}
            COMMAND "${CMAKE_COMMAND}"
                "-DWOLFRAM_KERNEL_EXECUTABLE=${WOLFRAM_KERNEL_EXECUTABLE}"
                "-DXPERM_PACLET_SOURCE=${_xperm_paclet_root}"
                "-DXPERM_PACLET_OUTPUT_DIRECTORY=${_xperm_paclet_parent}"
                "-DXPERM_PACLET_BASE_DIRECTORY=${XPERM_PACLET_BASE_DIRECTORY}"
                "-DXPERM_WOLFRAM_SYSTEM_ID=${XPERM_WOLFRAM_SYSTEM_ID}"
                "-DXPERM_PACLET_OPERATION=${_xperm_operation}"
                -P "${PROJECT_SOURCE_DIR}/cmake/RunPaclet.cmake"
            DEPENDS stage-paclet
            COMMENT "Running Wolfram ${_xperm_operation} for xPermLibraryLink"
            USES_TERMINAL VERBATIM)
    endforeach()
    if(NOT WOLFRAM_KERNEL_EXECUTABLE)
        message(STATUS "No host Wolfram kernel found: stage-paclet is available; set WOLFRAM_KERNEL_EXECUTABLE to use paclet or install-paclet")
    endif()
    message(STATUS "Paclet target SystemID: ${XPERM_WOLFRAM_SYSTEM_ID}")
endfunction()
