MACRO(CFHD_FIND varname headername)
    FIND_PATH(${varname}_INCLUDE_DIR ${headername}
        PATHS
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/include
        /usr/include
        /sw/include # Fink
        /opt/local/include # DarwinPorts
        /opt/csw/include # Blastwave
        /opt/include
        /usr/freeware/include
        PATH_SUFFIXES cineformsdk
        DOC "Location of CFHD Headers"
    )

    IF (NOT ${varname}_INCLUDE_DIR)
        message(FATAL_ERROR
        "${headername} not found.  Build/install: https://github.com/gopro/cineform-sdk.")
    ENDIF(NOT ${varname}_INCLUDE_DIR)

ENDMACRO(CFHD_FIND)

CFHD_FIND(CFHDENCODER  CFHDEncoder.h)
CFHD_FIND(CFHDMETADATA CFHDMetadata.h)
CFHD_FIND(CFHDVER      ver.h)

FIND_LIBRARY(CFHD_LIBRARY CFHDCodec
    PATHS
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
    DOC "Location of CFHD Library"
)

IF (NOT CFHD_LIBRARY)
    message(FATAL_ERROR
    "libCFHDCodec not found.  Build/install: https://github.com/gopro/cineform-sdk.")
ENDIF(NOT CFHD_LIBRARY)

SET(CFHD_FOUND "YES")
SET(CFHD_INCLUDE_DIR ${CFHDENCODER_INCLUDE_DIR})
