#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "SABUtils/MD5.h"

#include <QFileDialog>
#include <QSqlTableModel>
#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QSqlRecord>

#include <QSettings>
#include <QProgressBar>
#include <QDirIterator>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <QProgressDialog>
#include <QMessageBox>
#include <QThreadPool>
#include <QFontDatabase>
#include <QDebug>

#include <set>
#include <random>

#include <unordered_set>

CMainWindow::CMainWindow( QWidget *parent )
    : QMainWindow( parent ),
    fImpl( new Ui::CMainWindow ),
    fModel( nullptr )
{
    fImpl->setupUi( this );
    setWindowIcon( QIcon( ":/resources/finddupe.png" ) );
    setAttribute( Qt::WA_DeleteOnClose );

    connect( fImpl->selectLibraryFile, &QToolButton::clicked,   this, &CMainWindow::slotSelectLibraryFile );
	connect(fImpl->autofixBtn, &QToolButton::clicked, this, &CMainWindow::slotAutoFix);
	connect(fImpl->applyBtn, &QToolButton::clicked, this, &CMainWindow::slotApply);
	connect(fImpl->libraryFile, &QLineEdit::textChanged, this, &CMainWindow::slotLibraryFileChanged);

    QSettings settings;
    fImpl->libraryFile->setText( settings.value( "LibraryFile", QString() ).toString() );
}

namespace NSql
{
	// SQL keywords
	inline const static QLatin1String as() { return QLatin1String("AS"); }
	inline const static QLatin1String asc() { return QLatin1String("ASC"); }
	inline const static QLatin1String comma() { return QLatin1String(","); }
	inline const static QLatin1String desc() { return QLatin1String("DESC"); }
	inline const static QLatin1String eq() { return QLatin1String("="); }
	// "and" is a C++ keyword
	inline const static QLatin1String et() { return QLatin1String("AND"); }
	inline const static QLatin1String from() { return QLatin1String("FROM"); }
	inline const static QLatin1String leftJoin() { return QLatin1String("LEFT JOIN"); }
	inline const static QLatin1String on() { return QLatin1String("ON"); }
	inline const static QLatin1String orderBy() { return QLatin1String("ORDER BY"); }
	inline const static QLatin1String parenClose() { return QLatin1String(")"); }
	inline const static QLatin1String parenOpen() { return QLatin1String("("); }
	inline const static QLatin1String select() { return QLatin1String("SELECT"); }
	inline const static QLatin1String sp() { return QLatin1String(" "); }
	inline const static QLatin1String where() { return QLatin1String("WHERE"); }

	inline const static QString concat(const QString& a, const QString& b) { return a.isEmpty() ? b : b.isEmpty() ? a : QString(a).append(sp()).append(b); }

	// Build expressions based on key words
	inline const static QString as(const QString& a, const QString& b) { return b.isEmpty() ? a : concat(concat(a, as()), b); }
	inline const static QString asc(const QString& s) { return concat(s, asc()); }
	inline const static QString comma(const QString& a, const QString& b) { return a.isEmpty() ? b : b.isEmpty() ? a : QString(a).append(comma()).append(b); }
	inline const static QString desc(const QString& s) { return concat(s, desc()); }
	inline const static QString eq(const QString& a, const QString& b) { return QString(a).append(eq()).append(b); }
	inline const static QString et(const QString& a, const QString& b) { return a.isEmpty() ? b : b.isEmpty() ? a : concat(concat(a, et()), b); }
	inline const static QString from(const QString& s) { return concat(from(), s); }
	inline const static QString leftJoin(const QString& s) { return concat(leftJoin(), s); }
	inline const static QString on(const QString& s) { return concat(on(), s); }
	inline const static QString orderBy(const QString& s) { return s.isEmpty() ? s : concat(orderBy(), s); }
	inline const static QString paren(const QString& s) { return s.isEmpty() ? s : parenOpen() + s + parenClose(); }
	inline const static QString select(const QString& s) { return concat(select(), s); }
	inline const static QString where(const QString& s) { return s.isEmpty() ? s : concat(where(), s); }
}; 

class CSqlTableModel : public QSqlTableModel
{
public:
    CSqlTableModel(const QString & fileName, QObject* parent) :
        QSqlTableModel(parent, QSqlDatabase::database(dbConnectionName()))
    {
		database().setDatabaseName( fileName );
		if (!database().open())
		{
			QMessageBox::critical(nullptr, tr("Error opening db"), tr("Could not open library.db '%1'").arg(fileName));
			return;
		}

		setTable("MediaItems");
		setEditStrategy(QSqlTableModel::OnManualSubmit);
		setFilter("(IsFolder=0) AND (Path LIKE '/volume2/video/Movies%')");
    }

	QSqlRecord record(int rowNumber) const
	{
		return QSqlTableModel::record(rowNumber);
	}

