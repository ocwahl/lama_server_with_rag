# Define the target name for your new dynamic library
set(TARGET_LIB self_signed_cert)

# Define the source files for the library
set(LIB_SRCS
    self_signed.h
    self_signed.cpp
    # main.cpp
)

# Add the dynamic library
add_library(${TARGET_LIB} SHARED ${LIB_SRCS})
# add_executable(${TARGET_LIB} ${LIB_SRCS})


# --- OpenSSL ---
find_package(OpenSSL REQUIRED)
target_link_libraries(${TARGET_LIB} PRIVATE ${OPENSSL_LIBRARIES})

if (WIN32)
    TARGET_LINK_LIBRARIES(${TARGET_LIB} PRIVATE ws2_32)
endif()


# Install the library for external use (optional, but good practice)
install(TARGETS ${TARGET_LIB} DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/self_signed.h
        DESTINATION include)


include(CheckFunctionExists)
set(CMAKE_REQUIRED_LIBRARIES OpenSSL::Crypto)
check_function_exists(ASN1_TIME_to_tm HAS_OPENSSL_TO_TM)
check_function_exists(timegm HAS_TIMEGM)
if (OPENSSL_TO_TM AND HAS_TIMEGM)
add_definitions(-DOPENSSL_TO_TM)
endif ()

#target_include_directories(${TARGET_LIB} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../internal ${CURL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
target_include_directories(${TARGET_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} # For its own headers (e.g., "crypto_utils.h" from "postgres_client.h")
    # ${CMAKE_CURRENT_SOURCE_DIR}/../internal
    ${CMAKE_SOURCE_DIR}/common  # To find "common.h"
    ${CMAKE_SOURCE_DIR}/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/../internal
    # ${CURL_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    # ${CMAKE_SOURCE_DIR}/ggml/include
    # ${CMAKE_SOURCE_DIR}/tools/server
    # ${CMAKE_SOURCE_DIR}/tools/mtmd
)

# --- SGX Integration Starts Here ---
# You need to define the path to your Intel SGX SDK installation.
# The default path on Linux is often /opt/intel/sgxsdk
# You might need to adjust this based on your actual installation location.
set(SGX_SDK_PATH "/opt/intel/sgxsdk" CACHE PATH "Path to Intel SGX SDK installation")
set(TDX_ATTEST_PATH "/usr/lib/x86_64-linux-gnu" CACHE PATH "Path to Intel TDX attest installation")

if (EXISTS "${SGX_SDK_PATH}/include/sgx_urts.h") # Check if SDK path seems valid
    message(STATUS "Intel SGX SDK found at: ${SGX_SDK_PATH}")

    # Add SGX SDK include directories
    target_include_directories(${TARGET_LIB} PRIVATE
        ${SGX_SDK_PATH}/include
        # ${SGX_SDK_PATH}/include/tlibc #OWL WAS HERE
        # ${SGX_SDK_PATH}/include/libcxx #OWL WAS HERE
    )
else()
    message(WARNING "Intel SGX SDK not found at ${SGX_SDK_PATH}. SGX features will not compile.")
endif()
# --- SGX Integration Ends Here ---


target_compile_features(${TARGET_LIB} PRIVATE cxx_std_17)

