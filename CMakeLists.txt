cmake_minimum_required(VERSION 3.21)
project(KontrolLights VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# DLL autonome sous Windows (pas besoin du runtime Visual C++)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(MINGW)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static")
endif()
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
endif()

include(FetchContent)

# Gig Performer SDK (official)
FetchContent_Declare(gp-sdk
  GIT_REPOSITORY https://github.com/gigperformer/gp-sdk.git
  GIT_TAG        e5ae8ed0a219aef2fb5a91ce4fd730406b1accfa)  # branche beta-sdk-v62 : SDK v62, Gig Performer 5
FetchContent_MakeAvailable(gp-sdk)

# hidapi, lié statiquement pour que la DLL/dylib soit autonome
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(HIDAPI_WITH_LIBUSB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(hidapi
  GIT_REPOSITORY https://github.com/libusb/hidapi.git
  GIT_TAG        hidapi-0.14.0)
FetchContent_MakeAvailable(hidapi)

add_library(KontrolLights SHARED
  src/LibMain.cpp
  src/LibMain.h
  src/KontrolDevice.cpp
  src/KontrolDevice.h
  src/Zones.cpp
  src/Zones.h)

target_include_directories(KontrolLights PRIVATE shared)
target_link_libraries(KontrolLights PRIVATE gigperformer::sdk::cpp hidapi::hidapi)

set_target_properties(KontrolLights PROPERTIES PREFIX "")
if(APPLE)
  set_target_properties(KontrolLights PROPERTIES SUFFIX ".dylib")
  target_link_libraries(KontrolLights PRIVATE "-framework IOKit" "-framework CoreFoundation")
endif()

# Dossier d'extensions de Gig Performer
if(WIN32)
  set(GP_EXT_DIR "C:/ProgramData/Gig Performer/Extensions")  # = C:\Users\All Users\...
elseif(APPLE)
  set(GP_EXT_DIR "/Users/Shared/Gig Performer/Extensions")
endif()
if(GP_EXT_DIR)
  install(TARGETS KontrolLights LIBRARY DESTINATION "${GP_EXT_DIR}" RUNTIME DESTINATION "${GP_EXT_DIR}")
endif()
