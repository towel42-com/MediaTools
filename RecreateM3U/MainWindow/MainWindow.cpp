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
}

CMainWindow::~CMainWindow()
{
    saveSettings();
}

void CMainWindow::loadSettings()
{
    QSettings settings;

    fImpl->dir->setText(settings.value("Directory", QString()).toString());
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

    fItemMap.clear();
    fImpl->directories->clear();
    fImpl->directories->setHeaderLabels( QStringList() << "Name" );

    if ( aOK )
        QTimer::singleShot(0, this, &CMainWindow::slotLoad);
}

void CMainWindow::slotSelectDirectory()
{
    auto dir = QFileDialog::getExistingDirectory(this, tr("Select Directory:"), fImpl->dir->text());
    if (!dir.isEmpty())
        fImpl->dir->setText(dir);
}

void CMainWindow::slotLoad()
{
    loadDirectory();
}

void CMainWindow::loadDirectory()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    auto header = fImpl->directories->header();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);

    QProgressDialog dlg(tr("Computing Number of Directories..."), "Cancel", 0, 0, this);
    dlg.setMinimumDuration(0);
    dlg.setValue(1);

    int numDirs = getNumDirs( fImpl->dir->text(), &dlg );
    if (dlg.wasCanceled())
        return;
    dlg.setLabelText(tr("Finding Directories..."));
    dlg.setRange(0, numDirs);
    dlg.setValue(0);

    auto rootDir = new QTreeWidgetItem(fImpl->directories, QStringList() << ".", eParentDir);
    rootDir->setExpanded(true);
    fItemMap["."] = rootDir;

    QDirIterator ii(fImpl->dir->text(), QStringList() << "*.m3u", QDir::Filter::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::IteratorFlag::Subdirectories);

    auto relToDir = this->relToDir();

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
        auto relPath = relToDir.relativeFilePath( ii.filePath() );
        auto parent = getParent( info );
        Q_ASSERT( parent );

        if ( info.isDir() )
        {
            auto item = new QTreeWidgetItem( parent, QStringList() << relPath, eParentDir );
            fItemMap[relPath] = item;
        }
        else
        {
            loadM3UItem( info, parent, &dlg );
        }
    }

    QApplication::restoreOverrideCursor();
    qApp->processEvents();
}

QDir CMainWindow::relToDir() const
{
    return QDir( fImpl->dir->text() );
}

