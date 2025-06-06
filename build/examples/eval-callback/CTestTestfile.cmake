# CMake generated Testfile for 
# Source directory: /Users/damiantziamtzis/Developer/sandbox/lama_server_with_rag/examples/eval-callback
# Build directory: /Users/damiantziamtzis/Developer/sandbox/lama_server_with_rag/build/examples/eval-callback
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test-eval-callback "/Users/damiantziamtzis/Developer/sandbox/lama_server_with_rag/build/bin/llama-eval-callback" "--hf-repo" "ggml-org/models" "--hf-file" "tinyllamas/stories260K.gguf" "--model" "stories260K.gguf" "--prompt" "hello" "--seed" "42" "-ngl" "0")
set_tests_properties(test-eval-callback PROPERTIES  LABELS "eval-callback;curl" _BACKTRACE_TRIPLES "/Users/damiantziamtzis/Developer/sandbox/lama_server_with_rag/examples/eval-callback/CMakeLists.txt;8;add_test;/Users/damiantziamtzis/Developer/sandbox/lama_server_with_rag/examples/eval-callback/CMakeLists.txt;0;")
