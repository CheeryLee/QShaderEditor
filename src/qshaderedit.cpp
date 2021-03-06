/*
    QShaderEdit - Simple multiplatform shader editor
    Copyright (C) 2007 Ignacio Casta�o <castano@gmail.com>
    Copyright (C) 2007 Lars Uebernickel <larsuebernickel@gmx.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// Include GLEW before anything else.
#include <GL/glew.h>

#include "qshaderedit.h"
#include "effect.h"
#include "messagepanel.h"
#include "parameterpanel.h"
#include "scenepanel.h"
#include "editor.h"
#include "newdialog.h"
#include "scene.h"
#include "document.h"
#include "glutils.h"

#include <QFile>
#include <QTimer>
#include <QSettings>
#include <QUrl>
#include <QtDebug>
#include <QApplication>
#include <QMenu>
#include <QToolBar>
#include <QAction>
#include <QMenuBar>
#include <QTextEdit>
#include <QTabWidget>
#include <QStatusBar>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QMimeData>

namespace {
#ifdef Q_WS_MAC
	static const QString s_resourcePath = ":/images/mac";
#else
	static const QString s_resourcePath = ":/images/win";
#endif
}

// @@ Find a better name!
#define ORGANIZATION	"Castano Inc"


QShaderEdit::QShaderEdit(const QString& fileName) :
	m_document(NULL)
{
	setAcceptDrops(true);
	
	initGL();
	
	createDocument();
	
	createEditor();
	createActions();
	createToolbars();
	createDockWindows();
	createMenus();
	createStatusbar();
	
	loadSettings();
	
	show();

	// Make sure the main window is shown before the new file dialog.
	QApplication::processEvents();
	
	// Open document, or show new file dialog.
	if (fileName.isEmpty() || !m_document->loadFile(fileName))
	{
		m_document->reset(true);
	}
}

QShaderEdit::~QShaderEdit()
{
	delete m_glWidget;
	
	// @@ Not necessary, I believe.
	delete m_document;
}

QSize QShaderEdit::sizeHint() const
{
	return QSize(600, 400);
}



void QShaderEdit::about()
{
	// @@ Write a better about dialog.
	QMessageBox::about(this, tr("About QShaderEdit"), tr("<b>QShaderEdit</b> is a simple shader editor"));
}


void QShaderEdit::openRecentFile()
{
	QAction * action = qobject_cast<QAction *>(sender());
	if (action) {
		if (!m_document->close())
		{
			// User cancelled the operation.
			return;
		}
		
		QString fileName = action->data().toString();
		
		if (!m_document->loadFile(fileName))
		{
			QMessageBox::critical(this, tr("Error"), tr("Cannot load file:\n") + fileName, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton);
		}
	}
}

void QShaderEdit::clearRecentFiles()
{
	QSettings settings(ORGANIZATION, "QShaderEdit");
	settings.setValue("recentFileList", QStringList());

	/*foreach (QWidget *widget, QApplication::topLevelWidgets()) {
		MainWindow *mainWin = qobject_cast<MainWindow *>(widget);
		if (mainWin)
			mainWin->updateRecentFileActions();
	}*/
	
	// Only a single main window.
	updateRecentFileActions();
}

void QShaderEdit::onCursorPositionChanged()
{
	int line = m_editor->line();
	int column = m_editor->column();
	m_statusLabel->setText(QString(tr(" Line: %1  Col: %2")).arg(line).arg(column));
}

void QShaderEdit::onShaderTextChanged()
{
	// Compile after 1.5 seconds of inactivity.
	m_activityTimer->start(1500);
}

void QShaderEdit::onModifiedChanged(bool changed)
{
	if (sender() != m_document)
	{
		m_document->setModified(changed);
	}
	else if (sender() != m_editor)
	{
		m_editor->setModified(changed);
	}
	
	updateActions();
	updateWindowTitle(m_document->title());
}

