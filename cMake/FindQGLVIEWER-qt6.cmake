# - Try to find QGLViewer built against Qt6
# Once done this will define
#
#  QGLVIEWER_FOUND - system has QGLViewer
#  QGLVIEWER_INCLUDE_DIR - the QGLViewer include directory
#  QGLVIEWER_LIBRARIES - Link these to use QGLViewer

find_path(QGLVIEWER_INCLUDE_DIR
          NAMES qglviewer.h
          PATHS /usr/include/QGLViewer /usr/local/include/QGLViewer /usr/include/qglviewer /usr/local/include/qglviewer
         )

find_library(QGLVIEWER_LIBRARIES
             NAMES QGLViewer-qt6 QGLViewer
)

IF(QGLVIEWER_INCLUDE_DIR AND QGLVIEWER_LIBRARIES)
   SET(QGLVIEWER_FOUND TRUE)
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DQGLVIEWER_FOUND")
ENDIF(QGLVIEWER_INCLUDE_DIR AND QGLVIEWER_LIBRARIES)


include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(QGLVIEWER-qt6 DEFAULT_MSG QGLVIEWER_INCLUDE_DIR QGLVIEWER_LIBRARIES)
