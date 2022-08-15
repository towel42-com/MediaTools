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
#include <QMediaPlaylist>
#include <QMessageBox>
#include <QDate>
#include <QDesktopServices> 

CMainWindow::CMainWindow(QWidget* parent)
    : QMainWindow(parent),
    fImpl(new Ui::CMainWindow)
{
    fImpl->setupUi(this);
    connect(fImpl->directory, &QLineEdit::textChanged, this, &CMainWindow::slotDirectoryChanged);
    connect(fImpl->btnSelectDir, &QPushButton::clicked, this, &CMainWindow::slotSelectDirectory);
    connect(fImpl->btnLoad, &QPushButton::clicked, this, &CMainWindow::slotLoad);
    connect(fImpl->btnTransform, &QPushButton::clicked, this, &CMainWindow::slotTransform);
    connect(fImpl->files, &QTreeView::doubleClicked, this, &CMainWindow::slotDoubleClicked);

    auto completer = new QCompleter(this);
    auto fsModel = new QFileSystemModel(completer);
    fsModel->setRootPath("");
    completer->setModel(fsModel);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);

    fImpl->directory->setCompleter(completer);

    loadSettings();
}

CMainWindow::~CMainWindow()
{
    saveSettings();
}

void CMainWindow::loadSettings()
{
    QSettings settings;

    fImpl->directory->setText(settings.value("Directory", QString()).toString());
    fImpl->extensions->setText(settings.value("Extensions", QString("*.mkv;*.mp4;*.avi;*.m4v")).toString());

    slotDirectoryChanged();
}

void CMainWindow::saveSettings()
{
    QSettings settings;

    settings.setValue("Directory", fImpl->directory->text());
    settings.setValue("Extensions", fImpl->extensions->text());
}

void CMainWindow::slotDirectoryChanged()
{
    QFileInfo fi(fImpl->directory->text());
    fImpl->btnLoad->setEnabled(!fImpl->directory->text().isEmpty() && fi.exists() && fi.isDir());
}

void CMainWindow::slotSelectDirectory()
{
    auto dir = QFileDialog::getExistingDirectory(this, tr("Select Directory:"), fImpl->directory->text());
    if (!dir.isEmpty())
        fImpl->directory->setText(dir);
}

void CMainWindow::slotLoad()
{
    loadDirectory();
}

QModelIndex CMainWindow::getIndex(const QString& dirName) const
{
    auto srcIdx = fDirModel->index( dirName );
    auto proxyIdx = fDirFilterModel->mapFromSource(srcIdx);
    return proxyIdx;
}

bool CMainWindow::isDir( const QModelIndex & idx ) const
{
    if (idx.model() == fDirModel)
        return fDirModel->isDir(idx);
    if ( idx.model() == fDirFilterModel )
        return isDir(fDirFilterModel->mapToSource(idx));
    return false;
}

QFileInfo CMainWindow::getFileInfo(const QModelIndex& idx) const
{
    if (idx.model() == fDirModel)
        return fDirModel->fileInfo(idx);
    if (idx.model() == fDirFilterModel)
        return getFileInfo(fDirFilterModel->mapToSource(idx));
    return QFileInfo();
}

void CMainWindow::slotFinishedLoading()
{
    QApplication::restoreOverrideCursor();
    int cnt = getFoldersRemaining( fDirFilterModel->mapFromSource( fDirModel->rootIndex() ) );
    QMessageBox::information(this, tr("Number of Movies"), tr("There are <b>'%1'</b> movies that need tmdbid added").arg( cnt ) );
}

int CMainWindow::getFoldersRemaining( const QModelIndex & idx ) const
{
    int retVal = 0;
    if (folderHasMovie(idx))
        retVal++;
    for( auto ii = 0; ii < fDirFilterModel->rowCount( idx ); ++ii )
    {
        auto childIndex = fDirFilterModel->index(ii, 0, idx);
        retVal += getFoldersRemaining(childIndex);
    }
    return retVal;
}

bool CMainWindow::folderHasMovie(const QModelIndex& idx) const
{
    if (!isDir(idx) && idx.isValid() )
        return false;
    auto srcIdx = fDirFilterModel->mapToSource(idx);
    qDebug() << "Checking " << getFileInfo(srcIdx).absoluteFilePath() << " to see if it has a movie";
    for (int ii = 0; ii < fDirModel->rowCount(srcIdx); ++ii)
    {
        auto childIndex = fDirModel->index(ii, 0, srcIdx);
        if (isDir(childIndex))
            continue;
        auto fileInfo = getFileInfo(childIndex);
        if (fileInfo.suffix().toLower() == "mkv")
            return true;
    }
    return false;
}