void CMainWindow::loadM3UItem( const QFileInfo & info, QTreeWidgetItem* parent, QProgressDialog* dlg )
{
    auto relPath = relToDir().relativeFilePath( info.absoluteFilePath() );
    auto m3uItem = new QTreeWidgetItem( parent, QStringList() << relPath, eM3U );
    fItemMap[relPath] = m3uItem;

    QDirIterator ii( info.absolutePath(), QStringList() << "*.mkv", QDir::Filter::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::IteratorFlag::Subdirectories );
    dlg->setValue( dlg->value() + 1 );

    while( ii.hasNext() )
    {
        qApp->processEvents();
        if ( dlg->wasCanceled() )
            return;

        ii.next();

        relPath = relToDir().relativeFilePath( ii.filePath() );
        auto parent = getParent( ii.fileInfo() );
        if ( ii.fileInfo().isDir() )
        {
            auto item = new QTreeWidgetItem( parent, QStringList() << relPath, eParentDir );
            fItemMap[relPath] = item;
        }
        else // mkv file
        {
            auto mkvItem = new QTreeWidgetItem( parent, QStringList() << relPath, eMKV );
            fItemMap[relPath] = mkvItem;
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
    auto pos = fItemMap.find(path);
    if (pos == fItemMap.end())
        return nullptr;

    return (*pos).second;
}

QTreeWidgetItem* CMainWindow::getParent(const QFileInfo & fileInfo ) const
{
    auto path = fileInfo.absolutePath();
    auto relToDir = this->relToDir();
    path = relToDir.relativeFilePath(path);
    if ( path.isEmpty() )
    {
        path = ".";
    }
    auto retVal = getItem( path );
    if ( !retVal ) 
    {
        auto parent = getParent( QFileInfo( fileInfo.absolutePath() ) );
        Q_ASSERT( parent );

        retVal = new QTreeWidgetItem( parent, QStringList() << path, eParentDir );
        retVal->setExpanded( true );
        fItemMap[path] = retVal;
        return retVal;
    }
    return retVal;
}

int CMainWindow::getNumDirs(const QString& dir, QProgressDialog * dlg ) const
{
    qApp->processEvents();
    QDirIterator ii(dir, QStringList() << "*.m3u", QDir::Filter::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::IteratorFlag::Subdirectories);

    int cnt = 0;
    while (ii.hasNext())
    {
        ii.next();
        if (dlg->wasCanceled())
            break;

        if (skipDir(ii.fileName()))
            continue;

        if ( cnt % 5 == 0 )
            qApp->processEvents();
        cnt++;
    }
    return cnt;
}

void CMainWindow::slotTransform()
{
    QProgressDialog dlg(tr("Computing Number of M3U Files to fix..."), "Cancel", 0, 0, this);
    dlg.setMinimumDuration(0);
    dlg.setValue(1);

    int numDirs = getNumM3UToFix( &dlg );
    if (dlg.wasCanceled())
        return;
    dlg.setLabelText(tr("Fixing M3Us..."));
    dlg.setRange(0, numDirs);
    dlg.setValue(0);

    for (auto&& ii = 0; ii < fImpl->directories->topLevelItemCount(); ++ii)
    {
        qApp->processEvents();
        transform(fImpl->directories->topLevelItem(ii), ii, &dlg);
    }
}

int CMainWindow::getNumM3UToFix( QProgressDialog * dlg, QTreeWidgetItem * item ) const
{
    if (dlg->wasCanceled())
        return 0;

    int retVal = (item && ( item->type() == ENodeType::eM3U ) ) ? 1 : 0;

    for (int ii = 0; ii < (item ? item->childCount() : fImpl->directories->topLevelItemCount() ); ++ii )
    {
        if (dlg->wasCanceled())
            break;

        auto child = item ? item->child( ii ) : fImpl->directories->topLevelItem( ii );
        if (!child)
            continue;
        retVal += getNumM3UToFix(dlg, child);
    }

    return retVal;
}

QString CMainWindow::getPath( QTreeWidgetItem* item ) const
{
    if ( !item )
        return QString();
    if ( item->text(0) == "." )
        return relToDir().absolutePath();
    else
    {
        auto retVal = QDir( getPath( item->parent() ) ).absoluteFilePath( item->text( 0 ) );
        return retVal;
    }
}

void CMainWindow::generateM3U( QTreeWidgetItem * item ) const
{
    if ( !item )
        return;
    auto path = relToDir().absoluteFilePath( item->text( 0 ));

    QFile fi( path );
    fi.open( QFile::ReadOnly | QFile::Text );
    if ( !fi.isOpen() )
    {
        QMessageBox::critical( nullptr, QString( "Could not open" ), QString( "Could not open file '%1'" ).arg( path ) );
        return;
    }
    QString currLine;
    std::list< std::pair< QString, QString > > files;

    QString prevInf;

    while( !fi.atEnd() )
    {
        currLine = fi.readLine().trimmed();
        if ( currLine.isEmpty() )
            continue;
        else if ( currLine == "#EXTM3U" )
            continue;
        else if ( currLine.startsWith( "#EXTINF" ) )
        {
            prevInf = QUrl::fromPercentEncoding( currLine.toUtf8() );
            QRegularExpression regExp( "(?<prefix>.*,)\\s*\\d+\\s*-\\s*(?<name>.*)" );
            auto match = regExp.match( prevInf );
            if ( match.hasMatch() )
            {
                auto name = match.captured( "name" ).trimmed();
                auto prefix = match.captured( "prefix" ).trimmed();
                prevInf = prefix + name;
            }
        }
        else 
        {
            auto fileName = QUrl::fromPercentEncoding( currLine.toUtf8() );
            QRegularExpression regExp( "\\d+\\s*-\\s*(?<name>.*)" );
            auto match = regExp.match( fileName );
            if ( match.hasMatch() )
            {
                fileName = match.captured( "name" ).trimmed();
            }

            files.push_back( std::make_pair( prevInf,fileName ) );
            prevInf.clear();
        }
    }

    fi.close();
    auto fileName = QFileInfo( path ).absoluteFilePath() + ".bak";
    QFile::remove( fileName );
    if ( !fi.rename( fileName ) )
    {
        QMessageBox::critical( nullptr, QString( "Could not backup" ), QString( "Could not backup file '%1' to '%2'" ).arg( path ).arg( fileName ) );
        return;
    }

    QFile outFile = QFile( path );
    outFile.open( QFile::WriteOnly | QFile::Text );
    if ( !outFile.isOpen() )
    {
        QMessageBox::critical( nullptr, QString( "Could not open" ), QString( "Could not create file '%1'" ).arg( path ) );
        return;
    }
    QTextStream ts( &outFile );
    ts << "#EXTM3U" << "\n";
    for( auto && ii : files )
    {
        if ( !ii.first.isEmpty() && !ii.second.isEmpty() )
            ts << ii.first << "\n";
        if ( !ii.second.isEmpty() )
        {
            auto dir = QDir( QFileInfo( path ).absolutePath() );
            auto moviePath = getMoviePath( dir, ii.second, item );
            ts << QUrl::toPercentEncoding( moviePath ) << "\n";
        }
    }
    outFile.close();
}

std::list< QTreeWidgetItem * > CMainWindow::getMovies( QTreeWidgetItem * item ) const
{
    std::list< QTreeWidgetItem* > retVal;
    for ( int ii = 0; ii < item->childCount(); ++ii )
    {
        auto child = item->child( ii );
        if ( child->type() == eParentDir )
        {
            auto children = getMovies( child );
            retVal.insert( retVal.end(), children.begin(), children.end() );
        }
        else if ( child->type() == eMKV )
            retVal.push_back( child );
    }
    return retVal;
}

QString CMainWindow::getMoviePath( const QDir& dir, const QString& origName, QTreeWidgetItem * item ) const
{
    if ( QFileInfo( dir.absoluteFilePath( origName ) ).exists() )
        return origName;

    QRegularExpression regExp( "\\d+\\s*-\\s*(?<name>.*)" );
    auto match = regExp.match( origName );
    if ( match.hasMatch() )
    {
        auto name = match.captured( "name" );
        return getMoviePath( dir, name, item );
    }

    auto mkvFiles = getMovies( item->parent() );
    for( auto && ii : mkvFiles )
    {
        auto relPath = ii->text( 0 );
        auto pos = relPath.lastIndexOf( "/" );
        auto fileName = relPath;
        if ( pos != -1 )
        {
            fileName = fileName.mid( pos + 1 );
        }
        if ( ( origName == fileName ) || ( QFileInfo( origName ).baseName() == QFileInfo( fileName ).baseName() ) )
        {
            // found it
            auto absPath = relToDir().absoluteFilePath( relPath );
            auto itemDir = QFileInfo( relToDir().absoluteFilePath( item->text( 0 ) ) ).absolutePath();

            auto retVal = QDir( itemDir ).relativeFilePath( absPath );
            return retVal;
        }
    }
    return origName;
}

void CMainWindow::transform( QTreeWidgetItem * item, int /*pos*/, QProgressDialog * dlg)
{
    if (dlg->wasCanceled())
        return;

    if (!item)
        return;

    if ( item->type() == eM3U )
    {
        generateM3U( item );
    }

    for( int ii = 0; ii < item->childCount(); ++ii )
    {
        if (dlg->wasCanceled())
            return;
        transform(item->child(ii), ii, dlg);
    }
}