void QShaderEdit::onActivityTimeout()
{
	Q_ASSERT(m_document->effect() != NULL);
	
	// Delay recompilation while editor is active or effect still compiling.
	if (m_parameterPanel->isEditorActive() || m_document->effect()->isBuilding())
	{
		m_activityTimer->start(1500);
		return;
	}
	
	// Stop the timer.
	m_activityTimer->stop();

	statusBar()->showMessage(tr("Compiling..."));

	// Compile the effect.
	m_document->build();
}


void QShaderEdit::onTitleChanged(QString title)
{
	updateWindowTitle(title);
}

void QShaderEdit::onFileNameChanged(QString fileName)
{
	addRecentFile(fileName);
}

void QShaderEdit::onEffectCreated()
{
	Effect * effect = m_document->effect();
	Q_ASSERT(effect != NULL);
	
	m_editor->setEffect(effect);
	
	// Connect effect signals.
	connect(effect, SIGNAL(infoMessage(QString)), m_messagePanel, SLOT(info(QString)));
	connect(effect, SIGNAL(errorMessage(QString)), m_messagePanel, SLOT(error(QString)));
	connect(effect, SIGNAL(buildMessage(QString,int,OutputParser*)), m_messagePanel, SLOT(log(QString,int,OutputParser*)));
	
	updateActions();
	updateTechniques();
	
	// Init scene view.
	m_scenePanel->setEffect(effect);
	
}

void QShaderEdit::onEffectDeleted()
{
	m_scenePanel->setEffect(NULL);
	m_parameterPanel->setEffect(NULL);
	m_editor->setEffect(NULL);
	
	updateTechniques();
	updateActions();
}

void QShaderEdit::onEffectBuilding()
{
	Effect * effect = m_document->effect();
	Q_ASSERT(effect != NULL);

	// @@ Why?
	//m_parameterPanel->clear();

	m_messagePanel->clear();
	
	// Stop animation while building.
	if (effect->isAnimated()) {
		m_scenePanel->stopAnimation();
	}
	m_scenePanel->setViewUpdatesEnabled(false);
}

void QShaderEdit::onEffectBuilt(bool succeed)
{
	Effect * effect = m_document->effect();
	Q_ASSERT(effect != NULL);
	
	if (succeed)
	{
		statusBar()->showMessage(tr("Compilation succeed."), 2000);
		m_messagePanel->info(tr("Compilation succeed.\n"));
	}
	else
	{
		statusBar()->showMessage(tr("Compilation failed."), 2000);
		m_messagePanel->setVisible(true);
		m_messagePanel->error(tr("Compilation failed.\n"));
	}
	
	updateTechniques();
	m_parameterPanel->setEffect(effect);
	
	// @@ Restart animation? 
	if (effect->isAnimated()) {
		m_scenePanel->startAnimation();
	}
	else {
		m_scenePanel->stopAnimation();
	}
	
	m_scenePanel->setViewUpdatesEnabled(true);
	m_scenePanel->refresh();
}

void QShaderEdit::onParameterChanged()
{
	m_scenePanel->refresh();
}

void QShaderEdit::onTechniqueChanged(int index)
{
	if (index >= 0)
	{
		Effect * effect = m_document->effect();
		Q_ASSERT(effect != NULL);
		
		effect->selectTechnique(index);
		
		m_scenePanel->refresh();
	}
}

void QShaderEdit::updateEffectInputs()
{
	Effect * effect = m_document->effect();
	Q_ASSERT(effect);
	
	const int inputNum = effect->getInputNum();
	for (int i = 0; i < inputNum; i++)
	{
		const QTextEdit * textEdit = qobject_cast<const QTextEdit *>(m_editor->widget(i));
		
		if (textEdit != NULL)
		{
			effect->setInput(i, textEdit->toPlainText().toLatin1());
		}
	}
}


void QShaderEdit::initGL()
{
	if (QGLFormat::hasOpenGL())
	{
		QGLFormat format;
		//format.setRgba(true);
		//format.setAlpha(false);
		format.setDepth(true);
		//format.setStencil(false);	// not for now...
		format.setDoubleBuffer(true);
		
		m_glWidget = new QGLWidget(format, this);
		m_glWidget->setVisible(false);
		m_glWidget->makeCurrent();
		
		GLenum err = glewInit();
		if (GLEW_OK != err)
		{
			// Problem: glewInit failed, something is seriously wrong.
			qDebug("Error: %s\n", glewGetErrorString(err));
			
			delete m_glWidget;
			m_glWidget = NULL;
		}
		
		if (m_glWidget) m_glWidget->doneCurrent();
	}
}

