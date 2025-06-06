# rag_core/CMakeLists.txt

# Define the target name for your new dynamic library
set(TARGET_LIB rag_core)

# Find PostgreSQL (REQUIRED as this library depends on it)
find_package(PostgreSQL REQUIRED)

if(PostgreSQL_FOUND)
    include_directories(${PostgreSQL_INCLUDE_DIR})
    message(STATUS "Found PostgreSQL for ${TARGET_LIB}: includes=${PostgreSQL_INCLUDE_DIR}, libraries=${PostgreSQL_LIBRARIES}")
    add_definitions(-DLLAMA_USE_POSTGRES)
else()
    message(FATAL_ERROR "PostgreSQL library not found for ${TARGET_LIB}. This library requires PostgreSQL.")
endif()

# Define the source files for the library
set(LIB_SRCS
    rag_database.h
    postgres_client.h
    postgres_client.cpp
    crypto_utils.h
    crypto_utils.cpp
    ecies_utils.h
    ecies_utils.cpp
)

# Add the dynamic library
add_library(${TARGET_LIB} SHARED ${LIB_SRCS})

# Set include directories for the library itself
target_include_directories(${TARGET_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} # For its own headers (e.g., "crypto_utils.h" from "postgres_client.h")
    ${CMAKE_SOURCE_DIR}/common  # To find "common.h"
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/ggml/include
    ${CMAKE_SOURCE_DIR}/tools/server
    ${CMAKE_SOURCE_DIR}/tools/mtmd
)

# Link libraries that rag_core depends on
target_link_libraries(${TARGET_LIB} PRIVATE
    ${PostgreSQL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    common
    build_info
)

# --- OpenSSL ---
find_package(OpenSSL REQUIRED)
target_link_libraries(${TARGET_LIB} PRIVATE OpenSSL::SSL OpenSSL::Crypto)

if (WIN32)
    TARGET_LINK_LIBRARIES(${TARGET_LIB} PRIVATE ws2_32)
endif()

target_compile_features(${TARGET_LIB} PRIVATE cxx_std_17)

# Install the library for external use (optional, but good practice)
install(TARGETS ${TARGET_LIB} DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/rag_database.h
              ${CMAKE_CURRENT_SOURCE_DIR}/postgres_client.h
              ${CMAKE_CURRENT_SOURCE_DIR}/crypto_utils.h
              ${CMAKE_CURRENT_SOURCE_DIR}/ecies_utils.h
        DESTINATION include)