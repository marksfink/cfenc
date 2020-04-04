MACRO(FFMPEG_FIND varname shortname headername)
    FIND_PATH(FFMPEG_${varname}_INCLUDE_DIR lib${shortname}/${headername}
        PATHS
        $ENV{FFMPEG_DIR}/include
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/include
        /usr/include
        /sw/include # Fink
        /opt/local/include # DarwinPorts
        /opt/csw/include # Blastwave
        /opt/include
        /usr/freeware/include
        PATH_SUFFIXES ffmpeg
        DOC "Location of FFmpeg Headers"
    )

    FIND_LIBRARY(FFMPEG_${varname}_LIBRARY ${shortname}
        PATHS
        $ENV{FFMPEG_DIR}/lib
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/local/lib64
        /usr/lib
        /usr/lib64
        /sw/lib
        /opt/local/lib
        /opt/csw/lib
        /opt/lib
        /usr/freeware/lib64
        DOC "Location of FFmpeg Libraries"
    )

    IF (NOT FFMPEG_${varname}_INCLUDE_DIR)
        string(CONCAT MSG
        "${headername} not found.  On Ubuntu, install libavformat-dev, libavcodec-dev, "
        "libavutil-dev and libswscale-dev.")
        message(FATAL_ERROR ${MSG})
    ENDIF(NOT FFMPEG_${varname}_INCLUDE_DIR)

    IF (NOT FFMPEG_${varname}_LIBRARY)
        message(FATAL_ERROR "lib${shortname} not found.  Installing FFmpeg should resolve it.")
    ENDIF(NOT FFMPEG_${varname}_LIBRARY)

ENDMACRO(FFMPEG_FIND)

FFMPEG_FIND(LIBAVFORMAT avformat avformat.h)
FFMPEG_FIND(LIBAVCODEC  avcodec  avcodec.h)
FFMPEG_FIND(LIBAVUTIL   avutil   pixdesc.h)
FFMPEG_FIND(LIBSWSCALE  swscale  swscale.h)

SET(FFMPEG_FOUND "YES")
SET(FFMPEG_INCLUDE_DIR ${FFMPEG_LIBAVFORMAT_INCLUDE_DIR})
SET(FFMPEG_LIBRARIES
    ${FFMPEG_LIBAVFORMAT_LIBRARY}
    ${FFMPEG_LIBAVCODEC_LIBRARY}
    ${FFMPEG_LIBAVUTIL_LIBRARY}
    ${FFMPEG_LIBSWSCALE_LIBRARY})
