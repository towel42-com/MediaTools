// The MIT License( MIT )
//
// Copyright( c ) 2020 Scott Aron Bloom
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sub-license, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "MainWindow.h"
#include "DirModel.h"
#include "SABUtils/utils.h"
#include "SABUtils/ScrollMessageBox.h"
#include "SABUtils/ButtonEnabler.h"
#include "ui_MainWindow.h"

#include <QSettings>
#include <QFileInfo>
#include <QFileDialog>
#include <QCompleter>
#include <QMessageBox>
#include <QDateTime>
#include <QDate>
#include <QDesktopServices> 
#include <QTimer>
#include <QProgressDialog>
#include <QScrollBar>
#include <QDebug>

CMainWindow::CMainWindow(QWidget* parent)
    : QMainWindow(parent),
    fImpl(new Ui::CMainWindow)
{
    fImpl->setupUi(this);

    loadSettings();

    connect(fImpl->dir, &NSABUtils::CDelayLineEdit::sigTextChangedAfterDelay, this, &CMainWindow::slotDirectoryChanged);
    connect(fImpl->btnSelectDir, &QPushButton::clicked, this, &CMainWindow::slotSelectDirectory);
    connect(fImpl->btnTransform, &QPushButton::clicked, this, &CMainWindow::slotTransform);

    auto completer = new QCompleter(this);
    auto fsModel = new QFileSystemModel(completer);
    fsModel->setRootPath("");
    completer->setModel(fsModel);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);

    fImpl->dir->setCompleter(completer);

    QTimer::singleShot(0, this, &CMainWindow::slotDirectoryChanged);

    connect( fImpl->menubar, &NSABUtils::CMenuBarEx::sigAboutToEngage, []() 
             { 
                 qDebug() << QDateTime::currentDateTime().toString() << "Menubar engaged"; 
             } );
    connect( fImpl->menubar, &NSABUtils::CMenuBarEx::sigFinishedEngagement, []()
             { 
                 qDebug() << QDateTime::currentDateTime().toString() << "Menubar disengaged"; 
             } );
}

CMainWindow::~CMainWindow()
{
    saveSettings();
}

void CMainWindow::loadSettings()
{
    QSettings settings;

    fImpl->dir->setText( settings.value( "Directory", QString() ).toString() );
}

void CMainWindow::saveSettings()
{
    QSettings settings;

    settings.setValue("Directory", fImpl->dir->text());
}

void CMainWindow::slotDirectoryChanged()
{
    QFileInfo dir(fImpl->dir->text());
    bool aOK = !fImpl->dir->text().isEmpty() && dir.exists() && dir.isDir();
    if ( aOK )
        QTimer::singleShot(0, this, &CMainWindow::slotLoad);
}

void CMainWindow::slotSelectDirectory()
{
    auto dir = QFileDialog::getExistingDirectory(this, tr("Select Directory:"), fImpl->dir->text());
    if (!dir.isEmpty())
        fImpl->dir->setText( dir );
}

void CMainWindow::slotLoad()
{
    loadDirectory();
}