void QShaderEdit::createDocument()
{
	m_document = new Document(m_glWidget, this);
	connect(m_document, SIGNAL(modifiedChanged(bool)), this, SLOT(onModifiedChanged(bool)));
	connect(m_document, SIGNAL(titleChanged(QString)), this, SLOT(onTitleChanged(QString)));
	connect(m_document, SIGNAL(fileNameChanged(QString)), this, SLOT(onFileNameChanged(QString)));
	connect(m_document, SIGNAL(effectCreated()), this, SLOT(onEffectCreated()));
	connect(m_document, SIGNAL(effectDeleted()), this, SLOT(onEffectDeleted()));
	connect(m_document, SIGNAL(effectBuilding()), this, SLOT(onEffectBuilding()));
	connect(m_document, SIGNAL(effectBuilt(bool)), this, SLOT(onEffectBuilt(bool)));
	connect(m_document, SIGNAL(synchronizeEditors()), this, SLOT(updateEffectInputs()));
	
	m_activityTimer = new QTimer(this);
	connect(m_activityTimer, SIGNAL(timeout()), this, SLOT(onActivityTimeout()));
}

void QShaderEdit::createEditor()
{
	// Create editor.
	m_editor = new Editor(this);
	setCentralWidget(m_editor);
	m_editor->setFocus();
	connect(m_editor, SIGNAL(cursorPositionChanged()), this, SLOT(onCursorPositionChanged()));
	connect(m_editor, SIGNAL(textChanged()), this, SLOT(onShaderTextChanged()));
	connect(m_editor, SIGNAL(modifiedChanged(bool)), this, SLOT(onModifiedChanged(bool)));

	
	// Create editor actions.
	m_findAction = new QAction(QIcon(s_resourcePath + "/find.png"), tr("&Find"), this);
	m_findAction->setEnabled(false);
	m_findAction->setShortcut(tr("Ctrl+F"));
	connect(m_findAction, SIGNAL(triggered()), m_editor, SLOT(findDialog()));
	
	m_findNextAction = new QAction(tr("&Find Next"), this);
	m_findNextAction->setEnabled(false);
#ifdef Q_WS_MAC
	m_findNextAction->setShortcut(tr("Ctrl+G"));
#else
	m_findNextAction->setShortcut(tr("F3"));
#endif
	connect(m_findNextAction, SIGNAL(triggered()), m_editor, SLOT(findNext()));
	
	m_findPreviousAction = new QAction(tr("&Find Previous"), this);
	m_findPreviousAction->setEnabled(false);
#ifdef Q_WS_MAC
	m_findPreviousAction->setShortcut(tr("Shift+Ctrl+G"));
#else
	m_findPreviousAction->setShortcut(tr("Shift+F3"));
#endif
	connect(m_findPreviousAction, SIGNAL(triggered()), m_editor, SLOT(findPrevious()));
	
	m_gotoAction = new QAction(tr("&Goto"), this);
	m_gotoAction->setEnabled(false);
#ifdef Q_WS_MAC
	m_gotoAction->setShortcut(tr("Ctrl+L"));
#else
	m_gotoAction->setShortcut(tr("Ctrl+G"));
#endif
	connect(m_gotoAction, SIGNAL(triggered()), m_editor, SLOT(gotoDialog()));

	
	// Add hidden actions.
	QAction * action = NULL;

	action = new QAction(tr("&Next Tab"), this);
	action->setShortcut(tr("Alt+Right"));
	connect(action, SIGNAL(triggered()), m_editor, SLOT(nextTab()));
	this->addAction(action);
	
	action = new QAction(tr("&Previous Tab"), this);
	action->setShortcut(tr("Alt+Left"));
	connect(action, SIGNAL(triggered()), m_editor, SLOT(previousTab()));
	this->addAction(action);
}

