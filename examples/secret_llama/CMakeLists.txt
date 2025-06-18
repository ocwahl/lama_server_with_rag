set(TARGET secret-llama)

set(TARGET_SRCS
    secret-llama.cpp
    secret_llama.h
    llm_engine.h
    main.cpp
)
add_executable(${TARGET} ${TARGET_SRCS})
install(TARGETS ${TARGET} RUNTIME)
target_include_directories(${TARGET} PUBLIC . ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(${TARGET} PRIVATE common llama ${CMAKE_THREAD_LIBS_INIT})
target_compile_features(${TARGET} PRIVATE cxx_std_17)