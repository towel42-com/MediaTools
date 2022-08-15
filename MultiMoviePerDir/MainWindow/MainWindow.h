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

#ifndef _MAINWINDOW_H
#define _MAINWINDOW_H

#include <QMainWindow>
#include <QFileInfo>
#include <QUrl>
class CDirModel;
class CDirFilterModel;
class QFileInfo;
class QDir;
namespace NSABUtils { class CButtonEnabler; }
class QFile;
namespace Ui {class CMainWindow;};

using TTransformation=std::pair< QDir, std::list< std::pair< QFileInfo, QString > > >;
class CMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    CMainWindow(QWidget* parent = 0);
    ~CMainWindow();
public Q_SLOTS:
    void slotSelectDirectory();
    void slotDirectoryChanged();
    void slotLoad();
    void slotTransform();
    void slotDirLoaded(const QString& dirName);
    void slotFinishedLoading();
    void slotDoubleClicked(const QModelIndex& idx);
private:
    int getFoldersRemaining(const QModelIndex& idx) const;
    bool folderHasMovie(const QModelIndex& idx) const;
    void transformFile(const QFileInfo& fileInfo, TTransformation& transformations) const;
    QModelIndex getIndex(const QString& dirName) const;
    bool isDir(const QModelIndex& idx) const;
    QUrl getTMDBUrl(const QModelIndex& idx) const;
    QString getTMDBID(const QModelIndex& idx) const;
    QString getTMDBYear(const QModelIndex& idx) const;
    QFileInfo getFileInfo(const QModelIndex& idx) const;
    void loadSettings();
    void saveSettings();
    void loadDirectory();

    CDirModel* fDirModel{ nullptr };
    CDirFilterModel* fDirFilterModel{ nullptr };
    NSABUtils::CButtonEnabler* fBtnEnabler{ nullptr };
    std::unique_ptr< Ui::CMainWindow > fImpl;
};

#endif 
