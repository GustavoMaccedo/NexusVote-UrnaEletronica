# Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
cmake_minimum_required(VERSION 3.28)
project(nexusvote VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_definitions(NEXUSVOTE_VERSION="1.2.0-lab")

option(LAB_TEST_MODE "Build de laboratorio: habilita comandos de teste" ON)
option(WITH_QT "Interface Qt Widgets (requer Qt6 instalado)" OFF)
option(BUILD_TESTING "Compila suites de teste" ON)
option(WITH_RUST_SECURE "Liga canal seguro Rust (journal selado 1.3.0-lab)" ON)

if(LAB_TEST_MODE)
  add_compile_definitions(NEXUSVOTE_LAB_TEST_MODE=1)
endif()

find_package(OpenSSL REQUIRED)

if(WITH_RUST_SECURE)
  find_program(CARGO_BIN cargo)
  if(CARGO_BIN)
    add_compile_definitions(NEXUSVOTE_WITH_RUST_SECURE=1)
    if(NOT DEFINED ENV{CARGO_TARGET_DIR})
      set(RUST_TARGET_DIR "/tmp/nvbuild/rust-target")
    else()
      set(RUST_TARGET_DIR "$ENV{CARGO_TARGET_DIR}")
    endif()
    add_custom_command(
      OUTPUT "${RUST_TARGET_DIR}/debug/libnv_secure_ffi.a"
      COMMAND ${CMAKE_COMMAND} -E env "CARGO_TARGET_DIR=${RUST_TARGET_DIR}"
              ${CARGO_BIN} build -p nv-secure-ffi --manifest-path ${CMAKE_SOURCE_DIR}/rust/Cargo.toml
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/rust
      COMMENT "Compilando canal seguro Rust (nv-secure-ffi)"
      VERBATIM
    )
    add_custom_target(nv_secure_ffi_lib DEPENDS "${RUST_TARGET_DIR}/debug/libnv_secure_ffi.a")
    add_library(nvsecure STATIC IMPORTED GLOBAL)
    set_target_properties(nvsecure PROPERTIES IMPORTED_LOCATION "${RUST_TARGET_DIR}/debug/libnv_secure_ffi.a")
    add_dependencies(nvsecure nv_secure_ffi_lib)
  else()
    message(WARNING "WITH_RUST_SECURE=ON mas cargo não encontrado; seguindo sem canal Rust")
    set(WITH_RUST_SECURE OFF)
  endif()
endif()

add_library(nvcore STATIC
  src/core/minijson.cpp
  src/core/rules.cpp
  src/core/config_loader.cpp
  src/core/state_machine.cpp
  src/core/vote_store.cpp
  src/core/tally.cpp
  src/core/crypto_adapter.cpp
  src/core/clock.cpp
  src/core/roles.cpp
  src/core/session.cpp
  src/core/audit_export.cpp
  src/secure/secure_session.cpp
)
target_include_directories(nvcore PUBLIC src)
target_link_libraries(nvcore PUBLIC OpenSSL::Crypto)
if(WITH_RUST_SECURE AND TARGET nvsecure)
  target_link_libraries(nvcore PUBLIC nvsecure dl m pthread)
endif()
target_compile_options(nvcore PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)

add_executable(nexusvote-cli src/cli/main.cpp)
target_link_libraries(nexusvote-cli PRIVATE nvcore nvlab)

add_library(nvlab STATIC
  src/lab/nvlp.cpp
  src/lab/lab_daemon.cpp
  src/lab/mesario_daemon.cpp
  src/lab/keypad_evdev.cpp
)
target_include_directories(nvlab PUBLIC src)
target_link_libraries(nvlab PUBLIC nvcore)
target_compile_options(nvlab PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)

add_library(nvpaper STATIC
  src/paper/paper_escpos.cpp
)
target_include_directories(nvpaper PUBLIC src)
target_link_libraries(nvpaper PUBLIC nvcore)
target_compile_options(nvpaper PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)

add_executable(urna-pi src/terminal/urna_pi.cpp)
target_link_libraries(urna-pi PRIVATE nvcore nvlab nvpaper)

add_custom_target(build_info ALL
  COMMAND ${CMAKE_COMMAND}
    -DOUT=${CMAKE_BINARY_DIR}/build-info.json
    -DBIN_CLI=$<TARGET_FILE:nexusvote-cli>
    -DVERSION=1.2.0-lab
    -DCOMPILER=${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}
    -DBUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DLAB_TEST_MODE=${LAB_TEST_MODE}
    -DWITH_QT=${WITH_QT}
    -P ${CMAKE_SOURCE_DIR}/cmake/WriteBuildInfo.cmake
  DEPENDS nexusvote-cli
  COMMENT "Gerando build-info.json"
)

if(WITH_QT)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
  set(CMAKE_AUTOMOC ON)
  find_package(Qt6 REQUIRED COMPONENTS Widgets)
  add_executable(nexusvote-qt
    src/ui/qt/qt_voting_window.cpp
    src/ui/qt/qt_controller.cpp
    src/ui/qt/main_qt.cpp
  )
  target_link_libraries(nexusvote-qt PRIVATE nvcore Qt6::Widgets)
  target_compile_options(nexusvote-qt PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  enable_testing()
  add_executable(nvtests
    tests/main.cpp
    tests/test_config.cpp
    tests/test_rules.cpp
    tests/test_session.cpp
    tests/test_tally_store.cpp
    tests/test_crypto.cpp
    tests/test_nvlp.cpp
    tests/test_secure.cpp
    tests/test_clock.cpp
    tests/test_roles.cpp
    tests/test_hw_real.cpp
  )
  target_link_libraries(nvtests PRIVATE nvcore nvlab nvpaper)
  target_compile_options(nvtests PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_definitions(nvtests PRIVATE
    CONFIG_DIR="${CMAKE_SOURCE_DIR}/config"
    TMP_BASE="${CMAKE_BINARY_DIR}/test-tmp")
  if(WITH_QT)
    target_sources(nvtests PRIVATE tests/test_qt_window.cpp src/ui/qt/qt_voting_window.cpp
                                   src/ui/qt/qt_controller.cpp)
    target_link_libraries(nvtests PRIVATE Qt6::Widgets)
  endif()
  add_test(NAME nvtests COMMAND nvtests)
  if(WITH_QT)
    set_tests_properties(nvtests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LC_ALL=C.UTF-8")
  endif()
endif()
