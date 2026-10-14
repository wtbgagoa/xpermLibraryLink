# Documentation is opt-in: normal builds need neither Doxygen nor Graphviz.
option(BUILD_DOCUMENTATION "Enable the Doxygen documentation target" OFF)
option(DOCUMENTATION_WARNINGS_AS_ERRORS "Fail documentation generation on Doxygen warnings" ON)
option(DOCUMENTATION_LATEX "Generate LaTeX sources alongside HTML documentation" OFF)
option(DOCUMENTATION_PDF "Enable the docs-pdf target (also generates LaTeX sources)" OFF)

if(BUILD_DOCUMENTATION)
    find_package(Doxygen 1.9.1 REQUIRED COMPONENTS doxygen OPTIONAL_COMPONENTS dot)
    set(XPERM_DOC_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/documentation")
    set(XPERM_DOC_HAVE_DOT NO)
    set(XPERM_DOC_DOT_PATH "")
    if(DOXYGEN_DOT_FOUND)
        set(XPERM_DOC_HAVE_DOT YES)
        get_filename_component(XPERM_DOC_DOT_PATH "${DOXYGEN_DOT_EXECUTABLE}" DIRECTORY)
    endif()
    set(XPERM_DOC_WARN_AS_ERROR NO)
    if(DOCUMENTATION_WARNINGS_AS_ERRORS)
        set(XPERM_DOC_WARN_AS_ERROR YES)
    endif()
    set(XPERM_DOC_GENERATE_LATEX NO)
    if(DOCUMENTATION_LATEX OR DOCUMENTATION_PDF)
        set(XPERM_DOC_GENERATE_LATEX YES)
    endif()
    file(MAKE_DIRECTORY "${XPERM_DOC_OUTPUT}")
    configure_file("${PROJECT_SOURCE_DIR}/docs/Doxyfile.in"
                   "${XPERM_DOC_OUTPUT}/Doxyfile" @ONLY)
    # Intentionally run on each explicit request, so edits to comments and
    # guide pages cannot be missed by a partial dependency list.
    add_custom_target(docs
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${XPERM_DOC_OUTPUT}"
        COMMAND Doxygen::doxygen "${XPERM_DOC_OUTPUT}/Doxyfile"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Generating xPermLibraryLink algorithm guide and API reference"
        VERBATIM)
    message(STATUS "Doxygen HTML: ${XPERM_DOC_OUTPUT}/html/index.html")
    if(DOCUMENTATION_LATEX OR DOCUMENTATION_PDF)
        message(STATUS "Doxygen LaTeX: ${XPERM_DOC_OUTPUT}/latex/refman.tex")
    endif()
    if(DOCUMENTATION_PDF)
        find_program(XPERM_DOC_LATEXMK_EXECUTABLE NAMES latexmk)
        if(NOT XPERM_DOC_LATEXMK_EXECUTABLE)
            message(FATAL_ERROR "DOCUMENTATION_PDF requires latexmk and a TeX distribution with pdflatex and makeindex. Use DOCUMENTATION_LATEX=ON without DOCUMENTATION_PDF to generate sources only.")
        endif()
        # latexmk handles index generation and the passes needed to resolve
        # references. Invoke it directly, independently of the CMake generator.
        add_custom_target(docs-pdf
            COMMAND "${XPERM_DOC_LATEXMK_EXECUTABLE}"
                    -pdf -interaction=nonstopmode -halt-on-error refman.tex
            WORKING_DIRECTORY "${XPERM_DOC_OUTPUT}/latex"
            COMMENT "Compiling the xPermLibraryLink PDF manual"
            VERBATIM)
        add_dependencies(docs-pdf docs)
        message(STATUS "Doxygen PDF: ${XPERM_DOC_OUTPUT}/latex/refman.pdf")
    endif()
endif()
