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

#include "DirModel.h"
#include <QDebug>
#include <QUrl>
#include <QInputDialog>
#include <QTextStream>
#include <QCollator>
#include <QXmlQuery>
#include <QTimer>
#include <QUrl>
#include <set>
#include <list>
CDirModel::CDirModel(QObject* parent /*= 0*/) :
    QFileSystemModel(parent)
{
    (void)connect(this, &CDirModel::directoryLoaded, this, &CDirModel::slotDirLoaded);
    (void)connect(this, &CDirModel::rootPathChanged, this, [this](const QString& /*path*/) {reset();
    });

    fTimer = new QTimer(this);
    fTimer->setInterval(2000);
    fTimer->setSingleShot(true);
    connect(fTimer, &QTimer::timeout, this, &CDirModel::slotDirsFinishedLoading);

}

CDirModel::~CDirModel()
{

}

void CDirModel::slotDirLoaded(const QString& path)
{
    fLoadedDirs.insert(QFileInfo(path));

    if (fTimer->isActive())
        fTimer->stop();
    fTimer->start();
}

void CDirModel::slotDirsFinishedLoading()
{
    QSet< QFileInfo > handled;
    if (fLoadedDirs.size() == 1)
    {
        fFinishedLoading = true;
        reset();
    }
    else
    {
        for (auto&& ii : fLoadedDirs)
        {
            finishedLoading(ii, handled);
        }
    }
    emit sigLoadFinished();
}

void CDirModel::finishedLoading(const QFileInfo& fi, QSet< QFileInfo > & handled)
{
    if (handled.find(fi) != handled.end())
        return;
    handled.insert(fi);

    fFinishedLoading = true;
    auto path = fi.absoluteFilePath();
    auto lhsIdx = index(path, 0);
    if (!lhsIdx.isValid())
        return;

    auto rhsIdx = index(path, columnCount(lhsIdx.parent()) - 1);
    if (!rhsIdx.isValid())
        return;

    emit dataChanged(lhsIdx, rhsIdx);
    
    if (fi == QFileInfo(rootPath()))
        return;

    auto parentDir = QFileInfo(fi.absolutePath());
    finishedLoading(parentDir, handled);
}

QVariant CDirModel::data(const QModelIndex& index, int role /*= Qt::DisplayRole */) const
{
    if (!index.isValid())
        return QVariant();
    if (role == Qt::DisplayRole && index.column() == 4)
    {
        return getTMDBURL(index);
    }
    else if (role == Qt::DisplayRole && index.column() == 5 )
    {
        return getTMDBID(index);
    }
    else if (role == Qt::DisplayRole && index.column() == 6)
    {
        return getTMDBYear(index);
    }
    else if ( role == Qt::BackgroundRole )
    {
        if (isDir(index))
        {
            bool aOK;
            getTMDBURL(index, &aOK);
            if (!aOK)
                return QColor(Qt::red);
        }
    }

    return QFileSystemModel::data(index, role);
}

std::tuple< QString, QString, QString, bool > CDirModel::computeTMDBInfo(const QModelIndex& index ) const
{
    auto fileInfo = this->fileInfo(index);

    auto pos = fURLCache.find(fileInfo.absoluteFilePath());
    if (pos != fURLCache.end())
    {
        return (*pos).second;
    }

    if (!isDir(index))
    {
        fURLCache[fileInfo.absoluteFilePath()] = std::make_tuple(QString(), QString(), QString(), false);
        return std::make_tuple(QString(), QString(), QString(), false);
    }

    if (!fileInfo.exists())
    {
        fURLCache[fileInfo.absoluteFilePath()] = std::make_tuple(QString(), QString(), QString(), false);
        return std::make_tuple(QString(), QString(), QString(), false);
    }

    auto it = QDirIterator(fileInfo.absoluteFilePath(), { "*.nfo" });
    QString nfoFile;
    while (it.hasNext())
    {
        it.next();
        auto fi = it.fileInfo();
        if (nfoFile.isEmpty())
            nfoFile = fi.absoluteFilePath();
        else
        {
            fURLCache[fileInfo.absoluteFilePath()] = std::make_tuple(QString(), QString(), QString(), false);
            return std::make_tuple(QString(), QString(), QString(), false);
        }
    }
    if (nfoFile.isEmpty())
    {
        fURLCache[fileInfo.absoluteFilePath()] = std::make_tuple(QString(), QString(), QString(), false);
        return std::make_tuple(QString(), QString(), QString(), false);
    }

    auto url = getTMDBInfo(nfoFile);
    fURLCache[fileInfo.absoluteFilePath()] = url;
    return url;
}

QString CDirModel::getTMDBURL( const QModelIndex & index, bool * aOK ) const
{
    auto data = computeTMDBInfo(index);
    if (aOK)
        *aOK = std::get< 3 >(data);
    return std::get< 0 >( data );
}

QString CDirModel::getTMDBID(const QModelIndex& index, bool* aOK) const
{
    auto data = computeTMDBInfo(index);
    if (aOK)
        *aOK = std::get< 3 >(data);
    return std::get< 1 >(data);
}

QString CDirModel::getTMDBYear(const QModelIndex& index, bool* aOK) const
{
    auto data = computeTMDBInfo(index);
    if (aOK)
        *aOK = std::get< 3 >(data);
    return std::get< 2 >(data);
}

QString getString(QXmlQuery& query, const QString& queryString, bool* aOK /*= nullptr */)
{
    query.setQuery(queryString);
    if (!query.isValid())
    {
        if (aOK)
            *aOK = false;
        return QString();
    }
    QString retVal;
    query.evaluateTo(&retVal);
    retVal = retVal.trimmed();
    if (aOK)
        *aOK = true;
    return retVal;
}

