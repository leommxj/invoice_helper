cmake_minimum_required(VERSION 3.24)

project(invoice_helper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

function(invoice_helper_set_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
endfunction()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(wxWidgets CONFIG REQUIRED)
find_package(unofficial-libmupdf CONFIG REQUIRED)

set(WX_TARGETS wx::core wx::base)
if (TARGET wx::adv)
    list(APPEND WX_TARGETS wx::adv)
endif()

add_executable(invoice_helper
    src/main.cpp
    src/app/App.cpp
    src/model/InvoiceDocument.cpp
    src/ui/MainFrame.cpp
    src/ui/InvoiceDropTarget.cpp
    src/parser/FileDetector.cpp
    src/parser/Invoice.cpp
    src/parser/InvoiceParser.cpp
    src/parser/PdfText.cpp
    src/print/PdfPrint.cpp
)

target_include_directories(invoice_helper PRIVATE src)
target_link_libraries(invoice_helper PRIVATE ${WX_TARGETS} unofficial::libmupdf::libmupdf)
invoice_helper_set_warnings(invoice_helper)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(invoice_helper PRIVATE stdc++fs)
endif()

if (MINGW)
    target_link_options(invoice_helper PRIVATE -static)
endif()

if (WIN32)
    set_target_properties(invoice_helper PROPERTIES WIN32_EXECUTABLE TRUE)
    enable_language(RC)
    if (CMAKE_RC_COMPILER MATCHES "windres")
        set_source_files_properties(resources/invoice_helper.rc PROPERTIES COMPILE_FLAGS "-c 65001")
    endif()
    target_sources(invoice_helper PRIVATE resources/invoice_helper.rc)
endif()

add_executable(invoice_parser_smoke
    tests/parser_smoke.cpp
    src/parser/Invoice.cpp
    src/parser/InvoiceParser.cpp
    src/parser/PdfText.cpp
)
target_include_directories(invoice_parser_smoke PRIVATE src)
target_link_libraries(invoice_parser_smoke PRIVATE unofficial::libmupdf::libmupdf)
invoice_helper_set_warnings(invoice_parser_smoke)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(invoice_parser_smoke PRIVATE stdc++fs)
endif()
