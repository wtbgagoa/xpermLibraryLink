if(NOT WOLFRAM_KERNEL_EXECUTABLE OR NOT EXISTS "${WOLFRAM_KERNEL_EXECUTABLE}")
    message(FATAL_ERROR "A host Wolfram kernel is required for this target. Configure -DWOLFRAM_KERNEL_EXECUTABLE=/path/to/WolframKernel. stage-paclet does not require a kernel.")
endif()

set(_xperm_kernel_options -noinit -noprompt)
if(XPERM_PACLET_BASE_DIRECTORY)
    # Use Wolfram's own supported repository override; do not copy files into
    # application directories or modify the user's persistent kernel startup.
    file(MAKE_DIRECTORY "${XPERM_PACLET_BASE_DIRECTORY}")
    list(APPEND _xperm_kernel_options -pacletbase "${XPERM_PACLET_BASE_DIRECTORY}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "XPERM_PACLET_SOURCE=${XPERM_PACLET_SOURCE}"
        "XPERM_PACLET_OUTPUT_DIRECTORY=${XPERM_PACLET_OUTPUT_DIRECTORY}"
        "XPERM_PACLET_OPERATION=${XPERM_PACLET_OPERATION}"
        "XPERM_WOLFRAM_SYSTEM_ID=${XPERM_WOLFRAM_SYSTEM_ID}"
        "${WOLFRAM_KERNEL_EXECUTABLE}" ${_xperm_kernel_options}
        -script "${CMAKE_CURRENT_LIST_DIR}/BuildPaclet.wl"
    RESULT_VARIABLE _xperm_result)
if(NOT _xperm_result STREQUAL "0")
    message(FATAL_ERROR "Wolfram ${XPERM_PACLET_OPERATION} failed (${_xperm_result})")
endif()
