# Special treatment for x87 and x86-32 SSE (see GitHub issue #4324)
include(FindX87Math)

# Specify compile/link flags and definitions.
#
# If generator expressions are used, make sure to update rt_get_target_flags()
# to remove the generator expression. Otherwise, configuration will fail with
# and error about using generator expressions in add_custom_target().
# This comment is specifically about add_custom_target(UpdateInfo ...)
function(rt_setup_target CXX_TARGET)
    # Set globally in project CMakeLists.txt
    # target_compile_features(${CXX_TARGET} PUBLIC cxx_std_17)
    # set_target_properties(
    #     ${CXX_TARGET} PROPERTIES
    #     CXX_STANDARD_REQUIRED ON
    #     CXX_EXTENSIONS OFF
    # )

    set(COMPILE_DEFS "")
    set(COMPILE_OPTS "")
    set(CXX_ONLY_COMPILE_OPTS "")
    set(LINK_OPTS "")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND COMPILE_DEFS -D_DEBUG)
    else()
        list(APPEND COMPILE_DEFS NDEBUG)
    endif()

    # MyMutex:
    if(STRICT_MUTEX OR CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND COMPILE_DEFS STRICT_MUTEX=1)
    else()
        list(APPEND COMPILE_DEFS STRICT_MUTEX=0)
    endif()

    # MyRWMutex:
    if(TRACE_MYRWMUTEX)
        list(APPEND COMPILE_DEFS TRACE_MYRWMUTEX=1)
    else()
        list(APPEND COMPILE_DEFS TRACE_MYRWMUTEX=0)
    endif()

    if(AUTO_GDK_FLUSH)
        list(APPEND COMPILE_DEFS AUTO_GDK_FLUSH=1)
    else()
        list(APPEND COMPILE_DEFS AUTO_GDK_FLUSH=0)
    endif()

    if(WITH_MYFILE_MMAP)
        list(APPEND COMPILE_DEFS MYFILE_MMAP)
    endif()

    if(WITH_SAN)
        list(APPEND COMPILE_OPTS "-fsanitize=${WITH_SAN}" -fno-omit-frame-pointer)
        list(APPEND LINK_OPTS    "-fsanitize=${WITH_SAN}")
    endif()

    if(WITH_PROF)
        list(APPEND COMPILE_OPTS -pg)
        list(APPEND LINK_OPTS    -pg)
    endif()

    if(WIN32)
        list(APPEND COMPILE_DEFS -DWIN32 -D_WIN32)
        if(MINGW)
            list(APPEND COMPILE_DEFS -D_D__MINGW32__)
        endif()
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            list(APPEND COMPILE_DEFS WINVER=0x0501)
        endif()
    endif()

    if(WARNINGS_AS_ERRORS)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            list(APPEND COMPILE_OPTS /WX)
        else()
            # Works for GCC, Clang, and AppleClang
            list(APPEND COMPILE_OPTS -Werror)
        endif()
        # (CMake >= 3.24): set_target_properties(${CXX_TARGET} PROPERTIES COMPILE_WARNING_AS_ERROR ON)
    endif()

    # list(APPEND COMPILE_OPTS -Wall -Wextra)
    list(APPEND COMPILE_OPTS
        -Wall -Wuninitialized -Wcast-qual
        -Wno-deprecated-declarations -Wno-unused-result -Wunused-macros
    )

    # Stop compilation on typos such as std:swap (missing colon will be
    # detected as unused label):
    list(APPEND COMPILE_OPTS -Werror=unused-label)
    list(APPEND CXX_ONLY_COMPILE_OPTS -Werror=delete-incomplete)

    # Do net set math errno, as we never check its value.
    list(APPEND COMPILE_OPTS -fno-math-errno)

    # Suppress warning https://github.com/Beep6581/RawTherapee/issues/6105
    list(APPEND COMPILE_OPTS -Wno-attributes)

    # Special treatment for x87 and x86-32 SSE (see GitHub issue #4324)
    # Requires include(FindX87Math)
    if(HAVE_X87_MATH)
        list(APPEND COMPILE_OPTS -ffloat-store)
    endif()
    if(HAVE_X86_SSE_MATH)
        list(APPEND COMPILE_OPTS -msse2 -mfpmath=sse)
    endif()

    if(WITH_TRACY_MEMORY_PROFILING)
        list(APPEND COMPILE_OPTS -fno-omit-frame-pointer)
    endif()

    target_compile_definitions(${CXX_TARGET} PUBLIC ${COMPILE_DEFS})
    target_compile_options(${CXX_TARGET} PUBLIC ${COMPILE_OPTS})
    target_compile_options(${CXX_TARGET} PUBLIC
                           $<$<COMPILE_LANGUAGE:CXX>:${CXX_ONLY_COMPILE_OPTS}>)

    get_target_property(TARGET_TYPE ${CXX_TARGET} TYPE)
    # Ignoring shared library targets since previous code only modifies
    # CMAKE_EXE_LINKER_FLAGS which only affects executable targets.
    if(TARGET_TYPE STREQUAL "EXECUTABLE")
        target_link_options(${CXX_TARGET} PUBLIC ${LINK_OPTS})
    endif()

    if(WITH_LTO)
        set_target_properties(${CXX_TARGET} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endfunction()

function(rt_get_target_flags OUTPUT_VAR CXX_TARGET PROPERTY_NAME GLOBAL_VAR)
    get_target_property(PROP ${CXX_TARGET} ${PROPERTY_NAME})

    set(PROPERTY_VALUE "")
    if(NOT PROP STREQUAL "PROP-NOTFOUND")
        string(REGEX REPLACE "\\$<.*:(.*)>" "\\1" NO_GENERATOR_EXPR_PROP "${PROP}")
        string(REPLACE ";" " " PROPERTY_VALUE "${NO_GENERATOR_EXPR_PROP}")
    endif()

    string(TOUPPER ${CMAKE_BUILD_TYPE} UPPER_CMAKE_BUILD_TYPE)
    set(GLOBAL_VAR_CONFIG ${GLOBAL_VAR}_${UPPER_CMAKE_BUILD_TYPE})

    set(
        ${OUTPUT_VAR}
        "${${GLOBAL_VAR}} ${${GLOBAL_VAR_CONFIG}} ${PROPERTY_VALUE}"
        PARENT_SCOPE
    )
endfunction()
