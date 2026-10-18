# Focused developer test. Never registered or run by ordinary builds.
cmake_minimum_required(VERSION 3.20)
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef token)
set(work "${SIM4000_TEST_OUTPUT}/binary-dist-test-${token}")
if(EXISTS "${work}")
    message(FATAL_ERROR "Test workspace already exists")
endif()
file(MAKE_DIRECTORY "${work}")
set(script "${SIM4000_SOURCE}/dev/cmake/package_binary.cmake")
set(name sim4000-test.tar.gz)
set(args "-DSIM4000_SOURCE=${SIM4000_SOURCE}"
         "-DSIM4000_BINARY=${SIM4000_BINARY}"
         "-DSIM4000_OUTPUT=${work}"
         "-DSIM4000_ARCHIVE_NAME=${name}")
execute_process(COMMAND "${CMAKE_COMMAND}" ${args} -P "${script}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Packaging failed")
endif()
set(archive "${work}/${name}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive}"
                OUTPUT_VARIABLE listing RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR listing MATCHES "(^|\n)(/|[^s])" OR
   listing MATCHES "(^|\n)sim4000/.*\\.\\./")
    message(FATAL_ERROR "Unsafe archive path")
endif()
string(REPLACE "\n" ";" entries "${listing}")
foreach(path IN LISTS entries)
    if(NOT path STREQUAL "" AND NOT path MATCHES "^sim4000(/|$)")
        message(FATAL_ERROR "Archive has another top-level entry: ${path}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${work}/extract")
file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${work}/extract")
set(root "${work}/extract/sim4000")
set(source_files
    src/config/RC4000.cfg src/monitor/MONITOR src/disks/UNIT0 README.md LICENSE
    src/config/RC4000.protected.cfg src/monitor/MONITOR.protected
    src/disks/UNIT0.protected doc/PROFILES.txt src/config/RC4000.mipsts.cfg
    src/disks/UNIT0.mipsts src/disks/UNIT1.mipsts
    doc/usermanual.pdf doc/OVERVIEW.md licenses/GPL-3.0.txt doc/impl/V_1.2_DB_LICENSE.txt)
foreach(path IN LISTS source_files)
    get_filename_component(base "${path}" NAME)
    if(path STREQUAL "doc/usermanual.pdf" OR path STREQUAL "doc/OVERVIEW.md")
        set(destination "${root}/doc/${base}")
    elseif(path MATCHES "LICENSE.txt$" OR path MATCHES "GPL-3.0.txt$")
        set(destination "${root}/licenses/${base}")
    else()
        set(destination "${root}/${base}")
    endif()
    file(SHA256 "${SIM4000_SOURCE}/${path}" expected)
    file(SHA256 "${destination}" actual)
    if(NOT expected STREQUAL actual)
        message(FATAL_ERROR "Packaged file differs: ${path}")
    endif()
endforeach()
file(SHA256 "${SIM4000_BINARY}" expected)
file(SHA256 "${root}/sim4000" actual)
if(NOT expected STREQUAL actual OR NOT IS_DIRECTORY "${root}/exchange")
    message(FATAL_ERROR "Executable or exchange directory missing")
endif()
foreach(path IN ITEMS dev tests src CMakeLists.txt doc/impl UNIT0.overlay
                      UNIT0.original STDMON RC4000.dfc403.cfg protected
                      UNIT0.mipsts.overlay UNIT1.mipsts.overlay)
    if(EXISTS "${root}/${path}")
        message(FATAL_ERROR "Non-runtime content in package: ${path}")
    endif()
endforeach()
file(GLOB examples "${root}/examples/*")
list(LENGTH examples count)
if(NOT count EQUAL 15)
    message(FATAL_ERROR "Tutorial input count differs")
endif()
foreach(path IN LISTS examples)
    get_filename_component(base "${path}" NAME)
    file(SHA256 "${path}" actual)
    file(SHA256 "${SIM4000_SOURCE}/doc/usermanual/examples/${base}" expected)
    if(NOT expected STREQUAL actual)
        message(FATAL_ERROR "Example differs: ${base}")
    endif()
endforeach()

# Both compatibility names select the same preserved profile.
file(SHA256 "${root}/RC4000.cfg" expected)
file(SHA256 "${root}/RC4000.compat.cfg" actual)
if(NOT expected STREQUAL actual)
    message(FATAL_ERROR "Explicit compatibility config differs from default")
endif()