void CMainWindow::loadDirectory()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    fIDMap.clear();
    fDirMap.clear();
    fImpl->directories->clear();
    fImpl->directories->setHeaderLabels(QStringList() << "ID" << "Name");
    auto header = fImpl->directories->header();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);

    QProgressDialog dlg(tr("Computing Number of Directories..."), "Cancel", 0, 0, this);
    dlg.setMinimumDuration(0);
    dlg.setValue(1);

    //int numDirs = getNumDirs( fImpl->dir->text(), &dlg );
    //if (dlg.wasCanceled())
    //    return;
    dlg.setLabelText(tr("Finding Directories..."));
    dlg.setRange(0, 0);
    dlg.setValue(0);

    QDirIterator ii(fImpl->dir->text(), QStringList() << "*.mkv", QDir::Filter::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Files, QDirIterator::IteratorFlag::Subdirectories);

    auto relToDir = QDir(fImpl->dir->text());

    int cnt = 0;
    while (ii.hasNext())
    {
        if (dlg.wasCanceled())
            break;
        ii.next();
        if ( skipDir(ii.fileName() ) )
            continue;
        cnt++;
        dlg.setValue(cnt);
        qApp->processEvents();

        auto info = ii.fileInfo();

        QRegularExpression regExp("(?<name>.*)\\s\\(.*\\[(tmdbid|imdbid)\\=\\s*(?<id>.*)\\s*\\]");
        auto leafName = info.fileName();
        auto match = regExp.match(leafName);

        auto relPath = relToDir.relativeFilePath( ii.filePath() );

        if ( info.isDir() )
        {
            qDebug() << ii.filePath();
            QTreeWidgetItem *idItem = nullptr;
            if ( match.hasMatch() )
            {
                auto id = match.captured( "id" );
                auto name = match.captured( "name" );
                auto pos = fIDMap.find( id );
                if ( pos == fIDMap.end() )
                {
                    idItem = new QTreeWidgetItem( fImpl->directories, QStringList() << id << name, ENodeType::eID );
                    idItem->setExpanded( true );
                    fIDMap[id] = idItem;
                }
                else
                    idItem = ( *pos ).second;
            }
            else
            {
                continue;
            }

            auto dirItem = new QTreeWidgetItem( idItem, QStringList() << QString() << relPath, ENodeType::eDir );
            fDirMap[ii.filePath()] = dirItem;
            dirItem->setExpanded( true );
            idItem->setHidden( idItem->childCount() < 2 );
        }
        else
        {
            auto dirName = info.absolutePath();
            auto pos = fDirMap.find( dirName );
            //Q_ASSERT( pos != fDirMap.end() );
            if ( pos == fDirMap.end() )
                continue;

            auto fileItem = new QTreeWidgetItem( (*pos).second, QStringList() << QString() << relPath, ENodeType::eFile );
        }
    }

    for( auto ii = 0; ii < fImpl->directories->topLevelItemCount(); ++ii )
    {
        auto item = fImpl->directories->topLevelItem( ii );
        if ( item->isHidden() )
            continue;
        validateFiles( item );
    }
    QApplication::restoreOverrideCursor();
    qApp->processEvents();
}