void QShaderEdit::createDockWindows()
{
	m_messagePanel = new MessagePanel(tr("Messages"), this);
	m_messagePanel->setObjectName("MessagesDock");
	m_messagePanel->setAllowedAreas(Qt::BottomDockWidgetArea);
	m_messagePanel->setVisible(false);
	addDockWidget(Qt::BottomDockWidgetArea, m_messagePanel);
	connect(m_messagePanel, SIGNAL(messageClicked(int, int, int)), m_editor, SLOT(gotoLine(int, int, int)));

	m_scenePanel = new ScenePanel(tr("Scene"), this, m_glWidget);
	m_scenePanel->setObjectName("SceneDock");
	m_scenePanel->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, m_scenePanel);
	
	m_parameterPanel = new ParameterPanel(tr("Parameters"), this);
	m_parameterPanel->setObjectName("ParameterDock");
	m_parameterPanel->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, m_parameterPanel);
	connect(m_parameterPanel, SIGNAL(parameterChanged()), m_document, SLOT(onParameterChanged()));
	connect(m_parameterPanel, SIGNAL(parameterChanged()), this, SLOT(onParameterChanged()));
}


void QShaderEdit::createActions()
{
        m_newAction = new QAction(QIcon(s_resourcePath + "/filenew.png"), tr("&New Shader"), this);
	m_newAction->setShortcut(tr("Ctrl+N"));
        m_newAction->setStatusTip(tr("Create a new shader"));
	connect(m_newAction, SIGNAL(triggered()), m_document, SLOT(reset()));

        m_openAction = new QAction(QIcon(s_resourcePath + "/fileopen.png"), tr("&Open Shader"), this);
	m_openAction->setShortcut(tr("Ctrl+O"));
        m_openAction->setStatusTip(tr("Open an existing shader"));
	connect(m_openAction, SIGNAL(triggered()), m_document, SLOT(open()));

        m_saveAction = new QAction(QIcon(s_resourcePath + "/filesave.png"), tr("&Save Shader"), this);
	m_saveAction->setEnabled(false);
	m_saveAction->setShortcut(tr("Ctrl+S"));
        m_saveAction->setStatusTip(tr("Save this shader"));
	connect(m_saveAction, SIGNAL(triggered()), m_document, SLOT(save()));

	m_saveAsAction = new QAction(tr("Save &As..."), this);
	m_saveAsAction->setStatusTip(tr("Save this effect under a new name"));
	m_saveAsAction->setEnabled(false);
	connect(m_saveAsAction, SIGNAL(triggered()), m_document, SLOT(saveAs()));

	for (int i = 0; i < MaxRecentFiles; ++i)
	{
		m_recentFileActions[i] = new QAction(this);
		m_recentFileActions[i]->setVisible(false);
		connect(m_recentFileActions[i], SIGNAL(triggered()), this, SLOT(openRecentFile()));
	}
	
	m_clearRecentAction = new QAction(tr("&Clear Recent"), this);
	m_clearRecentAction->setEnabled(false);
	connect(m_clearRecentAction, SIGNAL(triggered()), this, SLOT(clearRecentFiles()));
}

