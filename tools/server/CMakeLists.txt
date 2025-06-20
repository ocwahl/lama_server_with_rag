set(TARGET llama-server)

# option(LLAMA_SERVER_SSL "Build SSL support for the server" OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

if (MINGW)
    # fix: https://github.com/ggml-org/llama.cpp/actions/runs/9651004652/job/26617901362?pr=8006
    add_compile_definitions(_WIN32_WINNT=${GGML_WIN_VER})
endif()

set(TARGET_SRCS
    server.cpp
    utils.hpp
    httplib.h
)
set(PUBLIC_ASSETS
    index.html.gz
    loading.html
)

foreach(asset ${PUBLIC_ASSETS})
    set(input "${CMAKE_CURRENT_SOURCE_DIR}/public/${asset}")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${asset}.hpp")
    list(APPEND TARGET_SRCS ${output})
    add_custom_command(
        DEPENDS "${input}"
        OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${input}" "-DOUTPUT=${output}" -P "${PROJECT_SOURCE_DIR}/scripts/xxd.cmake"
    )
    set_source_files_properties(${output} PROPERTIES GENERATED TRUE)
endforeach()

add_executable(${TARGET} ${TARGET_SRCS})
install(TARGETS ${TARGET} RUNTIME)

target_include_directories(${TARGET} PRIVATE ../llava)
target_include_directories(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR})

# Link to the rag_core library
target_include_directories(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/rag_core ${CMAKE_SOURCE_DIR}/tools/self_signed_cert)

# Link the server executable to your new dynamic library
target_link_libraries(${TARGET} PRIVATE common mtmd ${CMAKE_THREAD_LIBS_INIT} rag_core self_signed_cert) # Link rag_core here

if (LLAMA_SERVER_SSL)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${TARGET} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(${TARGET} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
endif()

# --- SGX Integration Starts Here ---
# You need to define the path to your Intel SGX SDK installation.
# The default path on Linux is often /opt/intel/sgxsdk
# You might need to adjust this based on your actual installation location.
set(SGX_SDK_PATH "/opt/intel/sgxsdk" CACHE PATH "Path to Intel SGX SDK installation")
set(TDX_ATTEST_PATH "/usr/lib/x86_64-linux-gnu" CACHE PATH "Path to Intel TDX attest installation")

if (EXISTS "${SGX_SDK_PATH}/include/sgx_urts.h") # Check if SDK path seems valid
    message(STATUS "Intel SGX SDK found at: ${SGX_SDK_PATH}")

    # Add ALL relevant SGX SDK library directories to the linker search path
    # THIS IS THE KEY CHANGE
    target_link_directories(${TARGET} PRIVATE
        ${SGX_SDK_PATH}/lib
        ${SGX_SDK_PATH}/lib64 # Common on some Linux systems
        ${SGX_SDK_PATH}/sdk_libs # <-- ADD THIS LINE for sdk_libs
        ${TDX_ATTEST_PATH} #is this useful
    )

    # Link SGX SDK libraries
    # You'll need to specify the correct library names.
    # Common ones for non-enclave code are sgx_urts and sgx_epid.
    # For TDLS (Trusted TLS), you might need sgx_tnet, sgx_ttls, etc.,
    # but the exact libraries depend on how sgx_ttls.h is used and what
    # SGX features it relies on.
    # Note: For simulation mode, these are usually provided by the SDK.
    target_link_libraries(${TARGET} PRIVATE
        # sgx_urts_sim # User Runtime Services
        # sgx_trts_sim
        # sgx_epid_sim # EPID library (if used for remote attestation)
        # sgx_uae_service_sim
        # sgx_tservice_sim
        # sgx_ttls
        tdx_tls
        tdx_attest
        # sgx_ttls
        # sgx_utls
        # Add other SGX libraries required by sgx_ttls.h if compiler complains about undefined references.
        # e.g., sgx_tnet for network functions, sgx_ttls if it's a separate lib
    )

    # You might also need specific compile definitions for SGX,
    # depending on how sgx_ttls.h is designed.
    # For simulation mode, often specific flags are needed.
    # add_compile_definitions(${TARGET} PRIVATE SGX_MODE_SIM) # Example for simulation mode
    # add_compile_definitions(${TARGET} PRIVATE _LIBCPP_SGX_NO_IO_STRING) # Example for libc++ on SGX

else()
    message(WARNING "Intel SGX SDK not found at ${SGX_SDK_PATH}. SGX features will not compile.")
    # You might want to wrap your SGX-dependent code with #ifdef checks
    # or make the SGX integration optional with a CMake option.
endif()
# --- SGX Integration Ends Here ---

if (WIN32)
    TARGET_LINK_LIBRARIES(${TARGET} PRIVATE ws2_32)
endif()

target_compile_features(${TARGET} PRIVATE cxx_std_17)
