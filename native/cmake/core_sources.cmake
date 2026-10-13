# The core's units and defines, read from web/build-core.sh's em++ line, so the native build and the wasm build compile the
# same sources: a unit added there is picked up here at the next configure (the script is a configure dependency).
# The wasm-only rider-context files (web/rider_context.cpp, web/rider_tls.S: the __tls_base swap) are left out; the native
# build has native/core/rider_context_native.cpp instead.
function(ssx_core_sources build_script out_units out_defines)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${build_script}")
    file(STRINGS "${build_script}" lines REGEX "^\"\\$EMXX\"")
    list(LENGTH lines count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "Expected one em++ compile line in ${build_script}, found ${count}")
    endif()
    # the sources: web/... or engine/... .cpp words
    string(REGEX MATCHALL "(web|engine)/[A-Za-z0-9_/]+\\.cpp" units "${lines}")
    list(REMOVE_ITEM units web/rider_context.cpp)
    list(REMOVE_DUPLICATES units)
    if(NOT units)
        message(FATAL_ERROR "No core sources found in ${build_script}")
    endif()
    # the -D defines on that line (their names only, as -DNAME or -DNAME=VALUE)
    string(REGEX MATCHALL " -D[A-Za-z0-9_]+(=[^ ]*)?" raw_defines "${lines}")
    set(defines "")
    foreach(d IN LISTS raw_defines)
        string(STRIP "${d}" d)
        string(SUBSTRING "${d}" 2 -1 d)
        list(APPEND defines "${d}")
    endforeach()
    set(${out_units} "${units}" PARENT_SCOPE)
    set(${out_defines} "${defines}" PARENT_SCOPE)
endfunction()
