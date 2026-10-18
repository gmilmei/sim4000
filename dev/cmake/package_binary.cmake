# Explicit developer packaging only; no Python, tests or document generation.
cmake_minimum_required(VERSION 3.20)

foreach(variable IN ITEMS SIM4000_SOURCE SIM4000_BINARY SIM4000_OUTPUT
                          SIM4000_ARCHIVE_NAME)
    if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
        message(FATAL_ERROR "Missing packaging argument: ${variable}")
    endif()
endforeach()
if(SIM4000_SANITIZERS)
    message(FATAL_ERROR "Do not package a sanitizer build; use a normal Release build")
endif()
if(NOT SIM4000_ARCHIVE_NAME MATCHES "^sim4000-[A-Za-z0-9_.-]+\\.tar\\.gz$")
    message(FATAL_ERROR "Invalid archive name")
endif()

# Package pristine source media/configuration, never writable build-tree media.
set(runtime_files
    src/config/RC4000.cfg
    src/config/RC4000.protected.cfg
    src/config/RC4000.mipsts.cfg
    src/monitor/MONITOR
    src/disks/UNIT0
    src/disks/UNIT0.mipsts
    src/disks/UNIT1.mipsts
    src/monitor/MONITOR.protected
    src/disks/UNIT0.protected)
set(document_files
    README.md
    LICENSE
    doc/usermanual.pdf
    doc/OVERVIEW.md
    doc/PROFILES.txt
    doc/impl/V_1.2_DB_LICENSE.txt
    licenses/GPL-3.0.txt)
set(example_files
    README.txt hello.alg print_source.alg primo_print.alg editor_lines.txt slang_integer.sl slang_error.sl
    slang_commands.fp nested_outer.fp nested_inner.fp conditional_commands.fp
    assembly_status.fp command_error.fp command_error_outer.fp assembly_error.fp)
foreach(path IN LISTS example_files)
    list(APPEND document_files "doc/usermanual/examples/${path}")
endforeach()
foreach(path IN LISTS runtime_files document_files)
    if(NOT EXISTS "${SIM4000_SOURCE}/${path}" OR
       IS_DIRECTORY "${SIM4000_SOURCE}/${path}")
        message(FATAL_ERROR "Required distribution file missing: ${path}")
    endif()
endforeach()
foreach(kind IN ITEMS monitor disks)
    if(kind STREQUAL "monitor")
        set(expected 9055b2f5aeaa9a5bc4e7f57a8bd5dd38cca6aceb1c5c98eb4deeeecd936cda62)
        set(path src/monitor/MONITOR.protected)
    else()
        set(expected ebc56434088ac0d0a5655924646a45bfd3d739bbb2217f45622c9626641f2483)
        set(path src/disks/UNIT0.protected)
    endif()
    file(SHA256 "${SIM4000_SOURCE}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Unverified protected profile artifact: ${path}")
    endif()
endforeach()
foreach(name IN ITEMS UNIT0.mipsts UNIT1.mipsts)
    if(name STREQUAL "UNIT0.mipsts")
        set(expected c3200b837e09ee0af054b47e7ac1b447f61c46c0e34a9fa4eae080da4c0e4c75)
    else()
        set(expected 63e941705e51bb9b61bc005af54929e5798b57e78bdb188506934140273c47ba)
    endif()
    file(SHA256 "${SIM4000_SOURCE}/src/disks/${name}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Unverified MIPS/TS profile artifact: ${name}")
    endif()
endforeach()
if(NOT EXISTS "${SIM4000_BINARY}" OR IS_DIRECTORY "${SIM4000_BINARY}")
    message(FATAL_ERROR "Built emulator missing: ${SIM4000_BINARY}")
endif()
if(NOT IS_DIRECTORY "${SIM4000_SOURCE}/doc/usermanual/examples")
    message(FATAL_ERROR "Tutorial examples directory missing")
endif()

# A new owned workspace prevents stale files from leaking into the archive.
file(MAKE_DIRECTORY "${SIM4000_OUTPUT}")
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef token)
set(workspace "${SIM4000_OUTPUT}/package-${token}")
if(EXISTS "${workspace}")
    message(FATAL_ERROR "Packaging workspace already exists: ${workspace}")
endif()
set(root "${workspace}/sim4000")
file(MAKE_DIRECTORY "${root}/doc" "${root}/licenses" "${root}/exchange")
configure_file("${SIM4000_BINARY}" "${root}/sim4000" COPYONLY)
file(CHMOD "${root}/sim4000"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                 GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
foreach(path IN ITEMS src/config/RC4000.cfg src/config/RC4000.protected.cfg
                      src/config/RC4000.mipsts.cfg src/disks/UNIT0.mipsts
                      src/disks/UNIT1.mipsts src/monitor/MONITOR src/disks/UNIT0)
    file(COPY "${SIM4000_SOURCE}/${path}" DESTINATION "${root}")
endforeach()
configure_file("${SIM4000_SOURCE}/src/config/RC4000.cfg"
               "${root}/RC4000.compat.cfg" COPYONLY)
configure_file("${SIM4000_SOURCE}/src/monitor/MONITOR.protected"
               "${root}/MONITOR.protected" COPYONLY)
configure_file("${SIM4000_SOURCE}/src/disks/UNIT0.protected"
               "${root}/UNIT0.protected" COPYONLY)
file(COPY "${SIM4000_SOURCE}/doc/PROFILES.txt" DESTINATION "${root}")
file(COPY "${SIM4000_SOURCE}/README.md" "${SIM4000_SOURCE}/LICENSE"
     DESTINATION "${root}")
file(COPY "${SIM4000_SOURCE}/doc/usermanual.pdf"
          "${SIM4000_SOURCE}/doc/OVERVIEW.md" DESTINATION "${root}/doc")
file(COPY "${SIM4000_SOURCE}/doc/impl/V_1.2_DB_LICENSE.txt"
          "${SIM4000_SOURCE}/licenses/GPL-3.0.txt"
     DESTINATION "${root}/licenses")
file(MAKE_DIRECTORY "${root}/examples")
foreach(path IN LISTS example_files)
    file(COPY "${SIM4000_SOURCE}/doc/usermanual/examples/${path}"
         DESTINATION "${root}/examples")
endforeach()

# Normalize data permissions irrespective of the packager's umask.
file(GLOB_RECURSE data_files LIST_DIRECTORIES FALSE "${root}/*")
list(REMOVE_ITEM data_files "${root}/sim4000")
foreach(path IN LISTS data_files)
    file(CHMOD "${path}" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endforeach()
file(GLOB_RECURSE directories LIST_DIRECTORIES TRUE "${root}/*")
list(APPEND directories "${root}")
foreach(path IN LISTS directories)
    if(IS_DIRECTORY "${path}")
        file(CHMOD "${path}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                                        GROUP_READ GROUP_EXECUTE
                                        WORLD_READ WORLD_EXECUTE)
    endif()
endforeach()

set(archive "${SIM4000_OUTPUT}/${SIM4000_ARCHIVE_NAME}")
set(temporary "${workspace}/archive.tar.gz")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cfz "${temporary}" --format=gnutar sim4000
    WORKING_DIRECTORY "${workspace}"
    RESULT_VARIABLE result
    ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    file(REMOVE_RECURSE "${workspace}")
    message(FATAL_ERROR "Archive creation failed: ${error}")
endif()
file(RENAME "${temporary}" "${archive}")
file(REMOVE_RECURSE "${workspace}")
message(STATUS "Binary distribution: ${archive}")