# Test executable permission and runtime independence with both packaged profiles.
file(WRITE "${work}/empty-input" "")
foreach(profile IN ITEMS RC4000.cfg RC4000.compat.cfg RC4000.protected.cfg RC4000.mipsts.cfg)
execute_process(COMMAND "${root}/sim4000" --check-config "${root}/${profile}"
    OUTPUT_VARIABLE config ERROR_VARIABLE error RESULT_VARIABLE result)
if(profile STREQUAL "RC4000.protected.cfg")
    set(mode rc4000)
else()
    set(mode off)
endif()
if(NOT result EQUAL 0 OR NOT config MATCHES "protection: ${mode}\n" OR
   NOT config MATCHES "cpu: RC4000 ix=on")
    message(FATAL_ERROR "Packaged profile selection failed: ${profile}: ${error}")
endif()
execute_process(
    COMMAND "${root}/sim4000" --run --config "${root}/${profile}"
            --steps 40000000 --set-disc-load-flag
    WORKING_DIRECTORY "${work}" INPUT_FILE "${work}/empty-input"
    OUTPUT_VARIABLE output ERROR_VARIABLE error RESULT_VARIABLE result
    TIMEOUT 30)
if(profile STREQUAL "RC4000.mipsts.cfg")
    set(marker "sos started")
    if(output MATCHES "initialize date:" OR output MATCHES "not allowed")
        message(FATAL_ERROR "MIPS/TS did not start unattended: ${output}")
    endif()
else()
    set(marker "initialize date:")
endif()
if(NOT result EQUAL 0 OR NOT output MATCHES "${marker}")
    message(FATAL_ERROR "Packaged profile failed: ${profile}: ${error}")
endif()
endforeach()
# Volatile boots must not modify either packaged base disk or monitor.
foreach(path IN ITEMS MONITOR UNIT0 MONITOR.protected UNIT0.protected UNIT0.mipsts UNIT1.mipsts)
    if(path MATCHES "^MONITOR")
        set(source "src/monitor/MONITOR")
    else()
        set(source "src/disks/${path}")
    endif()
    if(path STREQUAL "MONITOR.protected")
        set(source "src/monitor/MONITOR.protected")
    elseif(path STREQUAL "UNIT0.protected")
        set(source "src/disks/UNIT0.protected")
    endif()
    file(SHA256 "${SIM4000_SOURCE}/${source}" expected)
    file(SHA256 "${root}/${path}" actual)
    if(NOT expected STREQUAL actual)
        message(FATAL_ERROR "Packaged base changed during boot: ${path}")
    endif()
endforeach()

# Refuse unsafe modes/missing inputs without replacing a valid prior archive.
file(SHA256 "${archive}" before)
execute_process(
    COMMAND "${CMAKE_COMMAND}" ${args} -DSIM4000_SANITIZERS=ON -P "${script}"
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(result EQUAL 0)
    message(FATAL_ERROR "Sanitizer package was accepted")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" ${args}
            "-DSIM4000_SOURCE=${work}/missing-source" -P "${script}"
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
file(SHA256 "${archive}" after)
if(result EQUAL 0 OR NOT before STREQUAL after)
    message(FATAL_ERROR "Failed packaging replaced the prior archive")
endif()

# A missing or changed protected artifact must not silently produce a
# compatibility-only archive, nor replace an existing complete archive.
set(bad_source "${work}/bad-source")
foreach(path IN LISTS source_files)
    configure_file("${SIM4000_SOURCE}/${path}" "${bad_source}/${path}" COPYONLY)
endforeach()
foreach(path IN LISTS examples)
    get_filename_component(base "${path}" NAME)
    configure_file("${path}" "${bad_source}/doc/usermanual/examples/${base}" COPYONLY)
endforeach()
file(RENAME "${bad_source}/src/monitor/MONITOR.protected"
            "${bad_source}/src/monitor/MONITOR.protected.saved")
foreach(failure IN ITEMS missing changed)
    if(failure STREQUAL "changed")
        file(WRITE "${bad_source}/src/monitor/MONITOR.protected" "not verified")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${args}
                "-DSIM4000_SOURCE=${bad_source}" -P "${script}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(SHA256 "${archive}" after)
    if(result EQUAL 0 OR NOT before STREQUAL after OR
       NOT error MATCHES "(Required distribution file missing|Unverified protected profile artifact)")
        message(FATAL_ERROR "Protected ${failure} input was not safely rejected: ${error}")
    endif()
endforeach()
file(GLOB leftovers "${work}/package-*")
if(leftovers)
    message(FATAL_ERROR "Owned staging directories were not cleaned")
endif()
file(REMOVE_RECURSE "${work}")
message(STATUS "Binary archive paths, contents, boot and failure handling passed")
