# tests/rag_core_test/CMakeLists.txt

# Define the target name for your test executable
set(TARGET_TEST rag_core_test)

# Define the source files for the test executable
set(TEST_SRCS
    test_rag_core.cpp
)

# Add the executable
add_executable(${TARGET_TEST} ${TEST_SRCS})

# Include directories needed by the test executable
# It needs to find headers from rag_core, common, and the main llama.cpp root for llama-cpp.h
target_include_directories(${TARGET_TEST} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}        # For its own headers (if any)
    ${CMAKE_SOURCE_DIR}/tools/rag_core   # To find rag_core headers (crypto_utils.h, ecies_utils.h etc.)
    ${CMAKE_SOURCE_DIR}/common         # To find common.h
    ${CMAKE_SOURCE_DIR}                # To find llama-cpp.h (which common.h includes)
    ${CMAKE_SOURCE_DIR}/ggml/include   # If any common/rag_core headers indirectly include ggml headers
    ${CMAKE_SOURCE_DIR}/tools/server
    ${CMAKE_SOURCE_DIR}/tools/mtmd
)


# Link the test executable to the rag_core library
# It also needs common and mtmd if rag_core or its dependencies pull them in.
target_link_libraries(${TARGET_TEST} PRIVATE
    rag_core        # Link against your new library
    common          # If common utilities are directly used or linked by rag_core transitively
    mtmd            # If mtmd is used by common/rag_core
    ${CMAKE_THREAD_LIBS_INIT} # For threading utilities
)

# Make sure OpenSSL is linked, as rag_core depends on it
find_package(OpenSSL REQUIRED)
target_link_libraries(${TARGET_TEST} PRIVATE OpenSSL::SSL OpenSSL::Crypto)

if (WIN32)
    TARGET_LINK_LIBRARIES(${TARGET_TEST} PRIVATE ws2_32)
endif()

target_compile_features(${TARGET_TEST} PRIVATE cxx_std_17)

# Add the test to CTest
add_test(NAME ${TARGET_TEST} COMMAND ${TARGET_TEST})