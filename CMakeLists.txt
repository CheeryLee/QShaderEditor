cmake_minimum_required(VERSION 3.0.0)
project(QShaderEdit)

set(QShaderEdit_CMAKE_DIR "${QShaderEdit_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${QShaderEdit_CMAKE_DIR}")
set(CMAKE_CXX_FLAGS "-fPIC")

if(WIN32)
    set(GLEW_ROOT_DIR "${QShaderEdit_SOURCE_DIR}/extern/glew")
endif(WIN32)

set(Data_Dir ${QShaderEdit_SOURCE_DIR})

# Qt
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui OpenGL)
set(QT_USE_QTOPENGL true)
include_directories(
			${Qt5Core_INCLUDE_DIRS}
			${Qt5Gui_INCLUDE_DIRS}
			${Qt5Widgets_INCLUDE_DIRS}
			${Qt5OpenGL_INCLUDE_DIRS}
			${CMAKE_CURRENT_BINARY_DIR}/src 
			${CMAKE_CURRENT_BINARY_DIR}/src/completer)
#SET(LIBS ${LIBS} ${QT_QTCORE_LIBRARY_DEBUG} ${QT_QTGUI_LIBRARY_DEBUG} ${QT_QTOPENGL_LIBRARY_DEBUG})
set(LIBS ${LIBS} ${Qt5Core_LIBRARIES} ${Qt5Gui_LIBRARIES} ${Qt5OpenGL_LIBRARIES})

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

# OpenGL
find_package(OpenGL REQUIRED)
if(OPENGL_FOUND)
	message(STATUS "Looking for OpenGL - found")
	include_directories(${OPENGL_INCLUDE_PATH})
	set(LIBS ${LIBS} ${OPENGL_LIBRARIES})
else(OPENGL_FOUND)
	message(SEND_ERROR "Looking for OpenGL - not found")
endif(OPENGL_FOUND)

#GLEW 
include(${QShaderEdit_CMAKE_DIR}/FindGLEW.cmake)
if(FOUND_GLEW)
	message(STATUS "Looking for GLEW - found")
	include_directories(${GLEW_INCLUDE_PATH})
	set(LIBS ${LIBS} ${GLEW_LIBRARY})
else(FOUND_GLEW)
	message(SEND_ERROR "Looking for GLEW - not found")
endif(FOUND_GLEW)

# Cg
include(${QShaderEdit_CMAKE_DIR}/FindCg.cmake)
if(FOUND_CG)
	message(STATUS "Looking for Cg - found")
	include_directories(${CG_INCLUDE_PATH})
	set(LIBS ${LIBS} ${CG_LIBRARY} ${CG_GL_LIBRARY})
else(FOUND_CG)
	message(STATUS "Looking for Cg - not found")
endif(FOUND_CG)

SUBDIRS(src)
