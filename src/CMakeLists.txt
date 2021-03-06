
SET(SRCS
	effect.h
	effect.cpp
	glsleffect.cpp
	asmeffect.cpp
	cgfxeffect.cpp
	editor.h
	editor.cpp
	highlighter.h
	highlighter.cpp
	messagepanel.h
	messagepanel.cpp
	parameterpanel.h
	parameterpanel.cpp
	parameter.h
	parameter.cpp
	parametermodel.h
	parametermodel.cpp
	parameterdelegate.h
	parameterdelegate.cpp
	scenepanel.h
	scenepanel.cpp
	qglview.h
	qglview.cpp
	teapot.cpp
	newdialog.h
	newdialog.cpp
	texmanager.h
	texmanager.cpp
	outputparser.h
	outputparser.cpp
	scene.h
	scene.cpp
	finddialog.h
	finddialog.cpp
	gotodialog.h
	gotodialog.cpp
	glutils.h
	glutils.cpp
	imageplugin.h
	imageplugin.cpp
	cgexplicit.h
	cgexplicit.cpp
	document.h
	document.cpp)

SET(QT_SRCS ${SRCS}
	main.cpp
	qshaderedit.h
	qshaderedit.cpp)

SET(MOC_SRCS
	editor.h
	highlighter.h
	messagepanel.h
	parameterpanel.h
	parameterdelegate.h
	parametermodel.h
	scenepanel.h
	qglview.h
	newdialog.h
	finddialog.h
	gotodialog.h
	effect.h
	document.h)

SET(QT_MOC_SRCS qshaderedit.h)

SET(UIC_SRCS 
	newdialog.ui 
	parameterpropertiesdialog.ui
	texturepropertiesdialog.ui
	finddialog.ui
	gotodialog.ui)

SET(RCC_SRCS
	qshaderedit.qrc)


#CONFIGURE_FILE(${QShaderEdit_SOURCE_DIR}/config.h.in ${QShaderEdit_SOURCE_DIR}/config.h)

QT5_WRAP_UI(UICS ${UIC_SRCS})
QT5_ADD_RESOURCES(RCCS ${RCC_SRCS})
QT5_WRAP_CPP(MOCS ${MOC_SRCS})
QT5_WRAP_CPP(QT_MOCS ${QT_MOC_SRCS})

ADD_EXECUTABLE(qshaderedit MACOSX_BUNDLE ${QT_SRCS} ${MOCS} ${QT_MOCS} ${UICS} ${RCCS})
TARGET_LINK_LIBRARIES(qshaderedit ${LIBS})
INSTALL(TARGETS qshaderedit DESTINATION bin)

