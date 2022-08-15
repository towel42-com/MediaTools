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

    connect(fImpl->lhsDir, &QLineEdit::textChanged, this, &CMainWindow::slotDirectoryChanged);
    connect(fImpl->rhsDir, &QLineEdit::textChanged, this, &CMainWindow::slotDirectoryChanged);
    connect(fImpl->btnSelectLHSDir, &QPushButton::clicked, this, &CMainWindow::slotSelectLHSDirectory);
    connect(fImpl->btnSelectRHSDir, &QPushButton::clicked, this, &CMainWindow::slotSelectRHSDirectory);
    connect(fImpl->btnTransform, &QPushButton::clicked, this, &CMainWindow::slotTransform);

    auto completer = new QCompleter(this);
    auto fsModel = new QFileSystemModel(completer);
    fsModel->setRootPath("");
    completer->setModel(fsModel);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);

    fImpl->lhsDir->setCompleter(completer);
    fImpl->rhsDir->setCompleter(completer);

    QTimer::singleShot(0, this, &CMainWindow::slotDirectoryChanged);
}

CMainWindow::~CMainWindow()
{
    saveSettings();
}

void CMainWindow::loadSettings()
{
    QSettings settings;

    fImpl->lhsDir->setText(settings.value("LHSDirectory", QString()).toString());
    fImpl->rhsDir->setText(settings.value("RHSDirectory", QString()).toString());
}

void CMainWindow::saveSettings()
{
    QSettings settings;

    settings.setValue("LHSDirectory", fImpl->lhsDir->text());
    settings.setValue("RHSDirectory", fImpl->rhsDir->text());
}

void CMainWindow::slotDirectoryChanged()
{
    QFileInfo lhs(fImpl->lhsDir->text());
    bool aOK = !fImpl->lhsDir->text().isEmpty() && lhs.exists() && lhs.isDir();
    QFileInfo rhs(fImpl->rhsDir->text());
    aOK = aOK && !fImpl->rhsDir->text().isEmpty() && rhs.exists() && rhs.isDir();

    if ( aOK )
        QTimer::singleShot(0, this, &CMainWindow::slotLoad);
}

void CMainWindow::slotSelectLHSDirectory()
{
    auto dir = QFileDialog::getExistingDirectory(this, tr("Select LHS Directory:"), fImpl->lhsDir->text());
    if (!dir.isEmpty())
        fImpl->lhsDir->setText(dir);
}

void CMainWindow::slotSelectRHSDirectory()
{
    auto dir = QFileDialog::getExistingDirectory(this, tr("Select LHS Directory:"), fImpl->rhsDir->text());
    if (!dir.isEmpty())
        fImpl->rhsDir->setText(dir);
}

void CMainWindow::slotLoad()
{
    loadDirectory();
}

void CMainWindow::loadDirectory()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    fDirMap.clear();
    fImpl->directories->clear();
    fImpl->directories->setHeaderLabels(QStringList() << "LHS Name" << "RHS Name");
    auto header = fImpl->directories->header();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);

    QProgressDialog dlg(tr("Computing Number of Directories..."), "Cancel", 0, 0, this);
    dlg.setMinimumDuration(0);
    dlg.setValue(1);

    int numDirs = getNumDirs( fImpl->lhsDir->text(), &dlg );
    if (dlg.wasCanceled())
        return;
    dlg.setLabelText(tr("Finding Directories..."));
    dlg.setRange(0, numDirs);
    dlg.setValue(0);

    auto rootDir = new QTreeWidgetItem(fImpl->directories, QStringList() << ".", 1);
    rootDir->setExpanded(true);
    fDirMap["."] = rootDir;

    QDirIterator ii(fImpl->lhsDir->text(), QStringList() << "*.*" << "*", QDir::Filter::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::IteratorFlag::Subdirectories);

    auto lhsRelToDir = QDir(fImpl->lhsDir->text());
    auto rhsRelToDir = QDir(fImpl->rhsDir->text());

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

        auto lhsInfo = ii.fileInfo();

        QRegularExpression regExp("^(?<basename>.*)\\s*\\((?<year>\\d{4})\\)\\s*(-\\s*.*)?\\s*\\[(tmdbid|imdbid)\\=.*\\]$");
        auto leafName = lhsInfo.fileName();
        auto match = regExp.match(leafName);
        ENodeType type = eOK;
        if (!match.hasMatch())
        {
            if ( !hasChildDirs(lhsInfo) )
                type = eBadFileName;
        }

        regExp = QRegularExpression(".*\\s{2,}");
        match = regExp.match(leafName);
        if (match.hasMatch())
            type = eBadFileName;

        auto relPath = lhsRelToDir.relativeFilePath(ii.filePath());
        //qDebug() << "Working on " << relPath;
        //auto rhsInfo = QFileInfo(rhsRelToDir.absoluteFilePath(relPath));

        auto parent = getParent(lhsInfo);
        Q_ASSERT(parent);

        //QString rhsRelPathName = rhsInfo.exists() ? relPath : "";
        //ENodeType type = eParentDir;
        //if (!rhsInfo.exists())
        //{
        //    auto leafName = QFileInfo(relPath).fileName();
        //    auto parentDir = parent->text(0);
        //    QRegularExpression regExp("^(?<basename>.*)\\s*\\((?<year>\\d{4})\\)\\s*\\[(tmdbid|imdbid)\\=.*\\]$");
        //    Q_ASSERT(regExp.isValid());

        //    auto match = regExp.match( leafName );
        //    if (match.hasMatch())            
        //    {
        //        auto relToDir = QDir(rhsRelToDir.absoluteFilePath(parentDir));
        //        auto baseName = match.captured("basename").trimmed();
        //        auto year = match.captured("year").trimmed();

        //        auto rhsPath = relToDir.absoluteFilePath(baseName);
        //        //qDebug() << "Checking RHS for" << rhsPath;
        //        rhsInfo = QFileInfo( rhsPath );
        //        if (!rhsInfo.exists())
        //        {
        //            // check to see if the year exists
        //            rhsPath = relToDir.absoluteFilePath(baseName) + QString(" (%1)").arg(year);
        //            //qDebug() << "Checking RHS for" << rhsPath;
        //            rhsInfo = QFileInfo(rhsPath);
        //        }

        //        if (!rhsInfo.exists())
        //        {
        //            type = ENodeType::eMissingDir;
        //        }
        //        else
        //        {
        //            type = ENodeType::eOKDirToRename;
        //            rhsRelPathName = rhsRelToDir.relativeFilePath(rhsInfo.filePath());
        //        }
        //    }
        //    else // not a (year) [tmdb=] type of directory
        //    {
        //        type = eMissingDir;
        //    }
        //}

        QTreeWidgetItem* item = new QTreeWidgetItem(parent, QStringList() << relPath, type);
        if ( type == eOKDirToRename )
            item->setBackground(0, Qt::green);
        else if (type == eMissingDir)
        {
            item->setBackground(0, Qt::red);
            qDebug() << "Missing directory" << relPath;
        }
        else if (type == eBadFileName)
        {
            item->setBackground(0, Qt::red);
            qDebug() << "Bad file name" << relPath;
        }
        item->setExpanded(true);
        fDirMap[relPath] = item;
    }

    QApplication::restoreOverrideCursor();
    qApp->processEvents();
}

