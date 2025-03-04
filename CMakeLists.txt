set(PLUGIN_AUTHOR "StreamMyHeart")
set(PLUGIN_URL "https://github.com/Maciek03102003/StreamMyHeart")

cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" OFF)

option(BUILD_OBS_PLUGIN "Build as OBS plugin" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(UNIX)
  set(CMAKE_C_FLAGS "-Wall -std=c99 -fPIC -Wno-unused-parameter -Wno-unused-private-field")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -g")
  set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS} -Os -DNDEBUG -fvisibility=hidden")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O3 -DNDEBUG -fvisibility=hidden")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS} -O2 -g -fvisibility=hidden")

  set(CMAKE_CXX_FLAGS "-Wall -ffunction-sections -fdata-sections -fPIC -Wno-unused-parameter -Wno-unused-private-field")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS} -Os -DNDEBUG -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O2 -g -fvisibility=hidden")
endif()

message(
  "=======CMAKE cur_ver: ${CMAKE_VERSION}, cur_build_type:${CMAKE_BUILD_TYPE}, CMAKE_SYSTEM_NAME:${CMAKE_SYSTEM_NAME}"
)

include(compilerconfig)
include(defaults)
include(helpers)

if(BUILD_OBS_PLUGIN)
  add_library(${CMAKE_PROJECT_NAME} MODULE)
else()
  add_executable(${CMAKE_PROJECT_NAME})
endif()

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(Eigen3 REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIR})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Eigen3::Eigen)

if(UNIX)
  message("Configuring OpenCV for MacOS and Linux")
  set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/binary/opencv-4.11.0-macos/lib/cmake/opencv4)
elseif(WIN32)
  set(OpenCV_STATIC ON)
  set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/binary/opencv-4.11.0-windows)
endif()

find_package(OpenCV CONFIG REQUIRED)
if(OpenCV_FOUND)
  message("OpenCV_FOUND:${OpenCV_FOUND}")
  message("OpenCV_VERSION:${OpenCV_VERSION}")
  message("OpenCV_INCLUDE_DIRS:${OpenCV_INCLUDE_DIRS}")
  message("OpenCV_LIBRARIES:${OpenCV_LIBRARIES}")
endif()

include_directories(${OpenCV_INCLUDE_DIR})
link_directories(${OpenCV_LIBRARIES})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${OpenCV_LIBRARIES})

# Apply compiler options to ignore Dlib-specific warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE
      -Wno-newline-eof # Ignore missing newline at end of file
      -Wno-unused-but-set-variable # Ignore unused variables
      -Wno-pessimizing-move # Ignore pessimizing move warnings
      -Wno-comma # Ignore comma operator warnings
      -Wno-error # Do not treat warnings as errors
  )
endif()

if(UNIX)
  include_directories(${CMAKE_SOURCE_DIR}/3rdparty/dlib)
  link_directories(${CMAKE_SOURCE_DIR}/3rdparty/dlib/build)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE dlib)

  # Set paths for Dlib
  set(DLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dlib-19.24.6)
  set(DLIB_LIB_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dlib-19.24.6/build/dlib)

  # Include Dlib headers
  include_directories(${DLIB_INCLUDE_DIR})

  # Link against the static Dlib library
  add_library(libdlib STATIC IMPORTED)
  set_target_properties(libdlib PROPERTIES IMPORTED_LOCATION ${DLIB_LIB_DIR}/libdlib.a)

  # Link your OBS plugin with Dlib
  target_link_libraries(${PROJECT_NAME} PRIVATE ${DLIB_LIB_DIR}/libdlib.a)
  # set(dlib_STATIC ON)
  # set(dlib_DIR ${CMAKE_SOURCE_DIR}/binary/dlib-19.24.6-macos/lib/cmake/dlib)
  # find_package(dlib CONFIG REQUIRED)
  # if(dlib_FOUND)
  #   message("dlib_FOUND:${dlib_FOUND}")
  #   message("dlib_VERSION:${dlib_VERSION}")
  #   message("dlib_INCLUDE_DIRS:${dlib_INCLUDE_DIRS}")
  #   message("dlib_LIBRARIES:${dlib_LIBRARIES}")
  # endif()

  # target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE dlib::dlib)
elseif(WIN32)
  set(dlib_STATIC ON)
  set(dlib_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dlib-19.24.6/install/lib/cmake/dlib)

  find_package(dlib CONFIG REQUIRED)
  if(dlib_FOUND)
    message("dlib_FOUND:${dlib_FOUND}")
    message("dlib_VERSION:${dlib_VERSION}")
    message("dlib_INCLUDE_DIRS:${dlib_INCLUDE_DIRS}")
    message("dlib_LIBRARIES:${dlib_LIBRARIES}")
  endif()

  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE dlib::dlib)
endif()

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/algorithm/heart_rate_algorithm.cpp
    src/algorithm/face_detection/face_detection.cpp
    src/algorithm/face_detection/opencv_haarcascade.cpp
    src/algorithm/face_detection/opencv_dlib_68_landmarks_face_tracker.cpp
    src/graph_source.cpp
    src/graph_source_info.c
    src/algorithm/filtering/pre_filters.cpp
    src/algorithm/filtering/post_filters.cpp
    src/algorithm/filtering/filter_util.cpp
    src/plugin-main.cpp
    src/heart_rate_source.cpp
    src/heart_rate_source_info.c
    src/obs_utils.cpp
    eval/run_evaluation.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