void CMainWindow::loadDirectory()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    delete fDirModel;
    delete fDirFilterModel;

    fDirModel = new CDirModel(this);
    fDirFilterModel = new CDirFilterModel( this );
    fDirFilterModel->setSourceModel( fDirModel );
    fImpl->files->setModel(fDirFilterModel);
    fDirModel->setReadOnly(true);
    fDirModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    fDirModel->setNameFilterDisables(false);
    connect(fDirModel, &QFileSystemModel::directoryLoaded, this, &CMainWindow::slotDirLoaded);
    connect(fDirModel, &CDirModel::sigLoadFinished, this, &CMainWindow::slotFinishedLoading);
    fBtnEnabler = new NSABUtils::CButtonEnabler(fImpl->files, fImpl->btnTransform);

    fDirModel->reset();
    fDirModel->setNameFilters(fImpl->extensions->text().split(";"));
    fDirModel->setRootPath(QString());
    fDirModel->setRootPath(fImpl->directory->text());
    auto rootIdx = getIndex(fImpl->directory->text());
    fImpl->files->setRootIndex( rootIdx );
    fImpl->files->setExpandsOnDoubleClick(false);
}

QString CMainWindow::getTMDBYear(const QModelIndex& idx) const
{
    if (idx.model() == fDirFilterModel)
        return getTMDBYear(fDirFilterModel->mapToSource(idx));

    if (idx.model() != fDirModel)
        return QString();

    if (idx.column() != 6)
    {
        return getTMDBYear(fDirModel->index(idx.row(), 6, idx.parent()));
    }

    return idx.data().toString();
}

QString CMainWindow::getTMDBID(const QModelIndex& idx) const
{
    if (idx.model() == fDirFilterModel)
        return getTMDBID(fDirFilterModel->mapToSource(idx));

    if (idx.model() != fDirModel)
        return QString();

    if (idx.column() != 5)
    {
        return getTMDBID(fDirModel->index(idx.row(), 5, idx.parent()));
    }

    return idx.data().toString();
}

QUrl CMainWindow::getTMDBUrl(const QModelIndex& idx) const
{
    if (idx.model() == fDirFilterModel)
        return getTMDBUrl(fDirFilterModel->mapToSource(idx));

    if (idx.model() != fDirModel)
        return QUrl();

    if (idx.column() != 4)
    {
        return getTMDBUrl(fDirModel->index(idx.row(), 4, idx.parent()));
    }

    auto url = idx.data().toString();
    if (url.isEmpty())
        return QUrl();

    return QUrl(url);
}

void CMainWindow::slotDoubleClicked(const QModelIndex & idx )
{
    if (isDir(idx))
    {
        auto url = getTMDBUrl(idx);
        if (url.isValid())
            QDesktopServices::openUrl(QUrl(url));
    }
    else
    {
        auto fileInfo = getFileInfo(idx);
        if (fileInfo.exists())
            QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
    }
}


void CMainWindow::slotDirLoaded(const QString& dirName)
{
    qDebug() << "slotDirLoaded: " << dirName;
    auto idx = getIndex(dirName);
    if ( !idx.isValid() )
        return;
    Q_ASSERT(idx.isValid());
    auto numRows = fDirFilterModel->rowCount(idx);
    for (int ii = 0; ii < numRows; ++ii)
    {
        auto childIndex = fDirFilterModel->index(ii, 0, idx);
        if (childIndex.isValid() && isDir(childIndex))
        {
            fImpl->files->setExpanded(childIndex, true);
        }
    }
}

void CMainWindow::slotTransform()
{
    auto selected = fImpl->files->selectionModel()->selectedIndexes();
    std::set< QString > handled;
    for (auto&& ii : selected)
    {
        if (!isDir(ii))
            continue;
        auto fi = getFileInfo(ii);
        auto dirName = fi.absoluteFilePath();
        auto pos = handled.find(dirName);
        if (pos != handled.end())
            continue;

        handled.insert(dirName);
        auto id = getTMDBID(ii);
        if (id.isEmpty())
            continue;
        auto year = getTMDBYear(ii);

        auto oldName = fi.fileName();

        QString newName = oldName;
        if (!year.isEmpty())
        {
            QRegularExpression regExp("(?<prefix>\\s*)\\(\\s*(?<year>\\d+)\\s*\\)(?<suffix>\\s*)");
            auto match = regExp.match(newName);
            if (match.hasMatch())
                newName.replace(regExp, QString(" (%1)").arg(year));
            else
                newName += QString(" (%1)").arg(year);
        }
        newName += QString(" [tmdbid=%3]").arg(id);

        auto dir = QDir(dirName);
        dir.cdUp();
        if (!dir.rename(oldName, newName))
        {
            QMessageBox::critical(this, "Could not rename", QString("Could not rename '%1' to '%2'").arg(dirName).arg(newName));
            continue;
        }
    }
    fImpl->files->selectionModel()->clearSelection();
}




