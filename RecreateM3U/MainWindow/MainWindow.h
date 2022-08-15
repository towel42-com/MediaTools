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

#include <QDir>
class QTreeWidgetItem;
class QFileInfo;
class QProgressDialog;
#include <QMainWindow>

namespace Ui {class CMainWindow;};

class CMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    enum ENodeType
    {
        eParentDir = 1000,
        eM3U,
        eMKV
    };

    CMainWindow(QWidget* parent = 0);
    ~CMainWindow();
public Q_SLOTS:
    void slotSelectDirectory();
    void slotDirectoryChanged();
    void slotLoad();
    void slotTransform();
private:
    void loadSettings();
    void saveSettings();
    void loadDirectory();

    QDir relToDir() const;
    QString getPath( QTreeWidgetItem* item ) const;
    void generateM3U( QTreeWidgetItem* item ) const;

    bool hasChildDirs(const QFileInfo& lhsInfo ) const;
    std::list< QTreeWidgetItem* > getMovies( QTreeWidgetItem* item ) const;
    QString getMoviePath( const QDir& dir, const QString& origName, QTreeWidgetItem* item ) const;

    bool skipDir(const QString& path) const;
    void transform(QTreeWidgetItem* item, int pos, QProgressDialog * dlg);

    int getNumDirs( const QString & dir, QProgressDialog * dlg ) const;
    int getNumM3UToFix(QProgressDialog* dlg, QTreeWidgetItem* parent = nullptr) const;
    void loadM3UItem( const QFileInfo & info, QTreeWidgetItem* parent, QProgressDialog* dlg );

    QTreeWidgetItem* getItem(const QString & info) const;
    QTreeWidgetItem* getParent(const QFileInfo& info) const;

    mutable std::unordered_map< QString, QTreeWidgetItem* > fItemMap;
    std::unique_ptr< Ui::CMainWindow > fImpl;
};

#endif 
