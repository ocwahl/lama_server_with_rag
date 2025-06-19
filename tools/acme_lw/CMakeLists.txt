# Define the target name for your new dynamic library
set(TARGET_LIB acme_lw)

# Define the source files for the library
set(LIB_SRCS
    acme-exception.h
    acme-lw.h
    http.h
    acme-exception.cpp
    acme-lw.cpp
    http.cpp
)

# Add the dynamic library
add_library(${TARGET_LIB} SHARED ${LIB_SRCS})


find_package (CURL REQUIRED)
# --- OpenSSL ---
find_package(OpenSSL REQUIRED)
# target_link_libraries(${TARGET_LIB} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(${TARGET_LIB} PRIVATE ${CURL_LIBRARIES} ${OPENSSL_LIBRARIES})

if (WIN32)
    TARGET_LINK_LIBRARIES(${TARGET_LIB} PRIVATE ws2_32)
endif()


install (FILES acme-exception.h acme-lw.h DESTINATION include)
# Install the library for external use (optional, but good practice)
install(TARGETS ${TARGET_LIB} DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/acme-exception.h
              ${CMAKE_CURRENT_SOURCE_DIR}/acme-lw.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../internal
    ${CURL_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    # ${CMAKE_SOURCE_DIR}/ggml/include
    # ${CMAKE_SOURCE_DIR}/tools/server
    # ${CMAKE_SOURCE_DIR}/tools/mtmd
)


# rag_core/CMakeLists.txt
# Link libraries that rag_core depends on
#target_link_libraries(${TARGET_LIB} PRIVATE
#    ${PostgreSQL_LIBRARIES}
#    ${CMAKE_THREAD_LIBS_INIT}
#)



target_compile_features(${TARGET_LIB} PRIVATE cxx_std_17)

