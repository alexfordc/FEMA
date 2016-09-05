#ifndef USTP_MAIN_WINDOW_H
#define USTP_MAIN_WINDOW_H
#include <QtGui/QtGui>
#include "USTPOrderWidget.h"
#include "USTPTradeWidget.h"
#include "USTPositionWidget.h"
#include "USTPMarketWidget.h"
#include "USTPStrategyWidget.h"
#include "USTPCancelWidget.h"
#include "USTPTabWidget.h"

QT_BEGIN_NAMESPACE
class QToolBar;
class QAction;
class QTabWidget;
class QDockWidget;
QT_END_NAMESPACE

class USTPMainWindow : public QMainWindow
{
	Q_OBJECT

public:

	USTPMainWindow(QWidget * parent = 0);

	~USTPMainWindow();

private:
	void createPic();
	void createToolBar();
	void createActions();
	void createMenus();
	void createStatusBar();
	QDockWidget * createDockWidget(const QString& title, QWidget* pWidget, 
		Qt::DockWidgetAreas allowAreas = Qt::AllDockWidgetAreas, Qt::DockWidgetArea initArea = Qt::TopDockWidgetArea);

signals:
	void onUpdateKey(const int& bidKey, const int& askKey);

private slots:
	void about();
	void doCreateStrategy();
	void doSubscribeMarket();
	void doSaveStrategy();
	void doNewTab();
	void doNewKey();

private:
	void initConnect();

public slots:

	void doUSTPTradeFrontConnected();

	void doUSTPMdFrontConnected();

	void doUSTPTradeFrontDisconnected(int reason);

	void doUSTPMdFrontDisconnected(int reason);

	void doRemoveSubTab(int index);

	void doCreateNewKey(const int& bidKey, const int& askKey);
	
private:
	
	USTPTabWidget* mTabWidget;
	USTPMarketWidget* mMarketWidget;
	USTPOrderWidget* mOrderWidget;
	USTPTradeWidget* mTradeWidget;
	USTPositionWidget* mPositionWidget;
	USTPCancelWidget* mCancelWidget;
	
	QDockWidget* mMarketDockWidget;
	QDockWidget* mOrderDockWidget;
	QDockWidget* mTradeDockWidget;
	QDockWidget* mPositionDockWidget;
	QDockWidget* mCancelDockWidget;

	QToolBar* mToolBar;
	QAction* mHelpAction;
	QAction* mExitAction;
	QAction* mStrategyAction;
	QAction* mMarketAction;
	QAction* mNewTabAction;
	QAction* mSaveAction;
	QAction* mKeyAction;

	QMenu* mFileMenu;
	QMenu* mOperMenu;
	QMenu* mHelpMenu;

	QPixmap mLinkPic;
	QPixmap mDisconnectPic;
	QLabel* mTradeLinkLabel;
	QLabel* mMdLinkLabel;
	QLabel* mMessageLabel;

	QString mBrokerId;
	QString mUserId;
	QString mPsw;
	QIcon mTabIcon;
	int mBidKey;
	int mAskKey;
	int mTabIndex;
};

#endif
