cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" OFF)

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

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(Eigen3 REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIR})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Eigen3::Eigen)

if(UNIX)
  message("Configuring OpenCV for MacOS and Linux")
  set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/binary/opencv-4.11.0-macos/lib/cmake/opencv4)
elseif(WIN32)
  # set(OpenCV_DIR "C:/tools/opencv/build")
  # set(OpenCV_INCLUDE_DIR "C:/tools/opencv/build/include")
  # set(OpenCV_LIBRARIES "C:/tools/opencv/build/x64/vc16/lib")
  # find_package(OpenCV REQUIRED)
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

if (UNIX)
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
elseif(WIN32)
  include_directories(${CMAKE_SOURCE_DIR}/3rdparty/dlib)
  link_directories(${CMAKE_SOURCE_DIR}/3rdparty/dlib/build)
  # target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE dlib)

  # Set paths for Dlib
  set(DLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dlib-19.24.6)
  set(DLIB_LIB_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dlib-19.24.6/build/dlib/Release)

  # Include Dlib headers
  include_directories(${DLIB_INCLUDE_DIR})

  # Link against the static Dlib library
  add_library(libdlib STATIC IMPORTED)
  set_target_properties(libdlib PROPERTIES IMPORTED_LOCATION ${DLIB_LIB_DIR}/lib19.24.6_release_64bit_msvc1942.lib)

  # Link your OBS plugin with Dlib"
  target_link_libraries(${PROJECT_NAME} PRIVATE ${DLIB_LIB_DIR}/lib19.24.6_release_64bit_msvc1942.lib)
#   set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
#   set(CMAKE_POSITION_INDEPENDENT_CODE True)
#   set(DLIB_NO_GUI_SUPPORT ON)
#   set(DLIB_PNG_SUPPORT OFF)
#   set(DLIB_GIF_SUPPORT OFF)
#   set(DLIB_USE_FFMPEG OFF)
#   set(DLIB_JPEG_SUPPORT OFF)
#   set(DLIB_LINK_WITH_SQLITE3 OFF)
#   set(DLIB_WEBP_SUPPORT OFF)
#   set(DLIB_JXL_SUPPORT OFF)
#   set(DLIB_TEST_COMPILE_ALL_SOURCE_CPP OFF)
#   add_subdirectory(dlib)
#   # include_directories(${CMAKE_SOURCE_DIR}/dlib)
#   target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE dlib)
endif()

# if(WIN32)
# 	# Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
# 	if (MSVC)
# 		add_definitions(/MP /d2FH4-)
# 		add_definitions("-D_USE_MATH_DEFINES")
# 		add_definitions("-D_CRT_SECURE_NO_WARNINGS") # to avoid a warning for `fopen`
# 		add_definitions("-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR") # TODO: Remove once requiring OBS 30.2 or later.
# 	endif()
# endif()

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
    src/algorithm/HeartRateAlgorithm.cpp
    src/algorithm/face_detection/opencv_haarcascade.cpp
    src/plugin-main.cpp
    src/heart_rate_source.cpp
    src/heart_rate_source_info.c
    src/algorithm/face_detection/opencv_dlib_68_landmarks_face_tracker.cpp
    src/algorithm/face_detection/opencv_dlib_68_landmarks.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