	QSqlRecord record() const
	{
		if (tableName().isEmpty()) 
		{
			const_cast<CSqlTableModel*>(this)->setLastError(QSqlError(QLatin1String("No table name given"), QString(), QSqlError::StatementError));
			return QSqlRecord();
		}

		auto retVal = QSqlTableModel::record();
		std::set< QString > fields = { "Id", "Path", "Filename", "Name", "SortName", "ForcedSortName", "OriginalTitle", "LockedFields" };
		for (int ii = 0; ii < retVal.count(); ++ii)
		{
			auto field = retVal.fieldName(ii);
			if (fields.find(field) == fields.end())
			{
				retVal.remove(ii);
				ii--;
			}
		}

		return retVal;
	}


	QString selectStatement() const override
	{
        auto tmp = QSqlTableModel::selectStatement();
		if (tableName().isEmpty())
		{
			const_cast< CSqlTableModel * >( this )->setLastError(QSqlError(QLatin1String("No table name given"), QString(), QSqlError::StatementError));
			return QString();
		}

		if (record().isEmpty())
		{
            const_cast<CSqlTableModel*>(this)->setLastError(QSqlError(QLatin1String("Unable to find table ") + tableName(), QString(), QSqlError::StatementError));
			return QString();
		}

		const QString stmt = database().driver()->sqlStatement(QSqlDriver::SelectStatement, tableName(), record(), false);
		if (stmt.isEmpty())
		{
            const_cast<CSqlTableModel*>(this)->setLastError(QSqlError(QLatin1String("Unable to select fields from table ") + tableName(), QString(), QSqlError::StatementError));
			return stmt;
		}
        return NSql::concat(NSql::concat(stmt, NSql::where(filter())), orderByClause());
	}
};


char* dbConnectionName()
{
	return "library_db";
}

QString regEx()
{
	return "^\\s*(?<number>\\d+)\\s*-\\s*(?<realname>.*)\\.(?<ext>mp4|mkv|avi|m4v)";
}

class CFilterModel : public QSortFilterProxyModel 
{
public:
	CFilterModel(CSqlTableModel* parent) :
		QSortFilterProxyModel(parent),
		fSQLModel( parent ),
		fRegEx( regEx() )
	{
		setSourceModel(parent);
	}
	bool filterAcceptsRow(int source_row, const QModelIndex& /*source_parent*/) const
	{
		auto record = fSQLModel->record(source_row);
		auto fileName = record.value("Filename").toString();
		bool match  = fRegEx.match(fileName).hasMatch();
		return match;
	}
private:
	CSqlTableModel* fSQLModel{ nullptr };
	QRegularExpression fRegEx;
};

void CMainWindow::initModel()
{
    if (!QFileInfo::exists(fImpl->libraryFile->text()))
    {
        delete fModel;
        fModel = nullptr;
        return;
    }

    if ( fModel && ( fModel->database().databaseName() == fImpl->libraryFile->text() ) )
        return;

    delete fModel;
    fModel = new CSqlTableModel(fImpl->libraryFile->text(), this );
    
    fModel->select();
    
	fImpl->libraryView->setModel(fFilterModel = new CFilterModel(fModel));
}

CMainWindow::~CMainWindow()
{
    QSettings settings;
    settings.setValue( "LibraryFile", fImpl->libraryFile->text() );
}

void CMainWindow::slotSelectLibraryFile()
{
    auto libFile = QFileDialog::getOpenFileName( this, "Select Library.db", fImpl->libraryFile->text(), "Library Files (library.db)" );
    if (libFile.isEmpty() )
        return;

    fImpl->libraryFile->setText(libFile);
	initModel();
}

void CMainWindow::slotLibraryFileChanged()
{
    initModel();
}

void CMainWindow::slotAutoFix()
{
	while (fModel->canFetchMore())
		fModel->fetchMore();

	int rowCount = fFilterModel->rowCount();
	for (int ii = 0; ii < rowCount; ++ii )
	{
		updateRecord(ii);
	}
}

void CMainWindow::updateRecord(int ii)
{
	QRegularExpression regEx(::regEx());
	auto proxyIdx = fFilterModel->index(ii, 3);

	auto srcIdx = fFilterModel->mapToSource(proxyIdx);
	auto record = fModel->record(srcIdx.row());

	auto match = regEx.match(record.value("Filename").toString());
	bool changed = false;
	if (match.hasMatch())
	{
		auto baseName = match.captured("realname");
		auto num = match.captured("number").toInt();
		//auto ext = match.captured("ext").toInt();

		auto sortName = QString("%1 - %2").arg(num, 2, 10, QChar('0')).arg(baseName);

		record.setValue("Name", baseName);
		record.setValue("OriginalTitle", baseName);
		record.setValue("ForcedSortName", sortName);
		record.setValue("SortName", sortName);
		record.setValue("LockedFields", "Name|OriginalTitle|ForcedSortName|SortName");

		fModel->setRecord(srcIdx.row(), record);
	}
}

void CMainWindow::slotApply()
{
	fModel->submitAll();
}