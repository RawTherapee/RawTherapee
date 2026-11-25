function(rt_generate_about_command OUTPUT_VAR ENGINE_TARGET GUI_TARGET)
    set(ABOUT_COMMAND_WITH_ARGS "")

    # Set the platform bit-depth
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(PROC_BIT_DEPTH 32 bits)
    else()
        set(PROC_BIT_DEPTH 64 bits)
    endif()

    # Get compiler name and version.
    get_filename_component(COMPILER_INFO ${CMAKE_C_COMPILER} NAME_WE)
    set(COMPILER_INFO "${COMPILER_INFO} ${CMAKE_C_COMPILER_VERSION}")

    # Get C++ and linker flags for rtengine (GUI's flags may have fewer)
    rt_get_target_flags(CXX_FLAGS ${ENGINE_TARGET} COMPILE_OPTIONS CMAKE_CXX_FLAGS)
    rt_get_target_flags(LFLAGS ${GUI_TARGET} LINK_OPTIONS CMAKE_EXE_LINKER_FLAGS)

    if(NOT APPLE)
        set(ABOUT_COMMAND_WITH_ARGS
            ${CMAKE_COMMAND} -DPROJECT_SOURCE_DIR:STRING=${PROJECT_SOURCE_DIR}
            -DCACHE_NAME_SUFFIX:STRING=${CACHE_NAME_SUFFIX}
            -DPROC_LABEL:STRING="${PROC_LABEL}"
            -DPROC_BIT_DEPTH:STRING="${PROC_BIT_DEPTH}"
            -DBUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
            -DGTKMM_VERSION:STRING=${GTKMM_VERSION}
            -DOPTION_OMP:STRING=${OPTION_OMP}
            -DWITH_MYFILE_MMAP:STRING=${WITH_MYFILE_MMAP}
            -DJXL_VERSION:STRING=${JXL_VERSION}
            -DAVIF_VERSION:STRING=${AVIF_VERSION}
            -DLENSFUN_VERSION:STRING=${LENSFUN_VERSION})
    endif()

    if(WIN32)
        list(
            APPEND
            ABOUT_COMMAND_WITH_ARGS
            -DSYSTEM:STRING=Windows
            -DCXX_FLAGS:STRING=${CXX_FLAGS}
            -DLFLAGS:STRING=${LFLAGS}
            -DCOMPILER_INFO:STRING=${COMPILER_INFO}
            -DCMAKE_INSTALL_PREFIX:STRING="${CMAKE_INSTALL_PREFIX}"
            -DBIT_DEPTH:STRING="${CMAKE_SIZEOF_VOID_P}")
    elseif(APPLE)
        set(
             ABOUT_COMMAND_WITH_ARGS
             ${CMAKE_COMMAND}
                 -DPROJECT_SOURCE_DIR:STRING=${PROJECT_SOURCE_DIR}
                 -DCACHE_NAME_SUFFIX:STRING=${CACHE_NAME_SUFFIX}
                 -DSYSTEM:STRING=Apple
                 -DCXX_FLAGS:STRING=${CXX_FLAGS}
                 -DLFLAGS:STRING=${LFLAGS}
                 -DCOMPILER_INFO:STRING=${COMPILER_INFO}
                 -DPROC_LABEL:STRING="${PROC_LABEL}"
                 -DPROC_BIT_DEPTH:STRING="${PROC_BIT_DEPTH}"
                 -DBUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
                 -DGTKMM_VERSION:STRING=${GTKMM_VERSION}
                 -DOPTION_OMP:STRING=${OPTION_OMP}
                 -DWITH_MYFILE_MMAP:STRING=${WITH_MYFILE_MMAP}
                 -DJXL_VERSION:STRING=${JXL_VERSION}
                 -DAVIF_VERSION:STRING=${AVIF_VERSION}
                 -DLENSFUN_VERSION:STRING=${LENSFUN_VERSION}
                 -P ${PROJECT_SOURCE_DIR}/UpdateInfo.cmake)
    else()
        list(APPEND ABOUT_COMMAND_WITH_ARGS -DSYSTEM:STRING=Linux
             -DCXX_FLAGS:STRING=${CXX_FLAGS} -DLFLAGS:STRING=${LFLAGS}
             -DCOMPILER_INFO:STRING=${COMPILER_INFO})
    endif()
    if(NOT APPLE)
        list(APPEND
             ABOUT_COMMAND_WITH_ARGS
                 -P "${PROJECT_SOURCE_DIR}/UpdateInfo.cmake")
    endif()

    set(${OUTPUT_VAR} ${ABOUT_COMMAND_WITH_ARGS} PARENT_SCOPE)
endfunction()
