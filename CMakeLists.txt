cmake_minimum_required ( VERSION 2.8.5 )
cmake_policy ( SET CMP0054 NEW )
project ( StageCast-Audio )

include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/tests.cmake )

find_package ( PkgConfig )

pkg_check_modules ( ALSA REQUIRED alsa )
include_directories ( ${ALSA_INCLUDE_DIRS} )
add_compile_options ( ${ALSA_CFLAGS} )

pkg_check_modules ( DBus REQUIRED dbus-1 )
include_directories ( ${DBus_INCLUDE_DIRS} )
add_compile_options ( ${DBus_CFLAGS} )

pkg_check_modules ( Opus REQUIRED opus )
include_directories ( ${Opus_INCLUDE_DIRS} )
add_compile_options ( ${Opus_CFLAGS} )

pkg_check_modules ( Sndfile REQUIRED sndfile )
include_directories ( ${Sndfile_INCLUDE_DIRS} )
add_compile_options ( ${Sndfile_CFLAGS} )

pkg_check_modules ( AVFormat REQUIRED libavformat libavutil )
include_directories ( ${AVFormat_INCLUDE_DIRS} )
add_compile_options ( ${AVFormat_CFLAGS} )

pkg_check_modules ( AVCodec REQUIRED libavcodec libavutil )
include_directories ( ${AVCodec_INCLUDE_DIRS} )
add_compile_options ( ${AVCodec_CFLAGS} )

pkg_check_modules ( SSL REQUIRED libssl libcrypto )
include_directories ( ${SSL_INCLUDE_DIRS} )
add_compile_options ( ${SSL_CFLAGS} )

pkg_check_modules ( CryptoPP REQUIRED libcrypto++ )
include_directories ( ${CryptoPP_INCLUDE_DIRS} )
add_compile_options ( ${CryptoPP_CFLAGS} )

pkg_check_modules ( Rubberband REQUIRED rubberband samplerate fftw3 )
include_directories ( ${Rubberband_INCLUDE_DIRS} )
add_compile_options ( ${Rubberband_CFLAGS} )

pkg_check_modules ( V4L REQUIRED libv4l2 libjpeg libswscale )
include_directories ( ${V4L_INCLUDE_DIRS} )
add_compile_options ( ${V4L_CFLAGS} )

pkg_check_modules ( X264 REQUIRED x264 )
include_directories ( ${X264_INCLUDE_DIRS} )
add_compile_options ( ${X264_CFLAGS_OTHER} )

pkg_check_modules ( JSON REQUIRED jsoncpp )
include_directories ( SYSTEM ${JSON_INCLUDE_DIRS} )
add_compile_options ( ${JSON_CFLAGS_OTHER} )

include_directories ("${PROJECT_SOURCE_DIR}/src/util")
include_directories ("${PROJECT_SOURCE_DIR}/src/crypto")
include_directories ("${PROJECT_SOURCE_DIR}/src/http")
include_directories ("${PROJECT_SOURCE_DIR}/src/audio")
include_directories ("${PROJECT_SOURCE_DIR}/src/network")
include_directories ("${PROJECT_SOURCE_DIR}/src/playback")
include_directories ("${PROJECT_SOURCE_DIR}/src/server")
include_directories ("${PROJECT_SOURCE_DIR}/src/control")
include_directories ("${PROJECT_SOURCE_DIR}/src/video")
include_directories ("${PROJECT_SOURCE_DIR}/src/tests")
include_directories ("${PROJECT_SOURCE_DIR}/src/dbus")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src/util")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/crypto")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/http")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/audio")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/network")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/playback")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/server")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/control")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/video")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/dbus")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/frontend")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/tests")
