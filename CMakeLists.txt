cmake_minimum_required(VERSION 3.15)

# Project configuration
project(OcctImgui LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})

# Include directories
find_package(imgui REQUIRED)
find_package(OpenCASCADE REQUIRED)
find_package(Netgen CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS signals2 unit_test_framework filesystem system)
find_package(spdlog REQUIRED)
option(LIBIGL_USE_STATIC_LIBRARY "Use libIGL as static library" ON)
find_package(libigl REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(nlohmann_json REQUIRED)

# Add nfd (Native File Dialog) library
add_subdirectory(third_party/nfd EXCLUDE_FROM_ALL)

# IFR integration
option(OCCTIMGUI_IFR_USE_SOURCE "Build IFR feature recognizer directly from source instead of linking the prebuilt binaries" OFF)
set(IFR_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/IFR")
set(IFR_INSTALL_DIR "${IFR_ROOT_DIR}/installed")
set(OCCTIMGUI_ENABLE_IFR OFF)
set(OCCTIMGUI_ENABLE_IFR_SOURCE OFF)
set(IFR_LINK_TARGET "")

if(OCCTIMGUI_IFR_USE_SOURCE)
    if(EXISTS "${IFR_ROOT_DIR}/src/featureRecognizer/recognizer/CNC_FeatureRecognizer.h")
        include(ExternalProject)

        set(IFR_EXTERNAL_BUILD_DIR "${CMAKE_BINARY_DIR}/ifr_build")
        set(IFR_EXTERNAL_INSTALL_DIR "${CMAKE_BINARY_DIR}/ifr_install")

        set(IFR_EP_CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${IFR_EXTERNAL_INSTALL_DIR}
            -DVCPKG_MANIFEST_MODE=OFF
        )

        if(CMAKE_TOOLCHAIN_FILE)
            list(APPEND IFR_EP_CMAKE_ARGS "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
        endif()
        if(DEFINED VCPKG_TARGET_TRIPLET)
            list(APPEND IFR_EP_CMAKE_ARGS "-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")
        endif()
        if(CMAKE_PREFIX_PATH)
            list(JOIN CMAKE_PREFIX_PATH ";" IFR_PREFIX_PATH_JOINED)
            list(APPEND IFR_EP_CMAKE_ARGS "-DCMAKE_PREFIX_PATH=${IFR_PREFIX_PATH_JOINED}")
        endif()
        list(APPEND IFR_EP_CMAKE_ARGS "-DVCPKG_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
        if(CMAKE_BUILD_TYPE)
            list(APPEND IFR_EP_CMAKE_ARGS "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
        endif()
        if(DEFINED OpenCASCADE_DIR)
            list(APPEND IFR_EP_CMAKE_ARGS "-DOpenCASCADE_DIR=${OpenCASCADE_DIR}")
        endif()

        set(IFR_EP_CONFIG_ARGS "")
        if(CMAKE_CONFIGURATION_TYPES)
            set(IFR_EP_CONFIG_ARGS --config ${CMAKE_CFG_INTDIR})
        elseif(CMAKE_BUILD_TYPE)
            set(IFR_EP_CONFIG_ARGS --config ${CMAKE_BUILD_TYPE})
        endif()

        ExternalProject_Add(IFRExternal
            SOURCE_DIR ${IFR_ROOT_DIR}
            BINARY_DIR ${IFR_EXTERNAL_BUILD_DIR}
            CMAKE_GENERATOR ${CMAKE_GENERATOR}
            CMAKE_GENERATOR_PLATFORM ${CMAKE_GENERATOR_PLATFORM}
            CMAKE_GENERATOR_TOOLSET ${CMAKE_GENERATOR_TOOLSET}
            CMAKE_ARGS ${IFR_EP_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${IFR_EXTERNAL_INSTALL_DIR}/lib/featureRecognizer.lib"
            UPDATE_COMMAND ""
            BUILD_COMMAND ${CMAKE_COMMAND} --build . ${IFR_EP_CONFIG_ARGS}
            INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install ${IFR_EP_CONFIG_ARGS}
        )

        file(MAKE_DIRECTORY
            "${IFR_EXTERNAL_INSTALL_DIR}/include"
            "${IFR_EXTERNAL_INSTALL_DIR}/lib"
            "${IFR_EXTERNAL_INSTALL_DIR}/bin"
        )

        add_library(ifr_featureRecognizer SHARED IMPORTED GLOBAL)
        set_target_properties(ifr_featureRecognizer PROPERTIES
            IMPORTED_IMPLIB "${IFR_EXTERNAL_INSTALL_DIR}/lib/featureRecognizer.lib"
            IMPORTED_LOCATION "${IFR_EXTERNAL_INSTALL_DIR}/bin/featureRecognizer.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${IFR_EXTERNAL_INSTALL_DIR}/include"
        )

        add_dependencies(ifr_featureRecognizer IFRExternal)

        set(IFR_INSTALL_DIR "${IFR_EXTERNAL_INSTALL_DIR}")
        set(IFR_LINK_TARGET ifr_featureRecognizer)
        set(OCCTIMGUI_ENABLE_IFR ON)
        set(OCCTIMGUI_ENABLE_IFR_SOURCE ON)

    else()
        message(FATAL_ERROR "OCCTIMGUI_IFR_USE_SOURCE is ON, but IFR sources were not found at ${IFR_ROOT_DIR}")
    endif()
else()
    if(EXISTS "${IFR_INSTALL_DIR}/include" AND EXISTS "${IFR_INSTALL_DIR}/lib/featureRecognizer.lib")
        message(STATUS "IFR detected at ${IFR_INSTALL_DIR}")
        add_library(ifr_featureRecognizer SHARED IMPORTED GLOBAL)
        set_target_properties(ifr_featureRecognizer PROPERTIES
            IMPORTED_IMPLIB "${IFR_INSTALL_DIR}/lib/featureRecognizer.lib"
            IMPORTED_LOCATION "${IFR_INSTALL_DIR}/bin/featureRecognizer.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${IFR_INSTALL_DIR}/include"
        )
        set(IFR_LINK_TARGET ifr_featureRecognizer)
        set(OCCTIMGUI_ENABLE_IFR ON)
        set(OCCTIMGUI_ENABLE_IFR_SOURCE OFF)
    else()
        message(WARNING "IFR installed directory not found. Feature recognition will use mock implementation.")
    endif()
endif()

# Enable testing with CTest
include(CTest)
enable_testing()

# Define test data directory
set(MESH_TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
file(MAKE_DIRECTORY ${MESH_TEST_DATA_DIR})

# Create a library for shared components that will be used in both the main app and tests
add_library(OcctImguiLib STATIC
    src/ais/Mesh_DataSource.cpp
    src/mvvm/SelectionManager.cpp
    src/model/IModel.cpp
    src/model/GeometryModel.cpp
    src/model/ModelFactory.cpp
    src/model/ModelManager.cpp
    src/model/ModelImporter.cpp
    src/model/FeatureRecognitionModel.cpp
    src/model/FeatureRecognitionModel_Mock.cpp  # Using mock implementation temporarily
    src/view/ImGuiView.cpp
    src/view/OcctView.cpp
    src/view/FeatureRecognitionView.cpp
    src/viewmodel/GeometryViewModel.cpp
    src/viewmodel/FeatureRecognitionViewModel.cpp
    src/viewmodel/ViewModelManager.cpp
    src/window/WindowManager.cpp
    src/input/InputManager.cpp
    src/ApplicationBootstrapper.cpp
    src/Application.cpp
    src/GlfwOcctWindow.cpp
    src/utils/Logger.cpp
    src/utils/LoggerManager.cpp
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(OcctImguiLib PUBLIC OCCTIMGUI_DEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(OcctImguiLib PUBLIC OCCTIMGUI_RELWITHDEBINFO)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(OcctImguiLib PUBLIC OCCTIMGUI_RELEASE)
endif()

target_include_directories(OcctImguiLib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(OCCTIMGUI_IFR_LOCAL_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/brep
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/brep/auxiliary
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/brep/algorithm
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/brep/canrec
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/brep/hlr
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/brep/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/brep/interop
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/brep/asiActiveDataAPI
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/brep/asiActiveDataMesh
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/features
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/hybridGraph
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/mesh
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/mesh/algorithm
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/mesh/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/IFR
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/featureRecognizer/recognizer
    ${CMAKE_CURRENT_SOURCE_DIR}/IFR/src/VF3/include
)

if(OCCTIMGUI_ENABLE_IFR)
    target_link_libraries(OcctImguiLib PUBLIC ${IFR_LINK_TARGET})
    target_compile_definitions(OcctImguiLib PUBLIC OCCTIMGUI_ENABLE_IFR)
    target_include_directories(OcctImguiLib PUBLIC ${OCCTIMGUI_IFR_LOCAL_INCLUDE_DIRS})

    if(OCCTIMGUI_IFR_USE_SOURCE)
        add_dependencies(OcctImguiLib IFRExternal)
    endif()
endif()

target_link_libraries(OcctImguiLib
    PUBLIC
    ${OpenCASCADE_LIBRARIES}
    igl::igl_core
    Eigen3::Eigen
    nglib
    ngcore
    imgui::imgui
    spdlog::spdlog
    nfd::nfd
    igl_copyleft::igl_copyleft_cgal
    Boost::signals2
    Boost::filesystem
    Boost::system
    nlohmann_json::nlohmann_json
)


# Add executable target
add_executable(OcctImgui
    src/main.cpp
)

target_include_directories(OcctImgui
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/window
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input
)

# Link libraries
target_link_libraries(OcctImgui
    PRIVATE
    OcctImguiLib
)

if(OCCTIMGUI_ENABLE_IFR AND OCCTIMGUI_IFR_USE_SOURCE)
    add_dependencies(OcctImgui IFRExternal)
endif()

if(OCCTIMGUI_ENABLE_IFR AND WIN32)
    if(OCCTIMGUI_IFR_USE_SOURCE)
        add_custom_command(TARGET OcctImgui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:ifr_featureRecognizer>
                $<TARGET_FILE_DIR:OcctImgui>)
    else()
        add_custom_command(TARGET OcctImgui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${IFR_INSTALL_DIR}/bin/featureRecognizer.dll"
                $<TARGET_FILE_DIR:OcctImgui>)
    endif()
endif()

target_compile_options(OcctImgui PRIVATE
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

option(OCCTIMGUI_BUILD_TESTING "Build tests" ON)

# Tests configuration
if(OCCTIMGUI_BUILD_TESTING)
    # Create a function to add tests easily
    function(add_boost_test TEST_NAME TEST_SOURCE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_include_directories(${TEST_NAME}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/window
            ${CMAKE_CURRENT_SOURCE_DIR}/src/input
        )
        target_link_libraries(${TEST_NAME}
            PRIVATE
            OcctImguiLib
            Boost::unit_test_framework

            # Add other dependencies needed by your tests
        )

        # Define test target
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

        # Tell Boost.Test to use dynamic linking
        target_compile_definitions(${TEST_NAME} PRIVATE
            BOOST_TEST_DYN_LINK
            MESH_TEST_DATA_DIR="${MESH_TEST_DATA_DIR}"
        )
    endfunction()

    # Add the tests
    add_boost_test(model_test tests/model_test.cpp)
    add_boost_test(model_manager_test tests/model_manager_test.cpp)
    add_boost_test(mesh_datasource_test tests/mesh_datasource_test.cpp)
    add_boost_test(geometry_model_test tests/geometry_model_test.cpp)
    add_boost_test(model_importer_test tests/model_importer_test.cpp)
    add_boost_test(signal_test tests/signal_test.cpp)
    add_boost_test(message_bus_test tests/message_bus_test.cpp)
    add_boost_test(mvvm_integration_test tests/mvvm_integration_test.cpp)
    add_boost_test(property_test tests/property_test.cpp)
    add_boost_test(selection_manager_test tests/selection_manager_test.cpp)
endif()