bool CMainWindow::hasChildDirs(const QFileInfo& lhsInfo) const
{
    QDirIterator jj(lhsInfo.absoluteFilePath(), QStringList() << "*.*" << "*", QDir::Filter::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
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
    auto pos = fDirMap.find(path);
    Q_ASSERT(pos != fDirMap.end());
    if (pos == fDirMap.end())
        return nullptr;

    return (*pos).second;
}

QTreeWidgetItem* CMainWindow::getParent(const QFileInfo & fileInfo ) const
{
    auto path = fileInfo.absolutePath();
    auto relToDir = QDir(fImpl->lhsDir->text());
    path = QDir(fImpl->lhsDir->text()).relativeFilePath(path);
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

    int retVal = (parent && ( parent->type() == ENodeType::eOKDirToRename ) ) ? 1 : 0;

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

void CMainWindow::transform( QTreeWidgetItem * item, int pos, QProgressDialog * dlg)
{
    if (dlg->wasCanceled())
        return;

    if (!item)
        return;
    if ( item->type() == ENodeType::eOKDirToRename )
    {
        auto lhsRelToDir = QDir(fImpl->lhsDir->text());
        auto lhsRelPath = item->text(0);
        auto lhsPath = QFileInfo(lhsRelPath).path();
        lhsPath = lhsRelToDir.relativeFilePath(lhsPath);
        auto newDirName = QFileInfo(lhsRelPath).fileName();

        auto rhsRelToDir = QDir(fImpl->rhsDir->text());
        auto rhsRelPath = item->text(1);
        auto oldRhsAbsPath = rhsRelToDir.absoluteFilePath(rhsRelPath);
        auto newRhsAbsPath = QDir(rhsRelToDir.absoluteFilePath(lhsPath)).absoluteFilePath(newDirName);
        auto newRhsRelPath = rhsRelToDir.relativeFilePath(newRhsAbsPath);

        qDebug() << "Renaming " << oldRhsAbsPath << " to " << newRhsAbsPath;

        if ( !QFile::rename( oldRhsAbsPath, newRhsAbsPath ) )
        {
            QMessageBox::critical(this, "Could not rename", QString("Could not rename <b>%1</b> to <b>%2></b>").arg(oldRhsAbsPath).arg(newRhsAbsPath));
        }
        else
        {
            if (item->parent())
            {
                auto parent = item->parent();
                parent->takeChild(pos);
                item = new QTreeWidgetItem((QTreeWidgetItem*)nullptr, QStringList() << item->text( 0 ) << newRhsRelPath, eParentDir);
                parent->insertChild(pos, item);
            }
        }
        dlg->setValue(dlg->value() + 1);
        qApp->processEvents();
    }

    for( int ii = 0; ii < item->childCount(); ++ii )
    {
        if (dlg->wasCanceled())
            return;
        transform(item->child(ii), ii, dlg);
    }
}




