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

#ifndef _DIRMODEL_H
#define _DIRMODEL_H

#include <QFileSystemModel>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <set>
#include <QSet>
#include <tuple>
class QMediaPlaylist;
inline uint qHash(const QFileInfo& data, uint seed=0)
{
    return qHash(data.absoluteFilePath().toLower(), seed);
}

class QTimer;
class CDirModel : public QFileSystemModel
{
    Q_OBJECT
public:
    CDirModel(QObject* parent = nullptr);

    void reset();
    void setPreLoaded();

    ~CDirModel();
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& idx) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual int rowCount(const QModelIndex& parent) const override;

    QModelIndex rootIndex() const;

    int computeDepth(const QModelIndex& idx) const;
    bool acceptRow(const QModelIndex& srcIdex, int depth) const;

public Q_SLOTS:

private Q_SLOTS :
    void slotDirLoaded(const QString& dir);
    void slotDirsFinishedLoading();

    void finishedLoading(const QFileInfo & path, QSet< QFileInfo >& handled);

Q_SIGNALS:
    void sigLoadFinished(); 
private:
    std::tuple< QString, QString, QString, bool > computeTMDBInfo(const QModelIndex& index) const;
    std::tuple< QString, QString, QString, bool > getTMDBInfo(const QString& nfoFile) const;

    QString getTMDBURL(const QModelIndex& index, bool* aOK = nullptr) const;
    QString getTMDBID(const QModelIndex& index, bool* aOK = nullptr) const;
    QString getTMDBYear(const QModelIndex& index, bool* aOK = nullptr) const;
    mutable std::map< QString, std::tuple< QString, QString, QString, bool > > fURLCache; // url, id, year, aok
    QTimer* fTimer{ nullptr };
    QSet< QFileInfo > fLoadedDirs;
    bool fFinishedLoading{ false };
};

class CDirFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    CDirFilterModel(QObject* parent = nullptr);
    void setSourceModel(QAbstractItemModel* sourceModel) override;


    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& parent) const override;
private Q_SLOTS:

private:
};



#endif // 