void CMainWindow::validateFiles( QTreeWidgetItem * idItem ) 
{
    if ( !idItem )
        return;
    for( auto ii = 0; ii < idItem->childCount(); ++ii )
    {
        auto dirItem = idItem->child( ii );

        auto dirLeafName = QFileInfo( dirItem->text( 1 ) ).fileName();

        QRegularExpression regExp( "(?<name>.*)\\s\\(.*\\)\\s*-\\s*(?<extraInfo>.*)\\s*\\[(tmdbid|imdbid)\\=\\s*(?<id>.*)\\s*\\]" );
        auto match = regExp.match( dirLeafName );
        bool outOfOrder = false;
        if ( !match.hasMatch() )
        {
            QRegularExpression regExp( "(?<name>.*)\\s\\(.*\\)\\s*\\[(tmdbid|imdbid)\\=\\s*(?<id>.*)\\s*\\]\\s*-\\s*(?<extraInfo>.*)" );
            match = regExp.match( dirLeafName );
            if ( !match.hasMatch() )
                continue; // happens when its the base version
            outOfOrder = true;
        }
        Q_ASSERT( dirItem->childCount() == 1 );
        auto baseName = match.captured( "name" ).trimmed();
        auto extraInfo = match.captured( "extraInfo" ).trimmed();

        auto correctFileName2 = QString( "%1 - %2" ).arg( baseName ).arg( extraInfo );
        auto correctFileName1 = QString( "%1-%2" ).arg( baseName ).arg( extraInfo );

        auto fileItem = dirItem->child( 0 );
        auto filePath = fileItem->text( 1 );
        auto fileName = QFileInfo( filePath ).baseName();
        if ( outOfOrder || ( fileName != correctFileName1 ) && ( fileName != correctFileName2 ) )
        {
            delete fileItem;
            fileItem = new QTreeWidgetItem( dirItem, QStringList() << QString() << filePath, ENodeType::eBadFileName );
            fileItem->setBackground( 1, Qt::red );
        }
    }
}
bool CMainWindow::hasChildDirs(const QFileInfo& info) const
{
    QDirIterator jj(info.absoluteFilePath(), QStringList() << "*.*" << "*", QDir::Filter::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    return jj.hasNext();
}

bool CMainWindow::skipDir(const QString& path) const
{
    if (path.contains("Featurettes"))
        return true;
    if (path.contains("SRT"))
        return true;
    if (path.contains("Artwork"))
        return true;
    if (path.contains("Extras"))
        return true;
    if (path.contains("eaDir"))
        return true;
    if (path.contains("Subs"))
        return true;
    if (path.contains("subs"))
        return true;
    return false;
}

QTreeWidgetItem * CMainWindow::getItem( const QString & path ) const
{
    auto pos = fIDMap.find(path);
    Q_ASSERT(pos != fIDMap.end());
    if (pos == fIDMap.end())
        return nullptr;

    return (*pos).second;
}

QTreeWidgetItem* CMainWindow::getParent(const QFileInfo & fileInfo ) const
{
    auto path = fileInfo.absolutePath();
    auto relToDir = QDir(fImpl->dir->text());
    path = QDir(fImpl->dir->text()).relativeFilePath(path);
    if (path.isEmpty())
        path = ".";
    return getItem(path);
}

int CMainWindow::getNumDirs(const QString& dir, QProgressDialog * dlg ) const
{
    qApp->processEvents();
    QDirIterator ii(dir, QStringList() << "*.*" << "*", QDir::Filter::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::IteratorFlag::Subdirectories);

    int cnt = 0;
    while (ii.hasNext())
    {
        ii.next();
        if (dlg->wasCanceled())
            break;

        if (skipDir(ii.fileName()))
            continue;

        if ( cnt % 100 == 0 )
            qApp->processEvents();
        cnt++;
    }
    return cnt;
}

void CMainWindow::slotTransform()
{
    QProgressDialog dlg(tr("Computing Number of Directories to Rename..."), "Cancel", 0, 0, this);
    dlg.setMinimumDuration(0);
    dlg.setValue(1);

    int numDirs = getNumDirsToRename( &dlg );
    if (dlg.wasCanceled())
        return;
    dlg.setLabelText(tr("Renaming Directories..."));
    dlg.setRange(0, numDirs);
    dlg.setValue(0);

    for (auto&& ii = 0; ii < fImpl->directories->topLevelItemCount(); ++ii)
    {
        qApp->processEvents();
        transform(fImpl->directories->topLevelItem(ii), ii, &dlg);
    }
}

int CMainWindow::getNumDirsToRename( QProgressDialog * dlg, QTreeWidgetItem * parent ) const
{
    if (dlg->wasCanceled())
        return 0;

    int retVal = (parent && ( parent->type() == ENodeType::eBadFileName ) ) ? 1 : 0;

    for (int ii = 0; ii < (parent ? parent->childCount() : fImpl->directories->topLevelItemCount() ); ++ii )
    {
        if (dlg->wasCanceled())
            break;

        auto child = parent ? parent->child( ii ) : fImpl->directories->topLevelItem( ii );
        if (!child)
            continue;
        retVal += getNumDirsToRename(dlg, child);
    }

    return retVal;
}

void CMainWindow::transform( QTreeWidgetItem * fileItem, int /*pos*/, QProgressDialog * dlg)
{
    if (dlg->wasCanceled())
        return;

    if (!fileItem)
        return;
    if ( fileItem->type() == ENodeType::eBadFileName )
    {
        auto dirItem = fileItem->parent();

        auto dirLeafName = QFileInfo( dirItem->text( 1 ) ).fileName();

        QRegularExpression regExp( "(?<name>.*)\\s\\(.*\\)\\s*-\\s*(?<extraInfo>.*)\\s*\\[(tmdbid|imdbid)\\=\\s*(?<id>.*)\\s*\\]" );
        auto match = regExp.match( dirLeafName );
        if ( !match.hasMatch() ) // happens when its the base version shouldnt happen here since the file would be ok...
        {
            QRegularExpression regExp( "(?<name>.*)\\s\\(.*\\)\\s*\\[(tmdbid|imdbid)\\=\\s*(?<id>.*)\\s*\\]\\s*-\\s*(?<extraInfo>.*)" );
            match = regExp.match( dirLeafName );
        }

        if ( match.hasMatch() ) // happens when its the base version shouldnt happen here since the file would be ok...
        {
            auto baseName = match.captured( "name" ).trimmed();
            auto extraInfo = match.captured( "extraInfo" ).trimmed();

            auto correctFileName = QString( "%1 - %2" ).arg( baseName ).arg( extraInfo );

            auto filePath = fileItem->text( 1 );
            auto absPath = QDir( fImpl->dir->text() ).absoluteFilePath( filePath );
            auto fileInfo = QFileInfo( absPath );
            Q_ASSERT( fileInfo.exists() );

            auto dir = fileInfo.absoluteDir();
            auto ext = fileInfo.suffix();
            auto newFile = dir.absoluteFilePath( correctFileName + "." + ext );

            if ( !QFile::rename( absPath, newFile ) )
            {
                QMessageBox::critical( this, "could not rename", QString( "Could not rename '%1' to '%2" ).arg( absPath, newFile ) );
            }
        }
        dlg->setValue(dlg->value() + 1);
        qApp->processEvents();
    }

    for( int ii = 0; ii < fileItem->childCount(); ++ii )
    {
        if (dlg->wasCanceled())
            return;
        transform(fileItem->child(ii), ii, dlg);
    }
}




