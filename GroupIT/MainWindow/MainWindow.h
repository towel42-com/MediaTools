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
        eID = 1000,
        eDir,
        eFile,
        eBadFileName
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

    void validateFiles( QTreeWidgetItem *idItem );

    bool hasChildDirs(const QFileInfo& info ) const;

    bool skipDir(const QString& path) const;
    void transform(QTreeWidgetItem* item, int pos, QProgressDialog * dlg);

    int getNumDirs( const QString & dir, QProgressDialog * dlg ) const;
    int getNumDirsToRename(QProgressDialog* dlg, QTreeWidgetItem* parent = nullptr) const;

    QTreeWidgetItem* getItem(const QString & info) const;
    QTreeWidgetItem* getParent(const QFileInfo& info) const;

    std::unordered_map< QString, QTreeWidgetItem* > fIDMap;
    std::unordered_map< QString, QTreeWidgetItem * > fDirMap;

    std::unique_ptr< Ui::CMainWindow > fImpl;
};

#endif 
