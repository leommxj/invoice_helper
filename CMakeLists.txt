cmake_minimum_required(VERSION 3.20)

project(invoice_helper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(wxWidgets CONFIG REQUIRED)

set(MUPDF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mupdf" CACHE PATH "MuPDF installation root")
if (NOT EXISTS "${MUPDF_ROOT}/include/mupdf/fitz.h")
    message(FATAL_ERROR "MuPDF not found at ${MUPDF_ROOT}. Run the workspace dependency setup first.")
endif()

add_library(mupdf INTERFACE)
target_include_directories(mupdf INTERFACE "${MUPDF_ROOT}/include")
target_link_libraries(mupdf INTERFACE
    "${MUPDF_ROOT}/lib/libmupdf.a"
    "${MUPDF_ROOT}/lib/libmupdf-third.a"
)
if (NOT MSVC)
    target_link_libraries(mupdf INTERFACE m)
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
target_link_libraries(invoice_helper PRIVATE wx::wxmono mupdf)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(invoice_helper PRIVATE stdc++fs)
endif()

if (MINGW)
    target_link_options(invoice_helper PRIVATE -static)
endif()

if (MSVC)
    target_compile_options(invoice_helper PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(invoice_helper PRIVATE -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
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
target_link_libraries(invoice_parser_smoke PRIVATE mupdf)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(invoice_parser_smoke PRIVATE stdc++fs)
endif()
