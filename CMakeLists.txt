cmake_minimum_required(VERSION 3.20)

project(sim4000 VERSION 0.1.0 LANGUAGES C)

# Documentation is optional and never part of the ordinary emulator build.
find_program(SIM4000_LUALATEX_EXECUTABLE lualatex)
find_program(SIM4000_MAKEINDEX_EXECUTABLE makeindex)
if(SIM4000_LUALATEX_EXECUTABLE AND SIM4000_MAKEINDEX_EXECUTABLE)
    file(GLOB SIM4000_MANUAL_SOURCES CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/doc/usermanual/*.tex")
    set(SIM4000_MANUAL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/usermanual")
    add_custom_command(
        OUTPUT "${SIM4000_MANUAL_OUTPUT}/usermanual.pdf"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SIM4000_MANUAL_OUTPUT}"
        COMMAND ${SIM4000_LUALATEX_EXECUTABLE} -interaction=nonstopmode
                -halt-on-error -file-line-error
                "-output-directory=${SIM4000_MANUAL_OUTPUT}" usermanual.tex
        COMMAND ${CMAKE_COMMAND} -E chdir "${SIM4000_MANUAL_OUTPUT}"
                ${SIM4000_MAKEINDEX_EXECUTABLE} usermanual.idx
        COMMAND ${SIM4000_LUALATEX_EXECUTABLE} -interaction=nonstopmode
                -halt-on-error -file-line-error
                "-output-directory=${SIM4000_MANUAL_OUTPUT}" usermanual.tex
        COMMAND ${SIM4000_LUALATEX_EXECUTABLE} -interaction=nonstopmode
                -halt-on-error -file-line-error
                "-output-directory=${SIM4000_MANUAL_OUTPUT}" usermanual.tex
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/doc/usermanual"
        DEPENDS ${SIM4000_MANUAL_SOURCES}
        VERBATIM
    )
    add_custom_target(user-manual
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SIM4000_MANUAL_OUTPUT}/usermanual.pdf"
                "${CMAKE_CURRENT_SOURCE_DIR}/doc/usermanual.pdf"
        DEPENDS "${SIM4000_MANUAL_OUTPUT}/usermanual.pdf")
endif()

configure_file(src/config/RC4000.cfg
               ${CMAKE_CURRENT_BINARY_DIR}/RC4000.cfg COPYONLY)
configure_file(src/config/RC4000.cfg
               ${CMAKE_CURRENT_BINARY_DIR}/RC4000.compat.cfg COPYONLY)
configure_file(src/config/RC4000.protected.cfg
               ${CMAKE_CURRENT_BINARY_DIR}/RC4000.protected.cfg COPYONLY)
configure_file(src/monitor/MONITOR.protected
               ${CMAKE_CURRENT_BINARY_DIR}/MONITOR.protected COPYONLY)
configure_file(src/disks/UNIT0.protected
               ${CMAKE_CURRENT_BINARY_DIR}/UNIT0.protected COPYONLY)
configure_file(src/config/RC4000.mipsts.cfg
               ${CMAKE_CURRENT_BINARY_DIR}/RC4000.mipsts.cfg COPYONLY)
configure_file(src/disks/UNIT0.mipsts
               ${CMAKE_CURRENT_BINARY_DIR}/UNIT0.mipsts COPYONLY)
configure_file(src/disks/UNIT1.mipsts
               ${CMAKE_CURRENT_BINARY_DIR}/UNIT1.mipsts COPYONLY)
configure_file(doc/PROFILES.txt
               ${CMAKE_CURRENT_BINARY_DIR}/PROFILES.txt COPYONLY)
configure_file(src/config/RC4000.dfc403.cfg
               ${CMAKE_CURRENT_BINARY_DIR}/RC4000.dfc403.cfg COPYONLY)
configure_file(src/monitor/MONITOR
               ${CMAKE_CURRENT_BINARY_DIR}/MONITOR COPYONLY)
configure_file(src/monitor/STDMON
               ${CMAKE_CURRENT_BINARY_DIR}/STDMON COPYONLY)
configure_file(src/disks/UNIT0
               ${CMAKE_CURRENT_BINARY_DIR}/UNIT0 COPYONLY)
configure_file(src/disks/UNIT0.original
               ${CMAKE_CURRENT_BINARY_DIR}/UNIT0.original COPYONLY)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/exchange)

option(SIM4000_ENABLE_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(sim4000_core
    src/catalog.c
    src/config.c
    src/core.c
    src/core_image.c
    src/cpu.c
    src/decode.c
    src/device.c
    src/dfc403.c
    src/floating.c
    src/image.c
    src/hostfiles.c
    src/machine.c
    src/monitor.c
    src/rc749.c
    src/rc610.c
    src/terminal_v2.c
    src/trace.c
    src/throttle.c
)

target_include_directories(sim4000_core PUBLIC include)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sim4000_core PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
endif()

if(SIM4000_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sim4000_core PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer
    )
    target_link_options(sim4000_core PUBLIC -fsanitize=address,undefined)
endif()

find_path(READLINE_INCLUDE_DIR readline/readline.h REQUIRED)
find_library(READLINE_LIBRARY NAMES readline REQUIRED)
add_executable(sim4000 src/main.c src/telnet.c src/operator.c)
target_include_directories(sim4000 PRIVATE ${READLINE_INCLUDE_DIR})
target_link_libraries(sim4000 PRIVATE sim4000_core ${READLINE_LIBRARY})

# Developer-only opt-in target, intentionally not ALL and independent of
# Python-based media generation. Package the existing PDF; never rebuild it.
string(TOLOWER "${CMAKE_SYSTEM_NAME}" SIM4000_PACKAGE_SYSTEM)
add_custom_target(binary-dist
    COMMAND ${CMAKE_COMMAND}
            "-DSIM4000_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DSIM4000_BINARY=$<TARGET_FILE:sim4000>"
            "-DSIM4000_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dist"
            "-DSIM4000_ARCHIVE_NAME=sim4000-${PROJECT_VERSION}-${SIM4000_PACKAGE_SYSTEM}-${CMAKE_SYSTEM_PROCESSOR}.tar.gz"
            "-DSIM4000_SANITIZERS=${SIM4000_ENABLE_SANITIZERS}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/dev/cmake/package_binary.cmake"
    DEPENDS sim4000
    COMMENT "Package the standalone binary distribution (developer target)"
    VERBATIM)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sim4000 PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
endif()

# Ordinary builds contain only the emulator and prebuilt runtime media.
option(BUILD_TESTING "Build developer tests" OFF)
option(SIM4000_BUILD_DEVELOPER_TOOLS "Enable media-generation targets" OFF)
include(CTest)
if(BUILD_TESTING OR SIM4000_BUILD_DEVELOPER_TOOLS)
    add_executable(sim4000_build_host_image dev/c/build_host_image.c)
    target_link_libraries(sim4000_build_host_image PRIVATE sim4000_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(sim4000_build_host_image PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endif()
option(SIM4000_REQUIRE_PYTHON_TESTS
       "Require Python and the complete integration test suite" OFF)
if(SIM4000_REQUIRE_PYTHON_TESTS)
    if(NOT BUILD_TESTING)
        message(FATAL_ERROR "SIM4000_REQUIRE_PYTHON_TESTS requires BUILD_TESTING")
    endif()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
elseif(SIM4000_BUILD_DEVELOPER_TOOLS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
elseif(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
endif()

if(SIM4000_BUILD_DEVELOPER_TOOLS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/STDMON
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/dev/python/make_standard_monitor.py
                ${CMAKE_CURRENT_BINARY_DIR}/generated/STDMON
                --builder $<TARGET_FILE:sim4000_build_host_image>
        DEPENDS sim4000_build_host_image dev/python/make_standard_monitor.py
                dev/python/rebuild_monitor.py monsrc/sim4000.mops
                monsrc/montxt1 monsrc/montxt2 monsrc/montxt3 monsrc/montxt4
                monsrc/montxt5 monsrc/montxt6 monsrc/montxt7 monsrc/montxt8
                monsrc/montxt9 monsrc/montxt10 monsrc/montxt11 monsrc/mon8s monsrc/mon8init
                dev/python/make_host_image.py dev/python/make_unit0_copy.py
                src/disks/UNIT0 src/monitor/MONITOR
        VERBATIM USES_TERMINAL
    )
    add_custom_target(standard-monitor DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/generated/STDMON)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/UNIT0.host
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/dev/python/make_host_image.py
                ${CMAKE_CURRENT_BINARY_DIR}/UNIT0.host
                --builder $<TARGET_FILE:sim4000_build_host_image>
        DEPENDS sim4000_build_host_image
                dev/python/make_host_image.py dev/python/make_unit0_copy.py
                src/guest/fromhost.alg src/guest/tohost.alg
                src/disks/UNIT0.original src/monitor/MONITOR
        VERBATIM USES_TERMINAL
    )
    add_custom_target(native-image DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/UNIT0.host)
endif()

if(BUILD_TESTING)
    add_executable(sim4000_rc610_tests tests/c/test_rc610.c)
    target_link_libraries(sim4000_rc610_tests PRIVATE sim4000_core)
    add_test(NAME sim4000_rc610 COMMAND sim4000_rc610_tests)
    add_executable(sim4000_rc610_guest_tests tests/c/test_rc610_guest.c)
    target_link_libraries(sim4000_rc610_guest_tests PRIVATE sim4000_core)
    foreach(profile IN ITEMS guest protected dfc403 source source_protected)
        set(printer_mode "")
        set(printer_monitor MONITOR)
        set(printer_disk UNIT0)
        set(printer_source "")
        if(profile STREQUAL "protected")
            set(printer_mode --protected)
            set(printer_monitor MONITOR.protected)
            set(printer_disk UNIT0.protected)
        elseif(profile STREQUAL "dfc403")
            set(printer_mode --dfc403)
            set(printer_monitor STDMON)
        elseif(profile STREQUAL "source")
            set(printer_mode --source)
            set(printer_source ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/print_source.alg)
        elseif(profile STREQUAL "source_protected")
            set(printer_mode --source-protected)
            set(printer_monitor MONITOR.protected)
            set(printer_disk UNIT0.protected)
            set(printer_source ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/print_source.alg)
        endif()
        add_test(NAME sim4000_rc610_${profile}
            COMMAND ${CMAKE_COMMAND}
                "-DBINARY=$<TARGET_FILE:sim4000_rc610_guest_tests>"
                "-DMODE=${printer_mode}"
                "-DMONITOR=${CMAKE_CURRENT_SOURCE_DIR}/src/monitor/${printer_monitor}"
                "-DDISK=${CMAKE_CURRENT_SOURCE_DIR}/src/disks/${printer_disk}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}"
                "-DSOURCE=${printer_source}"
                "-DEXAMPLE=${CMAKE_CURRENT_SOURCE_DIR}/doc/usermanual/examples/print_source.alg"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_rc610_guest.cmake)
        set_tests_properties(sim4000_rc610_${profile} PROPERTIES TIMEOUT 60)
    endforeach()
    add_test(NAME sim4000_binary_dist
        COMMAND ${CMAKE_COMMAND}
                "-DSIM4000_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DSIM4000_BINARY=$<TARGET_FILE:sim4000>"
                "-DSIM4000_TEST_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_binary_dist.cmake")
    set_tests_properties(sim4000_binary_dist PROPERTIES TIMEOUT 180)
    add_executable(sim4000_tests
        tests/c/test_main.c
        src/telnet.c
    )
    target_link_libraries(sim4000_tests PRIVATE sim4000_core)
    target_include_directories(sim4000_tests PRIVATE src)
    target_compile_definitions(sim4000_tests PRIVATE
        SIM4000_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(sim4000_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        )
    endif()
    add_test(NAME sim4000_tests COMMAND sim4000_tests)

    add_executable(sim4000_ix_tests tests/c/test_ix.c)
    target_link_libraries(sim4000_ix_tests PRIVATE sim4000_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(sim4000_ix_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME sim4000_ix_tests COMMAND sim4000_ix_tests)

    add_executable(sim4000_protection_tests tests/c/test_protection.c)
    target_link_libraries(sim4000_protection_tests PRIVATE sim4000_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(sim4000_protection_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME sim4000_protection_tests COMMAND sim4000_protection_tests)

    add_executable(sim4000_hostfiles_tests tests/c/test_hostfiles.c)
    target_link_libraries(sim4000_hostfiles_tests PRIVATE sim4000_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(sim4000_hostfiles_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME sim4000_hostfiles_tests COMMAND sim4000_hostfiles_tests)

    add_executable(sim4000_rc749_tests tests/c/test_rc749.c)
    target_link_libraries(sim4000_rc749_tests PRIVATE sim4000_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(sim4000_rc749_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME sim4000_rc749_tests COMMAND sim4000_rc749_tests)

    add_executable(sim4000_terminal_v2_tests tests/c/test_terminal_v2.c)
    target_link_libraries(sim4000_terminal_v2_tests PRIVATE sim4000_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(sim4000_terminal_v2_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME sim4000_terminal_v2_tests COMMAND sim4000_terminal_v2_tests)

    add_executable(sim4000_dfc403_tests tests/c/test_dfc403.c)
    target_link_libraries(sim4000_dfc403_tests PRIVATE sim4000_core)
    target_compile_definitions(sim4000_dfc403_tests PRIVATE
        SIM4000_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(sim4000_dfc403_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME sim4000_dfc403_tests COMMAND sim4000_dfc403_tests)

    if(Python3_Interpreter_FOUND)
        add_test(NAME sim4000_rc610_cli
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_rc610_cli.py
                $<TARGET_FILE:sim4000>)
        set_tests_properties(sim4000_rc610_cli PROPERTIES TIMEOUT 120)
        add_test(
            NAME sim4000_manual_sources
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_manual_sources.py
        )
        add_executable(sim4000_floating_probe tests/c/floating_probe.c)
        target_link_libraries(sim4000_floating_probe PRIVATE sim4000_core)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(sim4000_floating_probe PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        endif()
        add_test(
            NAME sim4000_floating_oracle
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_floating_oracle.py
                    $<TARGET_FILE:sim4000_floating_probe>
        )
        set_tests_properties(sim4000_floating_oracle PROPERTIES TIMEOUT 90)
        add_test(
            NAME sim4000_operator_cli
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_operator_cli.py
                    $<TARGET_FILE:sim4000>
        )
        set_tests_properties(sim4000_operator_cli PROPERTIES TIMEOUT 90)
        add_test(NAME sim4000_mkdisc
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_mkdisc.py
                    $<TARGET_FILE:sim4000>)
        set_tests_properties(sim4000_mkdisc PROPERTIES TIMEOUT 30)
        add_test(
            NAME sim4000_operator_acceptance
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_operator_acceptance.py
                    $<TARGET_FILE:sim4000>
        )
        set_tests_properties(sim4000_operator_acceptance PROPERTIES TIMEOUT 120)
        add_test(NAME sim4000_mips_printer
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_mips_printer.py
                --offline --queued --simulator $<TARGET_FILE:sim4000>)
        set_tests_properties(sim4000_mips_printer PROPERTIES TIMEOUT 180)
        add_test(NAME sim4000_mips_protection_guards
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_mips_protection.py)
        set_tests_properties(sim4000_mips_protection_guards PROPERTIES TIMEOUT 15)
        add_test(NAME sim4000_mips_account_guards
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_mips_accounts.py)
        set_tests_properties(sim4000_mips_account_guards PROPERTIES TIMEOUT 15)
        add_test(NAME sim4000_mips_reconnect_guards
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_mips_reconnect.py)
        set_tests_properties(sim4000_mips_reconnect_guards PROPERTIES TIMEOUT 15)
        add_test(
            NAME sim4000_runtime_directory
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_runtime_directory.py
                    $<TARGET_FILE:sim4000>
        )
        set_tests_properties(sim4000_runtime_directory PROPERTIES TIMEOUT 120)
        add_test(
            NAME sim4000_throttle_cli
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_throttle_cli.py
                    $<TARGET_FILE:sim4000>
        )
        set_tests_properties(sim4000_throttle_cli PROPERTIES TIMEOUT 90)
        add_test(
            NAME sim4000_monitor_tape_tools
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_inspect_monitor_tape.py
        )
        add_test(
            NAME sim4000_monitor_rebuild_tools
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_rebuild_monitor.py
        )
        add_test(
            NAME sim4000_runtime_libraries
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_runtime_libraries.py
                    $<TARGET_FILE:sim4000_build_host_image>
        )
        set_tests_properties(sim4000_runtime_libraries PROPERTIES TIMEOUT 120)
        add_test(
            NAME sim4000_monitor_rebuild
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_monitor_rebuild.py
                    $<TARGET_FILE:sim4000_build_host_image>
        )
        set_tests_properties(sim4000_monitor_rebuild PROPERTIES TIMEOUT 360)
        add_test(NAME sim4000_monitor_rebuild_protected
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_monitor_rebuild.py
                $<TARGET_FILE:sim4000_build_host_image> --protected
                --printer-test $<TARGET_FILE:sim4000_rc610_guest_tests>)
        set_tests_properties(sim4000_monitor_rebuild_protected PROPERTIES TIMEOUT 480)
        add_test(
            NAME sim4000_catalog_workflow
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_catalog_workflow.py
                    $<TARGET_FILE:sim4000_build_host_image> $<TARGET_FILE:sim4000>
        )
        add_test(
            NAME sim4000_catstat_diagnostic
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_catstat.py
                    $<TARGET_FILE:sim4000_build_host_image>
        )
        set_tests_properties(sim4000_catstat_diagnostic PROPERTIES TIMEOUT 45)
        add_test(
            NAME sim4000_catstat_ix
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_catstat_ix.py
                    $<TARGET_FILE:sim4000_build_host_image> $<TARGET_FILE:sim4000>
        )
        set_tests_properties(sim4000_catstat_ix PROPERTIES TIMEOUT 90)
        add_test(
            NAME sim4000_ix_runtime
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_ix_runtime.py
                    $<TARGET_FILE:sim4000>
        )
        set_tests_properties(sim4000_ix_runtime PROPERTIES TIMEOUT 30)
        add_test(
            NAME sim4000_montest_help
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_montest_help.py
                    $<TARGET_FILE:sim4000>
        )
        set_tests_properties(sim4000_montest_help PROPERTIES TIMEOUT 90)
        add_test(
            NAME sim4000_shared_terminal
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_shared_terminal.py
                    $<TARGET_FILE:sim4000>
        )
        set_tests_properties(sim4000_shared_terminal PROPERTIES TIMEOUT 180)
        add_test(
            NAME sim4000_monitor_manual
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_monitor_manual.py
                    $<TARGET_FILE:sim4000_build_host_image>
        )
        add_test(
            NAME sim4000_standard_monitor
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_standard_monitor.py
                    $<TARGET_FILE:sim4000_build_host_image> $<TARGET_FILE:sim4000>
        )
        add_test(
            NAME sim4000_runtime_devices
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_runtime_devices.py
                    $<TARGET_FILE:sim4000>
        )
        add_test(
            NAME sim4000_unit0_working_copy
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_make_unit0_copy.py
        )
        add_test(
            NAME sim4000_native_host_image
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_host_image.py
                    --builder $<TARGET_FILE:sim4000_build_host_image>
        )
    endif()
endif()
