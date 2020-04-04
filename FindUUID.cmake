FIND_LIBRARY(UUID_LIBRARY uuid
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
    DOC "Location of UUID Library"
)

IF (NOT UUID_LIBRARY)
    message(FATAL_ERROR "libuuid not found.  On Ubuntu, install uuid-dev.")
ENDIF(NOT UUID_LIBRARY)

SET(UUID_FOUND "YES")