void QShaderEdit::createMenus()
{
	QAction * action = NULL;

	QMenu * fileMenu = menuBar()->addMenu(tr("&File"));

	fileMenu->addAction(m_newAction);
	fileMenu->addAction(m_openAction);
	
        QMenu * recentFileMenu = fileMenu->addMenu(tr("Recent Shaders"));
	for(int i = 0; i < MaxRecentFiles; i++) {
		recentFileMenu->addAction(m_recentFileActions[i]);
	}
	m_recentFileSeparator = recentFileMenu->addSeparator();
	recentFileMenu->addAction(m_clearRecentAction);
	updateRecentFileActions();

        fileMenu->addSeparator();

	fileMenu->addAction(m_saveAction);
	fileMenu->addAction(m_saveAsAction);

	fileMenu->addSeparator();
	
	action = new QAction(tr("E&xit"), this);
	action->setShortcut(tr("Ctrl+Q"));
	action->setStatusTip(tr("Exit the application"));
	connect(action, SIGNAL(triggered()), this, SLOT(close()));
	fileMenu->addAction(action);

	QMenu * editMenu = menuBar()->addMenu(tr("&Edit"));

	action = new QAction(QIcon(s_resourcePath + "/editundo.png"), tr("&Undo"), this);
	action->setShortcut(tr("Ctrl+Z"));
	action->setEnabled(false);
	connect(action, SIGNAL(triggered()), m_editor, SLOT(undo()));
	connect(m_editor, SIGNAL(undoAvailable(bool)), action, SLOT(setEnabled(bool)));
	editMenu->addAction(action);

	action = new QAction(QIcon(s_resourcePath + "/editredo.png"), tr("&Redo"), this);
	action->setShortcut(tr("Ctrl+Shift+Z"));
	action->setEnabled(false);
	connect(action, SIGNAL(triggered()), m_editor, SLOT(redo()));
	connect(m_editor, SIGNAL(redoAvailable(bool)), action, SLOT(setEnabled(bool)));
	editMenu->addAction(action);

	editMenu->addSeparator();

	action = new QAction(QIcon(s_resourcePath + "/editcut.png"), tr("C&ut"), this);
	action->setShortcut(tr("Ctrl+X"));
	action->setEnabled(false);
	connect(action, SIGNAL(triggered()), m_editor, SLOT(cut()));
	connect(m_editor, SIGNAL(copyAvailable(bool)), action, SLOT(setEnabled(bool)));
	editMenu->addAction(action);

	action = new QAction(QIcon(s_resourcePath + "/editcopy.png"), tr("&Copy"), this);
	action->setShortcut(tr("Ctrl+C"));
	action->setEnabled(false);
	connect(action, SIGNAL(triggered()), m_editor, SLOT(copy()));
	connect(m_editor, SIGNAL(copyAvailable(bool)), action, SLOT(setEnabled(bool)));
	editMenu->addAction(action);

	action = new QAction(QIcon(s_resourcePath + "/editpaste.png"), tr("&Paste"), this);
	action->setShortcut(tr("Ctrl+V"));
	action->setEnabled(false);
	connect(action, SIGNAL(triggered()), m_editor, SLOT(paste()));
	connect(m_editor, SIGNAL(pasteAvailable(bool)), action, SLOT(setEnabled(bool)));
	editMenu->addAction(action);

	editMenu->addSeparator();
	
	editMenu->addAction(m_findAction);
	editMenu->addAction(m_findNextAction);
	editMenu->addAction(m_findPreviousAction);
	editMenu->addAction(m_gotoAction);
	
	QMenu * viewMenu = menuBar()->addMenu(tr("&View"));

	viewMenu->addAction(m_scenePanel->toggleViewAction());
	viewMenu->addAction(m_parameterPanel->toggleViewAction());
	viewMenu->addAction(m_messagePanel->toggleViewAction());
	
	viewMenu->addSeparator();
	
	viewMenu->addAction(m_fileToolBar->toggleViewAction());
	viewMenu->addAction(m_techniqueToolBar->toggleViewAction());
	
	// On KDE this should be under the "Preferences" menu.
	//QMenu * toolbarMenu = viewMenu->addMenu(tr("&Toolbars"));
	//toolbarMenu->addAction(m_fileToolBar->toggleViewAction());
	//toolbarMenu->addAction(m_techniqueToolBar->toggleViewAction());
	
	Q_ASSERT( m_scenePanel != NULL );
	menuBar()->addMenu(m_scenePanel->menu());
	
	
	QMenu * helpMenu = menuBar()->addMenu(tr("&Help"));

	action = new QAction(tr("&About"), this);
	action->setStatusTip(tr("Show the application's about box"));
	connect(action, SIGNAL(triggered()), this, SLOT(about()));
	helpMenu->addAction(action);

	action = new QAction(tr("About &Qt"), this);
	action->setStatusTip(tr("Show the Qt library's about box"));
	connect(action, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
	helpMenu->addAction(action);
	
	
}

void QShaderEdit::createToolbars()
{
	m_fileToolBar = new QToolBar(tr("File Toolbar"), this);
	m_fileToolBar->setObjectName("FileToolBar");
	this->addToolBar(m_fileToolBar);

	m_fileToolBar->addAction(m_newAction);
	m_fileToolBar->addAction(m_openAction);
	m_fileToolBar->addAction(m_saveAction);
	
	m_techniqueToolBar = new QToolBar(tr("Technique Toolbar"), this);
	m_techniqueToolBar->setObjectName("TechniqueToolBar");
	this->addToolBar(m_techniqueToolBar);

	m_techniqueCombo = new QComboBox();
	m_techniqueCombo->setEditable(false);
	m_techniqueCombo->setInsertPolicy(QComboBox::InsertAtBottom);
	m_techniqueCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	m_techniqueCombo->setEnabled(false);
	connect(m_techniqueCombo, SIGNAL(activated(int)), this, SLOT(onTechniqueChanged(int)));

	QLabel * techniqueLabel = new QLabel(tr("Technique: "), m_techniqueToolBar);
	techniqueLabel->setBuddy(m_techniqueCombo);
	m_techniqueToolBar->addWidget(techniqueLabel);
	m_techniqueToolBar->addWidget(m_techniqueCombo);

	setUnifiedTitleAndToolBarOnMac(true);
}

void QShaderEdit::createStatusbar()
{
	statusBar()->showMessage(tr("Ready"));
	m_statusLabel = new QLabel(statusBar());
	m_statusLabel->setText(tr(" Line: 0  Col: 0"));
	statusBar()->addPermanentWidget(m_statusLabel);
}

void QShaderEdit::updateActions()
{
	m_saveAction->setEnabled(m_document->isModified() || m_document->file() == NULL);
	m_saveAsAction->setEnabled(true);
	
	m_findAction->setEnabled(true);
	m_findNextAction->setEnabled(true);
	m_findPreviousAction->setEnabled(true);
	m_gotoAction->setEnabled(true);

	/*QString fileName;
	fileName = m_document->fileName();
	m_saveAction->setText(tr("Save %1").arg(fileName));*/
}

void QShaderEdit::updateTechniques()
{
	QString lastText = m_techniqueCombo->currentText();
	
	Effect * effect = m_document->effect();
	
	int count = 0;
	if (effect != NULL)
	{
		count = effect->getTechniqueNum();
	}
	
	if (count == 0)
	{
		m_techniqueCombo->setEnabled(false);
	}
	else
	{
		m_techniqueCombo->setEnabled(true);
		m_techniqueCombo->clear();
		
		for(int i = 0; i < count; i++) {
			m_techniqueCombo->addItem(effect->getTechniqueName(i));
		}
		m_techniqueCombo->setEnabled(count > 1);
		
		int idx = m_techniqueCombo->findText(lastText);
		if( idx != -1 ) {
			m_techniqueCombo->setCurrentIndex(idx);
			effect->selectTechnique(idx);
		}
	}
}

void QShaderEdit::updateWindowTitle(QString title)
{
	title.append(" - ");

	title.append("QShaderEditor"); //QApplication::instance()->applicationName();

	this->setWindowTitle(title);
}

void QShaderEdit::updateRecentFileActions()
{
	QSettings settings(ORGANIZATION, "QShaderEdit");
	QStringList files = settings.value("recentFileList").toStringList();

	int numRecentFiles = qMin(files.count(), (int)MaxRecentFiles);

	for (int i = 0; i < numRecentFiles; ++i)
	{
		QString text = tr("&%1 %2").arg(i + 1).arg(QFileInfo(files[i]).fileName());
		m_recentFileActions[i]->setText(text);
		m_recentFileActions[i]->setData(files[i]);
		m_recentFileActions[i]->setVisible(true);
		
		// Disable entry if file type not supported...
		int idx = files[i].lastIndexOf('.');
		QString fileExtension = files[i].mid(idx+1);
		
		const EffectFactory * factory = EffectFactory::factoryForExtension(fileExtension);
		if (factory == NULL || !factory->isSupported())
		{
			m_recentFileActions[i]->setEnabled(false);
		}
	}

	for (int i = numRecentFiles; i < MaxRecentFiles; ++i)
	{
		m_recentFileActions[i]->setVisible(false);
	}
	
	m_recentFileSeparator->setVisible(numRecentFiles > 0);
	m_clearRecentAction->setEnabled(numRecentFiles > 0);
}

void QShaderEdit::addRecentFile(QString fileName)
{
 	QSettings settings(ORGANIZATION, "QShaderEdit");
	QStringList files = settings.value("recentFileList").toStringList();
	files.removeAll(fileName);
	files.prepend(fileName);
	while (files.size() > MaxRecentFiles)
	{
		files.removeLast();
	}

	settings.setValue("recentFileList", files);

	/*foreach (QWidget *widget, QApplication::topLevelWidgets()) {
		MainWindow *mainWin = qobject_cast<MainWindow *>(widget);
		if (mainWin)
			mainWin->updateRecentFileActions();
	}*/
	
	// Only a single main window.
	updateRecentFileActions();
}


/*virtual*/ void QShaderEdit::closeEvent(QCloseEvent * event)
{
	Q_ASSERT(event != NULL);
	
	if (m_document->close())
	{
		event->accept();
		saveSettings();
	}
	else
	{
		event->ignore();
	}
}

/*virtual*/ void QShaderEdit::keyPressEvent(QKeyEvent * event)
{
	if (event->key() == Qt::Key_Escape)
	{
		m_messagePanel->close();
		if (m_editor->currentWidget())
		{
			m_editor->currentWidget()->setFocus();
		}
		event->accept();
	}
	else
	{
		event->ignore();
	}
}

/*virtual*/ void QShaderEdit::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/uri-list"))
    {
	    QList<QUrl> urls = event->mimeData()->urls();
		if (urls.size())
		{
			QString fileName = urls[0].toLocalFile();
	    	
			if (m_document->canLoadFile(fileName))
			{
				event->acceptProposedAction();
			}
		}   
    }
}

