set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

if (CONAN_AUTO_INSTALL)
  # Download automatically the cmake-conan integration file
  if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
     message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
     file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/master/conan.cmake"
                    "${CMAKE_BINARY_DIR}/conan.cmake"
                    TLS_VERIFY ON)
  endif()

  include(${CMAKE_BINARY_DIR}/conan.cmake)

  conan_cmake_autodetect(settings)
  conan_cmake_install(PATH_OR_REFERENCE ..
                      BUILD missing
                      REMOTE conancenter
                      OPTIONS webready=True
                      SETTINGS ${settings})
endif()

if (APPLE)
    # On Apple, we use the conan cmake_paths generator
    if (EXISTS ${CMAKE_BINARY_DIR}/conan_paths.cmake)
        include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
    endif()
else()
    # Otherwise, we rely on the conan cmake_find_package generator
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
    list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
endif()

find_package (Python3 COMPONENTS Interpreter)
if (NOT Python3_Interpreter_FOUND)
    message(WARNING "Python3 was not found. Python tests under the 'tests' folder will not be executed")
endif()

find_package(Filesystem REQUIRED)

# don't use Frameworks on the Mac (#966)
if (APPLE)
     set(CMAKE_FIND_FRAMEWORK NEVER)
endif()

if( EXIV2_ENABLE_PNG )
    find_package( ZLIB REQUIRED )
endif( )

if( EXIV2_ENABLE_WEBREADY )
    if( EXIV2_ENABLE_CURL )
        find_package(CURL REQUIRED)
    endif()
endif()

if (EXIV2_ENABLE_XMP AND EXIV2_ENABLE_EXTERNAL_XMP)
    message(FATAL_ERROR "EXIV2_ENABLE_XMP AND EXIV2_ENABLE_EXTERNAL_XMP are mutually exclusive.  You can only choose one of them")
else()
    if (EXIV2_ENABLE_XMP)
        find_package(EXPAT REQUIRED)
    elseif (EXIV2_ENABLE_EXTERNAL_XMP)
        find_package(XmpSdk REQUIRED)
    endif ()
endif()

if (EXIV2_ENABLE_NLS)
    find_package(Intl REQUIRED)
endif( )

find_package(Iconv)
if( ICONV_FOUND )
    message ( "-- ICONV_INCLUDE_DIR : " ${Iconv_INCLUDE_DIR} )
    message ( "-- ICONV_LIBRARIES : " ${Iconv_LIBRARY} )
endif()

if( BUILD_WITH_CCACHE )
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        message(STATUS "Program ccache found")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    endif()
endif()

