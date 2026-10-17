cmake_minimum_required(VERSION 3.25)
project(wots4_mod_better_save_name LANGUAGES C CXX)

# When building standalone, pass -DWOTS4_SDK_DIR=<path to wots4-mods>
if(NOT COMMAND wots4_add_mod)
    set(WOTS4_SDK_DIR "" CACHE PATH "Path to wots4-mods (ModLoader)")
    if(NOT EXISTS "${WOTS4_SDK_DIR}/cmake/Wots4.cmake")
        message(FATAL_ERROR "Set WOTS4_SDK_DIR to the path of wots4-mods")
    endif()
    include(${WOTS4_SDK_DIR}/cmake/Wots4.cmake)
    enable_testing()
    set(WOTS4_BUILD_TESTS ON)
endif()

set(BETTER_SAVE_NAME_LABEL_SOURCES src/event_data.cpp src/save_summary.cpp src/label_fit.cpp src/row_label_cache.cpp)

# Use the Mod's folder name in the game folder (mods/<name>/) as <name>
wots4_add_mod(better_save_name
    SOURCES src/mod.cpp src/game_hooks.cpp ${BETTER_SAVE_NAME_LABEL_SOURCES}
    DEF src/mod.def
    FILES README.md README.en.md)
target_include_directories(mod_better_save_name PRIVATE src)
target_compile_definitions(mod_better_save_name PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(WOTS4_BUILD_TESTS)
    wots4_add_probe(better_save_name_offline_tests
        SOURCES tests/offline_tests.cpp ${BETTER_SAVE_NAME_LABEL_SOURCES}
        INCLUDES src)
    set(BETTER_SAVE_NAME_TEST_EXE "${WOTS4_ROOT}/research/bin/WayOfTheSamurai4.unpacked.exe"
        CACHE FILEPATH "Decrypted game copy for offline hook tests")
    wots4_add_probe(better_save_name_exe_tests SOURCES tests/exe_tests.cpp INCLUDES src
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS "${BETTER_SAVE_NAME_TEST_EXE}" $<TARGET_FILE:mod_better_save_name> NEEDS_EXE)
    add_dependencies(better_save_name_exe_tests mod_better_save_name)
    # A refused initialization runs in its own process: an accepted one keeps its redirected call.
    add_test(NAME better_save_name_exe_conflict COMMAND better_save_name_exe_tests
        "${BETTER_SAVE_NAME_TEST_EXE}" $<TARGET_FILE:mod_better_save_name> conflict)
    set_tests_properties(better_save_name_exe_conflict PROPERTIES LABELS needs_exe)
endif()