/*virtual*/ void QShaderEdit::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
	
	if (urls.size())
	{
		QString fileName = urls[0].toLocalFile();
		
		Q_ASSERT(m_document != NULL);
		if (m_document->canLoadFile(fileName))
		{
			event->acceptProposedAction();
			
			m_document->loadFile(urls[0].toLocalFile());
		}
	}
}


void QShaderEdit::loadSettings()
{
	QSettings pref( ORGANIZATION, "QShaderEdit" );

	pref.beginGroup("MainWindow");

	resize(pref.value("size", QSize(640,480)).toSize());
	bool maximize = pref.value("maximized", false).toBool();

	QByteArray state = pref.value("state").toByteArray();
	if (!state.isEmpty()) {
		restoreState(state);
	}

	pref.endGroup();

	//	m_autoCompile = pref.value("autoCompile", true).toBool();
	//m_lastEffect = pref.value("lastEffect", ".").toString();
	Document::setLastEffect(pref.value("lastEffect", ".").toString());
	SceneFactory::setLastFile(pref.value("lastScene", ".").toString());
	ParameterPanel::setLastPath(pref.value("lastParameterPath", ".").toString());

	if (maximize) {
		setWindowState(windowState() | Qt::WindowMaximized);
	}
}

void QShaderEdit::saveSettings()
{
	QSettings pref( ORGANIZATION, "QShaderEdit" );

	pref.beginGroup("MainWindow");
	pref.setValue("maximized", isMaximized());
#if defined(Q_WS_WIN)
	pref.setValue("size", normalGeometry().size());
#else
	pref.setValue("size", size());
#endif
	pref.setValue("state", saveState());
	pref.endGroup();

	//	pref.setValue("autoCompile", m_autoCompile);
	//pref.setValue("lastEffect", m_lastEffect);
	pref.setValue("lastEffect", Document::lastEffect());
	pref.setValue("lastScene", SceneFactory::lastFile());
	pref.setValue("lastParameterPath", ParameterPanel::lastPath());
}

