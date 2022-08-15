#ifndef CMAINWINDOW_H
#define CMAINWINDOW_H

#include <QMainWindow>
#include <QDate>
#include <QList>
#include <memory>

class CSqlTableModel;
class CFilterModel;
namespace Ui { class CMainWindow; }

char* dbConnectionName();

class CMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    CMainWindow( QWidget *parent = 0 );
	~CMainWindow();

Q_SIGNALS:

public Q_SLOTS:
    void slotSelectLibraryFile();
    void slotLibraryFileChanged();
    void slotAutoFix();


	void slotApply();
private:
    void initModel();
	void updateRecord(int ii);
    CFilterModel* fFilterModel{ nullptr };
    CSqlTableModel * fModel{ nullptr };
    std::unique_ptr< Ui::CMainWindow > fImpl;
};

#endif 
