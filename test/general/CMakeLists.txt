CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

INCLUDE("../cmake/gather.cmake")

GET_PROJECT_NAME(PROJECT_ID)

PROJECT (${PROJECT_ID})

#Adding all subdirectories to build
SUBDIRLIST(SUBDIRS ${CMAKE_CURRENT_LIST_DIR})

FOREACH(subdir ${SUBDIRS})
    ADD_SUBDIRECTORY(${subdir})
ENDFOREACH()
