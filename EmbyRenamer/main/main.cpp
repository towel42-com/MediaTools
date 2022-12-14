#include "MainWindow/MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QSqlDatabase>

int main( int argc, char ** argv )
{
    QApplication appl( argc, argv );
    Q_INIT_RESOURCE( application );

    QCoreApplication::setOrganizationName( "OnShore Consulting Services" );
    QCoreApplication::setApplicationName( "Emby Renamer" );
    QCoreApplication::setApplicationVersion( "1.0.0" );
    QCoreApplication::setOrganizationDomain( "www.towel42.com" );
    appl.setWindowIcon( QPixmap( ":/resources/finddupe.png" ) );

    QString appDir = appl.applicationDirPath();

    QStringList libPaths = appl.libraryPaths();
    libPaths.push_front( appDir );
    appl.setLibraryPaths( libPaths );

    if ( !QSqlDatabase::isDriverAvailable( "QSQLITE" ) )
    {
        QMessageBox::critical( NULL, "Error initializing system", "Could not find Database libraries.  Please re-install or contact support." );
        return 0;
    }

	QSqlDatabase db;
	if (QSqlDatabase::contains(dbConnectionName()))
		db = QSqlDatabase::database(dbConnectionName());
	else
		db = QSqlDatabase::addDatabase("QSQLITE", dbConnectionName());

    CMainWindow * wnd = new CMainWindow;
    wnd->show();
    return appl.exec();
}