std::tuple< QString, QString, QString, bool > CDirModel::getTMDBInfo( const QString & nfoFile ) const
{
    QFileInfo fileInfo(nfoFile);
    if (!fileInfo.exists())
    {
        return std::make_tuple( QString(), QString(), QString(), false );
    }

    QFile fi( nfoFile);
    QXmlQuery query;
    if (!fi.open(QFile::ReadOnly) || !query.setFocus(&fi))
    {
        return std::make_tuple(QString(), QString(), QString(), false);
    }

    bool aOK;
    auto tmdbid = getString(query, "/movie/tmdbid/string()", &aOK);
    QString url;
    if ( aOK )
    {
        url = QUrl(QString("https://themoviedb.org/movie/%1").arg(tmdbid)).toString();
    }

    auto year = getString(query, "/movie/releasedate/string()", &aOK);
    if (!aOK)
    {
        year = getString(query, "/movie/premiered/string()", &aOK);
    }
    auto pos = year.indexOf("-");
    if (pos != -1) // yy-mm-dd
        year = year.left(pos);

    return std::make_tuple(url, tmdbid, year, true);
}

QVariant CDirModel::headerData(int section, Qt::Orientation orientation, int role /*= Qt::DisplayRole */) const
{
    if ((section == 4) && (orientation == Qt::Orientation::Horizontal) && (role == Qt::DisplayRole))
        return tr("themoviedb URL");
    if ((section == 5) && (orientation == Qt::Orientation::Horizontal) && (role == Qt::DisplayRole))
        return tr("TMDBID");
    if ((section == 6) && (orientation == Qt::Orientation::Horizontal) && (role == Qt::DisplayRole))
        return tr("Release Year");
    return QFileSystemModel::headerData(section, orientation, role);
}

Qt::ItemFlags CDirModel::flags(const QModelIndex& idx) const
{
    return QFileSystemModel::flags(idx);
}

int CDirModel::columnCount(const QModelIndex& parent) const
{
    auto retVal = QFileSystemModel::columnCount(parent);
    return retVal ? retVal + 3 : 0;
}

int CDirModel::rowCount(const QModelIndex& parent) const
{
    if (parent.data() == "#recycle")
    {
        return 0;
    }
    return QFileSystemModel::rowCount(parent);
}


QModelIndex CDirModel::rootIndex() const
{
    auto rootPath = this->rootPath();
    auto root = this->index(rootPath);
    return root;
}

CDirFilterModel::CDirFilterModel(QObject* parent /*= nullptr */) :
    QSortFilterProxyModel(parent)
{
    //setRecursiveFilteringEnabled(true);
}

void CDirFilterModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    QSortFilterProxyModel::setSourceModel(sourceModel);
}

int CDirModel::computeDepth( const QModelIndex & idx ) const
{
    if ( !idx.isValid() )
        return 0;
    qDebug().nospace().noquote() << "Computing Depth for " << idx.data();
    if (idx == rootIndex())
        return 0;
    auto parent = idx.parent();
    return computeDepth(parent) + 1;
}

QString indent( int depth )
{
    return QString(4 * depth, '-') + ">";
}

bool CDirFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& parent) const
{
    auto dirModel = dynamic_cast<CDirModel*>(this->sourceModel());
    if (!dirModel)
        return true;
    auto srcIndex = dirModel->index(sourceRow, 0, parent);

    return dirModel->acceptRow(srcIndex, 0 );
}

bool CDirModel::acceptRow( const QModelIndex & srcIdx, int depth) const
{
    auto baseName = srcIdx.data().toString();
    if (baseName.toLower() == "#recycle")
        return false;

    if (baseName.toLower() == "subs")
        return false;

    if (!fFinishedLoading)
        return true;

    auto isDir = this->isDir(srcIdx);
    if (isDir && canFetchMore(srcIdx))
        return true;

    qDebug().nospace().noquote() << indent(depth) << "Checking to see if " << ( isDir ? "Dir" : "File" ) << srcIdx.data() << " should be shown";
    if (isDir)
    {
        QRegularExpression regExp("\\[tmdbid\\=\\d+\\].*$");
        auto match = regExp.match(baseName);
        bool hasMatch = match.hasMatch();
        if (hasMatch)
            return false;

        regExp = QRegularExpression("\\[imdbid\\=tt\\d+\\].*$");
        match = regExp.match(baseName);
        hasMatch = match.hasMatch();
        if (hasMatch)
            return false;
        for( int ii = 0; ii < rowCount( srcIdx ); ++ii )
        {
            auto idx = index(ii, 0, srcIdx);
            if (this->isDir(idx))
            {
                if (acceptRow(idx, depth + 1))
                    return true;
            }
            else 
            {
                auto fi = fileInfo(idx);
                qDebug() << fi.absoluteFilePath();
                if (fi.suffix().toLower() == "mkv")
                    return true;
            }
        }
        return false;
    }
    else
    {
        //auto parent = srcIdx.parent();
        //return acceptRow(parent, depth + 1);
        auto fi = fileInfo(srcIdx);
        return fi.suffix().toLower() == "mkv";
    }
    return false;
}

void CDirModel::reset()
{
    this->fLoadedDirs.clear(); 
    fFinishedLoading = false;
    fURLCache.clear();
}

void CDirModel::setPreLoaded()
{
    fFinishedLoading = true;
}
