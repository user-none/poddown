project(poddown VERSION 1.0.0)

set(SOURCES
    "cast.c"
    "cpthread.c"
    "downloader.c"
    "main.c"
    "rw_files.c"
    "settings.c"
    "str_builder.c"
    "str_helpers.c"
    "tpool.c"
    "xmem.c"
    "xml_helpers.c"
)

if(APPLE)
    list(APPEND SOURCES
        "settings_mac.m"
    )
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

if(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "_DARWIN_C_SOURCE")
else(UNIX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "_XOPEN_SOURCE=600")
endif()
target_include_directories(${PROJECT_NAME}
    PUBLIC "${LIBXML2_INCLUDE_DIR}"
        "${CURL_INCLUDE_DIR}"
)
target_link_libraries(${PROJECT_NAME}
    "${CMAKE_THREAD_LIBS_INIT}" "${LIBXML2_LIBRARIES}" "${CURL_LIBRARIES}"
)
if(APPLE)
    find_library(FOUNDATION_LIBRARY Foundation)
    target_link_libraries(${PROJECT_NAME} ${FOUNDATION_LIBRARY})
endif()
