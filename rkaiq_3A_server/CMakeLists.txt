
if(NOT CMAKE_BUILD_TYPE)
    # default to Release build for GCC builds
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif()

cmake_minimum_required(VERSION 2.8)

#project name
PROJECT(rkaiq_3A_server)

#set BUILROOT_BUILD_PROJECT
#set(BUILROOT_BUILD_PROJECT true)
#set(ENABLE_ASAN true)

# ----------------------------------------------------------------------------
# SET THE COMPILER PATH
# ----------------------------------------------------------------------------
if(BUILROOT_BUILD_PROJECT)
    SET(CMAKE_C_COMPILER "/home/camera/camera/rv1109_sdk/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm- linux-gnueabihf-gcc") 
	SET(CMAKE_CXX_COMPILER "/home/camera/camera/rv1109_sdk/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++")
else()
    SET(CMAKE_C_COMPILER "/home/camera/camera/rv1109_sdk/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc") 
    SET(CMAKE_CXX_COMPILER "/home/camera/camera/rv1109_sdk/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++")
endif()

# ----------------------------------------------------------------------------
# ADD SYMBOL TABLE FOR GDB
# ----------------------------------------------------------------------------
# SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -ggdb")
# SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -ggdb")

# ----------------------------------------------------------------------------
# set property to classify library kinds
# ----------------------------------------------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

#set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

#set ADD_RK_AIQ
set(ADD_RK_AIQ true)

#SET RK_AIQ SOURCE DIR
set(RK_AIQ_SOURCE_DIR ${PROJECT_SOURCE_DIR}/..)
message("rkaiq src path" ${RK_AIQ_SOURCE_DIR})

if(ENABLE_ASAN)
add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
SET(LINK_ASAN asan)
endif()

if(ADD_RK_AIQ)
add_compile_options(-std=c++11)
add_compile_options(-DADD_RK_AIQ)
add_compile_options(-O0)
# add_compile_options(-v)
SET(RK_AIQ_LIB_DIR ${RK_AIQ_SOURCE_DIR}/build/linux/all_lib/${CMAKE_BUILD_TYPE})
message("rkaiq lib path" ${RK_AIQ_LIB_DIR})
SET(RK_AIQ_INCLUDE_DIR
${RK_AIQ_SOURCE_DIR}/include/xcore/
${RK_AIQ_SOURCE_DIR}/include/xcore/base/
#${RK_AIQ_SOURCE_DIR}/include/aiq_core/
${RK_AIQ_SOURCE_DIR}/include/algos/
${RK_AIQ_SOURCE_DIR}/include/common/
${RK_AIQ_SOURCE_DIR}/include/iq_parser/
${RK_AIQ_SOURCE_DIR}/include/uAPI/
${RK_AIQ_SOURCE_DIR}/
)
SET(RK_AIQ_LIB rkaiq)
endif()

#head file path
include_directories(
${RK_AIQ_INCLUDE_DIR}
)

# set output binary path
if(WIN32)
    SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/exe)
    SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/all_lib/)
    SET(CMAKE_LIBRARY_PATH ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})
else()
    SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/exe/${CMAKE_BUILD_TYPE})
    SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/all_lib/${CMAKE_BUILD_TYPE})
    SET(CMAKE_LIBRARY_PATH ${LIBRARY_OUTPUT_PATH})
endif()

#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
#set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
message(${CMAKE_LIBRARY_PATH})
message(${LIBRARY_OUTPUT_PATH})

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

#
link_directories("${LIBRARY_OUTPUT_PATH}" "${RK_AIQ_LIB_DIR}" "${RK_DRM_LIB_DIR}")
#add executable file
ADD_EXECUTABLE(rkaiq_3A_server rkaiq_3A_server.cpp)
target_link_libraries(rkaiq_3A_server ${RK_AIQ_LIB} pthread)
