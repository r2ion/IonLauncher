if(NOT silver-bun_FOUND)
    add_subdirectory(${PROJECT_SOURCE_DIR}/src/thirdparty/silver-bun silver-bun)
    set(silver-bun_FOUND
        1
        PARENT_SCOPE
        )
endif()
