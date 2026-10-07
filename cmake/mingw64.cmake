set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(WSTP_INCLUDE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/wstp/windows"
)

set(WSTP_LIBRARY
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/wstp/windows/wstp64i4.lib"
)

