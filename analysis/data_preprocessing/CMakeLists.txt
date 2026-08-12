cmake_minimum_required(VERSION 3.10)

project(
  cshine_gamma_data_preprocessing
  VERSION 0.15.0
  DESCRIPTION "Portable CSHINE-Gamma calibration and detector-signal preprocessing"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(
  ROOT 6.22 REQUIRED
  COMPONENTS Core Gpad Graf Hist Imt RIO Thread Tree TreePlayer
)
include(GNUInstallDirs)

set(CALIBRATION_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/t_2d_fit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/t_gamma_cali.h)

set(CALIBRATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/t_2d_fit.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/src/t_gamma_cali.cxx)

ROOT_GENERATE_DICTIONARY(
  G__cshine_gamma_calibration
  ${CALIBRATION_HEADERS}
  LINKDEF ${CMAKE_CURRENT_SOURCE_DIR}/include/LinkDef.h
  OPTIONS -I${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(
  cshine_gamma_calibration
  SHARED
  ${CALIBRATION_SOURCES}
  ${CMAKE_CURRENT_BINARY_DIR}/G__cshine_gamma_calibration.cxx
)

target_include_directories(
  cshine_gamma_calibration
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_calibration
  PUBLIC
    ROOT::Core
    ROOT::Graf
    ROOT::Hist
    ROOT::RIO
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_calibration
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

set_target_properties(
  cshine_gamma_calibration
  PROPERTIES
    OUTPUT_NAME cshine_gamma_calibration
)

add_library(
  cshine_gamma_source_background
  STATIC
  src/source_background_spectra.cxx
)

target_include_directories(
  cshine_gamma_source_background
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_source_background
  PUBLIC
    ROOT::Core
    ROOT::Hist
    ROOT::RIO
    ROOT::Tree
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_source_background
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

add_executable(
  build_source_spectra
  apps/build_source_spectra.cxx
)

target_link_libraries(
  build_source_spectra
  PRIVATE
    cshine_gamma_source_background
)

add_library(
  cshine_gamma_gain_relation
  STATIC
  src/gain_relation.cxx
)

target_include_directories(
  cshine_gamma_gain_relation
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_gain_relation
  PUBLIC
    cshine_gamma_calibration
    ROOT::Core
    ROOT::Gpad
    ROOT::Graf
    ROOT::Hist
    ROOT::RIO
    ROOT::Tree
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_gain_relation
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

add_executable(
  fit_gain_relation
  apps/fit_gain_relation.cxx
)

target_link_libraries(
  fit_gain_relation
  PRIVATE
    cshine_gamma_gain_relation
)

add_library(
  cshine_gamma_energy_calibration
  STATIC
  src/energy_calibration.cxx
)

target_include_directories(
  cshine_gamma_energy_calibration
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_energy_calibration
  PUBLIC
    cshine_gamma_calibration
    ROOT::Core
    ROOT::Gpad
    ROOT::Graf
    ROOT::Hist
    ROOT::RIO
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_energy_calibration
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

add_executable(
  fit_energy_calibration
  apps/fit_energy_calibration.cxx
)

target_link_libraries(
  fit_energy_calibration
  PRIVATE
    cshine_gamma_energy_calibration
)

add_library(
  cshine_gamma_time_calibration
  STATIC
  src/time_calibration.cxx
)

target_include_directories(
  cshine_gamma_time_calibration
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_time_calibration
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

add_library(
  cshine_gamma_time_fit_outputs
  STATIC
  src/time_fit_outputs.cxx
)

target_include_directories(
  cshine_gamma_time_fit_outputs
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_time_fit_outputs
  PUBLIC
    cshine_gamma_time_calibration
    ROOT::Core
    ROOT::Gpad
    ROOT::Hist
    ROOT::RIO
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_time_fit_outputs
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

add_executable(
  inspect_time_fit_outputs
  apps/inspect_time_fit_outputs.cxx
)

target_link_libraries(
  inspect_time_fit_outputs
  PRIVATE
    cshine_gamma_time_fit_outputs
)

add_library(
  cshine_gamma_time_amplitude_spectra
  STATIC
  src/time_amplitude_spectra.cxx
)

target_include_directories(
  cshine_gamma_time_amplitude_spectra
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_time_amplitude_spectra
  PUBLIC
    cshine_gamma_time_calibration
    ROOT::Core
    ROOT::Hist
    ROOT::Imt
    ROOT::RIO
    ROOT::Thread
    ROOT::Tree
    ROOT::TreePlayer
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_time_amplitude_spectra
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

add_executable(
  build_time_amplitude_spectra
  apps/build_time_amplitude_spectra.cxx
)

add_library(
  cshine_gamma_calibrated_event_tree
  STATIC
  src/calibrated_event_tree.cxx
)

set(RECONSTRUCTION_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/jiugong_recon.h)

ROOT_GENERATE_DICTIONARY(
  G__cshine_gamma_reconstruction
  ${RECONSTRUCTION_HEADERS}
  LINKDEF ${CMAKE_CURRENT_SOURCE_DIR}/include/ReconstructionLinkDef.h
  OPTIONS -I${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(
  cshine_gamma_reconstruction
  SHARED
  src/jiugong_recon.cxx
  ${CMAKE_CURRENT_BINARY_DIR}/G__cshine_gamma_reconstruction.cxx
)

target_include_directories(
  cshine_gamma_reconstruction
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_reconstruction
  PUBLIC
    ROOT::Core
    ROOT::RIO
)

add_library(
  cshine_gamma_shower_reconstruction
  STATIC
  src/shower_reconstruction.cxx
)

target_include_directories(
  cshine_gamma_shower_reconstruction
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_shower_reconstruction
  PUBLIC
    cshine_gamma_reconstruction
)

add_library(
  cshine_gamma_reconstructed_event_tree
  STATIC
  src/reconstructed_event_tree.cxx
)

target_include_directories(
  cshine_gamma_reconstructed_event_tree
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_reconstructed_event_tree
  PUBLIC
    cshine_gamma_shower_reconstruction
    ROOT::Core
    ROOT::Hist
    ROOT::RIO
    ROOT::Tree
    ROOT::TreePlayer
)

add_executable(
  build_reconstructed_event_tree
  apps/build_reconstructed_event_tree.cxx
)

add_library(
  cshine_gamma_reconstructed_spectrum_merge
  STATIC
  src/reconstructed_spectrum_merge.cxx
)

target_include_directories(
  cshine_gamma_reconstructed_spectrum_merge
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_reconstructed_spectrum_merge
  PUBLIC
    ROOT::Core
    ROOT::Gpad
    ROOT::Graf
    ROOT::Hist
    ROOT::RIO
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_reconstructed_spectrum_merge
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

add_executable(
  merge_reconstructed_spectra
  apps/merge_reconstructed_spectra.cxx
)

target_link_libraries(
  merge_reconstructed_spectra
  PRIVATE
    cshine_gamma_reconstructed_spectrum_merge
)

add_library(
  cshine_gamma_chapter3_diagnostics
  STATIC
  src/gamma_position.cxx
  src/spatial_spread.cxx
  src/chapter3_diagnostics.cxx
)

target_include_directories(
  cshine_gamma_chapter3_diagnostics
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_chapter3_diagnostics
  PUBLIC
    cshine_gamma_shower_reconstruction
    ROOT::Core
    ROOT::Hist
    ROOT::RIO
    ROOT::Tree
    ROOT::TreePlayer
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_chapter3_diagnostics
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

add_executable(
  build_chapter3_diagnostics
  apps/build_chapter3_diagnostics.cxx
)

add_library(
  cshine_gamma_trigger_diagnostics
  STATIC
  src/trigger_diagnostics.cxx
)

target_include_directories(
  cshine_gamma_trigger_diagnostics
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_trigger_diagnostics
  PUBLIC
    cshine_gamma_shower_reconstruction
    ROOT::Core
    ROOT::Hist
    ROOT::RIO
    ROOT::Tree
    ROOT::TreePlayer
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_trigger_diagnostics
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

add_executable(
  build_trigger_diagnostics
  apps/build_trigger_diagnostics.cxx
)

target_link_libraries(
  build_trigger_diagnostics
  PRIVATE
    cshine_gamma_trigger_diagnostics
)

add_library(cshine_gamma_fast_coincidence_spectra STATIC
  src/fast_coincidence_spectra.cxx)
target_include_directories(cshine_gamma_fast_coincidence_spectra PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(cshine_gamma_fast_coincidence_spectra PUBLIC
  cshine_gamma_shower_reconstruction ROOT::Core ROOT::Hist ROOT::RIO
  ROOT::Tree ROOT::TreePlayer)
add_executable(build_fast_coincidence_spectra
  apps/build_fast_coincidence_spectra.cxx)
target_link_libraries(build_fast_coincidence_spectra PRIVATE
  cshine_gamma_fast_coincidence_spectra)

add_library(cshine_gamma_observed_spectrum STATIC src/observed_spectrum.cxx)
target_include_directories(cshine_gamma_observed_spectrum PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(cshine_gamma_observed_spectrum PUBLIC
  ROOT::Core ROOT::Hist ROOT::RIO)
add_executable(build_observed_spectrum apps/build_observed_spectrum.cxx)
target_link_libraries(build_observed_spectrum PRIVATE
  cshine_gamma_observed_spectrum)
add_executable(inspect_observed_spectrum apps/inspect_observed_spectrum.cxx)
target_link_libraries(inspect_observed_spectrum PRIVATE
  cshine_gamma_observed_spectrum)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  foreach(m11_target cshine_gamma_fast_coincidence_spectra
                     cshine_gamma_observed_spectrum)
    target_compile_options(${m11_target} PRIVATE -Wall -Wextra -Wpedantic)
  endforeach()
endif()

target_link_libraries(
  build_chapter3_diagnostics
  PRIVATE
    cshine_gamma_chapter3_diagnostics
)

target_link_libraries(
  build_reconstructed_event_tree
  PRIVATE
    cshine_gamma_reconstructed_event_tree
)

target_include_directories(
  cshine_gamma_calibrated_event_tree
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_calibrated_event_tree
  PUBLIC
    cshine_gamma_calibration
    cshine_gamma_time_calibration
    ROOT::Core
    ROOT::RIO
    ROOT::Tree
    ROOT::TreePlayer
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_calibrated_event_tree
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  foreach(reconstruction_target
          cshine_gamma_reconstruction
          cshine_gamma_shower_reconstruction
          cshine_gamma_reconstructed_event_tree)
    target_compile_options(
      ${reconstruction_target}
      PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
  endforeach()
endif()

add_executable(
  build_calibrated_event_tree
  apps/build_calibrated_event_tree.cxx
)

target_link_libraries(
  build_calibrated_event_tree
  PRIVATE
    cshine_gamma_calibrated_event_tree
)

add_library(
  cshine_gamma_neighbor_time_diagnostics
  STATIC
  src/neighbor_time_diagnostics.cxx
)

target_include_directories(
  cshine_gamma_neighbor_time_diagnostics
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  cshine_gamma_neighbor_time_diagnostics
  PUBLIC
    ROOT::Core
    ROOT::Hist
    ROOT::RIO
    ROOT::Tree
    ROOT::TreePlayer
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    cshine_gamma_neighbor_time_diagnostics
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

add_executable(
  build_neighbor_time_diagnostics
  apps/build_neighbor_time_diagnostics.cxx
)

target_link_libraries(
  build_neighbor_time_diagnostics
  PRIVATE
    cshine_gamma_neighbor_time_diagnostics
)

target_link_libraries(
  build_time_amplitude_spectra
  PRIVATE
    cshine_gamma_time_amplitude_spectra
)

if(APPLE)
  set_target_properties(
    fit_gain_relation fit_energy_calibration build_calibrated_event_tree
    build_neighbor_time_diagnostics build_reconstructed_event_tree
    merge_reconstructed_spectra build_chapter3_diagnostics
    build_trigger_diagnostics build_fast_coincidence_spectra
    build_observed_spectrum inspect_observed_spectrum
    PROPERTIES INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}"
  )
elseif(UNIX)
  set_target_properties(
    fit_gain_relation fit_energy_calibration build_calibrated_event_tree
    build_reconstructed_event_tree build_chapter3_diagnostics
    build_trigger_diagnostics build_fast_coincidence_spectra
    build_observed_spectrum inspect_observed_spectrum
    PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
  )
endif()

include(CTest)

if(BUILD_TESTING)
  add_executable(
    test_calibration_logic
    tests/test_calibration_logic.cxx
  )

  target_link_libraries(
    test_calibration_logic
    PRIVATE
      cshine_gamma_calibration
      ROOT::Core
  )

  add_test(
    NAME calibration_logic
    COMMAND test_calibration_logic
  )

  add_executable(
    test_calibration_dictionary
    tests/test_calibration_dictionary.cxx
  )

  target_link_libraries(
    test_calibration_dictionary
    PRIVATE
      cshine_gamma_calibration
      ROOT::Core
      ROOT::RIO
  )

  add_test(
    NAME calibration_dictionary
    COMMAND test_calibration_dictionary
  )

  set_tests_properties(
    calibration_dictionary
    PROPERTIES LABELS "root-io"
  )

  add_executable(
    inspect_calibration_object
    tests/inspect_calibration_object.cxx
  )

  target_link_libraries(
    inspect_calibration_object
    PRIVATE
      cshine_gamma_calibration
      ROOT::Core
      ROOT::RIO
  )

  add_executable(
    test_source_background_spectra
    tests/test_source_background_spectra.cxx
  )

  target_link_libraries(
    test_source_background_spectra
    PRIVATE
      cshine_gamma_source_background
      ROOT::Core
      ROOT::Hist
      ROOT::RIO
      ROOT::Tree
  )

  add_test(
    NAME source_background_spectra
    COMMAND test_source_background_spectra
  )

  add_executable(
    test_gain_relation
    tests/test_gain_relation.cxx
  )

  target_link_libraries(
    test_gain_relation
    PRIVATE
      cshine_gamma_gain_relation
  )

  add_test(
    NAME gain_relation
    COMMAND test_gain_relation
  )

  set_tests_properties(
    gain_relation
    PROPERTIES LABELS "root-fit;root-io"
  )

  add_executable(
    test_energy_calibration
    tests/test_energy_calibration.cxx
  )

  target_link_libraries(
    test_energy_calibration
    PRIVATE
      cshine_gamma_energy_calibration
  )

  add_test(
    NAME energy_calibration
    COMMAND test_energy_calibration
  )

  set_tests_properties(
    energy_calibration
    PROPERTIES LABELS "root-fit;root-io"
  )

  add_executable(
    test_time_calibration
    tests/test_time_calibration.cxx
  )

  target_link_libraries(
    test_time_calibration
    PRIVATE
      cshine_gamma_time_calibration
  )

  add_test(
    NAME time_calibration
    COMMAND test_time_calibration
  )

  add_executable(
    test_time_fit_outputs
    tests/test_time_fit_outputs.cxx
  )

  target_link_libraries(
    test_time_fit_outputs
    PRIVATE
      cshine_gamma_time_fit_outputs
  )

  add_test(
    NAME time_fit_outputs
    COMMAND test_time_fit_outputs
  )

  set_tests_properties(
    time_fit_outputs
    PROPERTIES LABELS "root-io;historical-artifact-audit"
  )

  add_executable(
    test_time_amplitude_spectra
    tests/test_time_amplitude_spectra.cxx
  )

  target_link_libraries(
    test_time_amplitude_spectra
    PRIVATE
      cshine_gamma_time_amplitude_spectra
  )

  add_test(
    NAME time_amplitude_spectra
    COMMAND test_time_amplitude_spectra
  )

  set_tests_properties(
    time_amplitude_spectra
    PROPERTIES LABELS "root-io;multithreaded-histogram-production"
  )

  add_executable(
    test_calibrated_event_tree
    tests/test_calibrated_event_tree.cxx
  )

  target_link_libraries(
    test_calibrated_event_tree
    PRIVATE
      cshine_gamma_calibrated_event_tree
  )

  add_test(
    NAME calibrated_event_tree
    COMMAND test_calibrated_event_tree
  )

  set_tests_properties(
    calibrated_event_tree
    PROPERTIES LABELS "root-io;event-level-calibration"
  )

  add_executable(
    test_neighbor_time_diagnostics
    tests/test_neighbor_time_diagnostics.cxx
  )

  target_link_libraries(
    test_neighbor_time_diagnostics
    PRIVATE
      cshine_gamma_neighbor_time_diagnostics
  )

  add_test(
    NAME neighbor_time_diagnostics
    COMMAND test_neighbor_time_diagnostics
  )

  set_tests_properties(
    neighbor_time_diagnostics
    PROPERTIES LABELS "root-io;neighbor-time-diagnostics"
  )

  add_executable(
    test_shower_reconstruction
    tests/test_shower_reconstruction.cxx
  )

  target_link_libraries(
    test_shower_reconstruction
    PRIVATE
      cshine_gamma_shower_reconstruction
      ROOT::Core
      ROOT::RIO
      ROOT::Tree
      ROOT::TreePlayer
  )

  add_test(
    NAME shower_reconstruction
    COMMAND test_shower_reconstruction
  )

  set_tests_properties(
    shower_reconstruction
    PROPERTIES LABELS "root-io;shower-reconstruction"
  )

  add_executable(
    test_reconstructed_event_tree
    tests/test_reconstructed_event_tree.cxx
  )

  target_link_libraries(
    test_reconstructed_event_tree
    PRIVATE
      cshine_gamma_reconstructed_event_tree
  )

  add_test(
    NAME reconstructed_event_tree
    COMMAND test_reconstructed_event_tree
  )

  set_tests_properties(
    reconstructed_event_tree
    PROPERTIES LABELS "root-io;event-level-reconstruction"
  )

  add_executable(
    test_reconstructed_spectrum_merge
    tests/test_reconstructed_spectrum_merge.cxx
  )

  target_link_libraries(
    test_reconstructed_spectrum_merge
    PRIVATE
      cshine_gamma_reconstructed_spectrum_merge
  )

  add_test(
    NAME reconstructed_spectrum_merge
    COMMAND test_reconstructed_spectrum_merge
  )

  set_tests_properties(
    reconstructed_spectrum_merge
    PROPERTIES LABELS "root-io;run-spectrum-merge"
  )

  add_executable(
    test_chapter3_diagnostics
    tests/test_chapter3_diagnostics.cxx
  )

  target_link_libraries(
    test_chapter3_diagnostics
    PRIVATE
      cshine_gamma_chapter3_diagnostics
  )

  add_test(
    NAME chapter3_diagnostics
    COMMAND test_chapter3_diagnostics
  )

  set_tests_properties(
    chapter3_diagnostics
    PROPERTIES LABELS "root-io;chapter3-diagnostics"
  )

  add_executable(
    test_trigger_diagnostics
    tests/test_trigger_diagnostics.cxx
  )

  target_link_libraries(
    test_trigger_diagnostics
    PRIVATE
      cshine_gamma_trigger_diagnostics
  )

  add_test(
    NAME trigger_diagnostics
    COMMAND test_trigger_diagnostics
  )

  set_tests_properties(
    trigger_diagnostics
    PROPERTIES LABELS "root-io;trigger-diagnostics"
  )

  add_executable(test_fast_coincidence_spectra
    tests/test_fast_coincidence_spectra.cxx)
  target_link_libraries(test_fast_coincidence_spectra PRIVATE
    cshine_gamma_fast_coincidence_spectra)
  add_test(NAME fast_coincidence_spectra
    COMMAND test_fast_coincidence_spectra)
  set_tests_properties(fast_coincidence_spectra PROPERTIES
    LABELS "root-io;fast-coincidence-cross-check")

  add_executable(test_observed_spectrum tests/test_observed_spectrum.cxx)
  target_link_libraries(test_observed_spectrum PRIVATE
    cshine_gamma_observed_spectrum)
  add_test(NAME observed_spectrum COMMAND test_observed_spectrum)
  set_tests_properties(observed_spectrum PROPERTIES
    LABELS "root-io;background-subtraction")

  add_executable(test_observed_spectrum_interface
    tests/test_observed_spectrum_interface.cxx)
  target_link_libraries(test_observed_spectrum_interface PRIVATE
    cshine_gamma_observed_spectrum)
  add_test(NAME observed_spectrum_interface
    COMMAND test_observed_spectrum_interface)
  set_tests_properties(observed_spectrum_interface PROPERTIES
    LABELS "root-io;analysis-interface")
endif()

install(
  TARGETS
    cshine_gamma_calibration
    cshine_gamma_source_background
    cshine_gamma_gain_relation
    cshine_gamma_energy_calibration
    cshine_gamma_time_calibration
    cshine_gamma_time_fit_outputs
    cshine_gamma_time_amplitude_spectra
    cshine_gamma_calibrated_event_tree
    cshine_gamma_neighbor_time_diagnostics
    cshine_gamma_reconstruction
    cshine_gamma_shower_reconstruction
    cshine_gamma_reconstructed_event_tree
    cshine_gamma_reconstructed_spectrum_merge
    cshine_gamma_chapter3_diagnostics
    cshine_gamma_trigger_diagnostics
    cshine_gamma_fast_coincidence_spectra
    cshine_gamma_observed_spectrum
    build_source_spectra
    fit_gain_relation
    fit_energy_calibration
    inspect_time_fit_outputs
    build_time_amplitude_spectra
    build_calibrated_event_tree
    build_neighbor_time_diagnostics
    build_reconstructed_event_tree
    merge_reconstructed_spectra
    build_chapter3_diagnostics
    build_trigger_diagnostics
    build_fast_coincidence_spectra
    build_observed_spectrum
    inspect_observed_spectrum
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
  FILES ${CALIBRATION_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  FILES
    include/source_background_spectra.h
    include/gain_relation.h
    include/energy_calibration.h
    include/time_calibration.h
    include/time_fit_outputs.h
    include/time_amplitude_spectra.h
    include/gamma_raw_data.hpp
    include/calibrated_event_tree.h
    include/neighbor_time_diagnostics.h
    include/jiugong_recon.h
    include/shower_reconstruction.h
    include/reconstructed_event_tree.h
    include/reconstructed_spectrum_merge.h
    include/gamma_position.hpp
    include/spatial_spread.hpp
    include/chapter3_diagnostics.h
    include/trigger_diagnostics.h
    include/fast_coincidence_spectra.h
    include/observed_spectrum.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libcshine_gamma_calibration_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/libcshine_gamma_calibration.rootmap
    ${CMAKE_CURRENT_BINARY_DIR}/libcshine_gamma_reconstruction_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/libcshine_gamma_reconstruction.rootmap
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
