#include <QtCore/QByteArray>
#include "USTPConfig.h"
#include "USTPMainWindow.h"
#include "USTPMdDialog.h"
#include "USTPKeyDialog.h"
#include "USTParamDialog.h"
#include "USTPCtpLoader.h"
#include "USTPMutexId.h"
#include "USTPMdApi.h"
#include "USTPTradeApi.h"
#include "USTPProfile.h"

USTPMainWindow::USTPMainWindow(QWidget * parent)
: QMainWindow(parent)
{	
	mTabIndex = -1;
	mBrokerId = USTPCtpLoader::getBrokerId();
	mUserId = USTPMutexId::getUserId();
	mPsw = USTPMutexId::getLoginPsw();
	createPic();
	createActions();
	createMenus();
	createToolBar();
	createStatusBar();

	mTabIcon = QIcon("../image/tab.png");
	
	mRspKeyDown = new USTPRspKeyDown;
	mKeyDownApi = KeyDownHock::CreateKeyHock();
	mKeyDownApi->Init();
	mKeyDownApi->RegisterSpi(mRspKeyDown);
	mMarketWidget = new USTPMarketWidget(this);
	mOrderWidget = new USTPOrderWidget(this);
	mTradeWidget = new USTPTradeWidget(this);
	mPositionWidget = new USTPositionWidget(this);
	mCancelWidget = new USTPCancelWidget(this);
	mTabWidget = new USTPTabWidget(this);
	mTabWidget->setTabsClosable(true);
		
	QString group, order, key, spread, actinNum;
	
	USTPProfile::readItem(tr("[LimitSpread]"), spread);
	QStringList spreadList = spread.split(",");
	for(int index = 0; index < spreadList.size(); index++){
		QString itemSpread = spreadList.at(index);
		QStringList item = itemSpread.split(":");
		USTPMutexId::setLimitSpread(item.at(0), item.at(1).toInt());
	}

	USTPProfile::readItem(tr("[ActionNum]"), actinNum);
	QStringList actionList = actinNum.split(",");
	for(int actionIndex = 0; actionIndex < actionList.size(); actionIndex++){
		QString itemAction = actionList.at(actionIndex);
		QStringList actionItem = itemAction.split(":");
		USTPMutexId::initActionNum(actionItem.at(0), actionItem.at(1).toInt());
	}
	
	USTPProfile::readItem(tr("[KeyItem]"), key);
	QStringList keyList = key.split("|");
	mBidKey = keyList.at(0).toInt();
	mAskKey = keyList.at(1).toInt();
	if(USTPProfile::readItem(tr("[GroupItem]"), group)){
		USTPProfile::readItem(tr("[TradeItem]"), order);
		QStringList groupList = group.split(",");
		QStringList orderList = order.split(":");
		for(int nIndex = 0; nIndex < groupList.size(); nIndex++){
			USTPStrategyWidget* pWidget = new USTPStrategyWidget(nIndex, mBidKey, mAskKey, mMarketWidget, mOrderWidget, mCancelWidget, mTabWidget, this);
			mTabWidget->addTab(pWidget, mTabIcon, groupList.at(nIndex));
			mTabWidget->setTabEnabled(nIndex, true);
			mTabIndex = nIndex;
			if(orderList.size() > nIndex){
				QString subTab = orderList.at(nIndex);
				pWidget->loadList(subTab);
			}
		}
	}
	mSpeMarketWidget = new USTPSpeMarketWidget(this);
	mSubmitWidget = new USTPSubmitWidget(mBidKey, mAskKey, mOrderWidget, mCancelWidget, mSpeMarketWidget, this);

	mMarketDockWidget = createDockWidget(MARKET_DOCK_WIDGET, mTabWidget, Qt:: AllDockWidgetAreas, Qt::TopDockWidgetArea);
	mOrderDockWidget = createDockWidget(ORDER_DOCK_WIDGET, mOrderWidget, Qt:: AllDockWidgetAreas, Qt::TopDockWidgetArea);
	mCancelDockWidget = createDockWidget(CANCEL_DOCK_WIDGET, mCancelWidget, Qt:: AllDockWidgetAreas, Qt::BottomDockWidgetArea);	
	mPositionDockWidget = createDockWidget(POSITION_DOCK_WIDGET, mPositionWidget, Qt:: AllDockWidgetAreas, Qt::BottomDockWidgetArea);
	mTradeDockWidget = createDockWidget(TRADE_DOCK_WIDGET, mTradeWidget, Qt:: AllDockWidgetAreas, Qt::BottomDockWidgetArea);
	
	QWidget* pWidget = new QWidget(this);
	QGridLayout* pLayOut = new QGridLayout;
	pLayOut->addWidget(mSpeMarketWidget, 0, 0, 8, 3);	
	pLayOut->addWidget(mSubmitWidget, 8, 0, 1, 3);
	pLayOut->addWidget(mMarketWidget, 0, 3, 9, 5);
	pWidget->setLayout(pLayOut);
	setCentralWidget(pWidget);
	initConnect();
	setWindowIcon(QIcon("../image/title.png"));
	setWindowTitle(MAIN_WINDOW_TILTE);
	showMaximized(); 
}

USTPMainWindow::~USTPMainWindow()
{	
	QString actionList;
	QMap<QString, int> actionNms;
	if (USTPMutexId::getTotalActionNum(actionNms)){
		QMapIterator<QString, int> i(actionNms);
		int nIndex = 0;
		while (i.hasNext()){
			i.next();
			QString item = i.key() + tr(":") + QString::number(i.value());
			if (nIndex == 0)
				actionList = item;
			else{
				actionList += tr(",");
				actionList += item;
			}
			nIndex++;
		}
		USTPProfile::writeItem(tr("[ActionNum]"), actionList);
	}
	
	if(mMarketWidget){
		delete mMarketWidget;
		mMarketWidget = NULL;
	}

	if(mOrderWidget){
		delete mOrderWidget;
		mOrderWidget = NULL;
	}

	if(mOrderDockWidget){
		delete mOrderDockWidget;
		mOrderDockWidget = NULL;
	}

	if(mTradeWidget){
		delete mTradeWidget;
		mTradeWidget = NULL;
	}

	if(mTradeDockWidget){
		delete mTradeDockWidget;
		mTradeDockWidget = NULL;
	}

	if(mPositionWidget){
		delete mPositionWidget;
		mPositionWidget = NULL;
	}

	if(mPositionDockWidget){
		delete mPositionDockWidget;
		mPositionDockWidget = NULL;
	}
	
	for(int nIndex = 0; nIndex < mTabWidget->count() - 1; nIndex++){
		USTPStrategyWidget* pWidget = qobject_cast<USTPStrategyWidget*>(mTabWidget->widget(nIndex));
		delete pWidget;
		pWidget = NULL;
	}
	
	if (mTabWidget){	
		delete mTabWidget;
		mTabWidget = NULL;
	}

	if(mCancelWidget){
		delete mCancelWidget;
		mCancelWidget = NULL;
	}

	if(mCancelDockWidget){
		delete mCancelDockWidget;
		mCancelDockWidget = NULL;
	}

	if (mMarketDockWidget){
		delete mMarketDockWidget;
		mMarketDockWidget = NULL;
	}

	if(mSpeMarketWidget){
		delete mSpeMarketWidget;
		mSpeMarketWidget = NULL;
	}

	if(mSubmitWidget){
		delete mSubmitWidget;
		mSubmitWidget = NULL;
	}
	if(mKeyDownApi){
		mKeyDownApi->Release();
		delete mKeyDownApi;
		mKeyDownApi = NULL;
	}
}

void USTPMainWindow::initConnect()
{	
	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPTradeFrontConnected()), this, SLOT(doUSTPTradeFrontConnected()));
	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPTradeFrontDisconnected(int)), this, SLOT(doUSTPTradeFrontDisconnected(int)));
	connect(USTPCtpLoader::getMdSpi(), SIGNAL(onUSTPMdFrontConnected()), this, SLOT(doUSTPMdFrontConnected()));
	connect(USTPCtpLoader::getMdSpi(), SIGNAL(onUSTPMdFrontDisconnected(int)), this, SLOT(doUSTPMdFrontDisconnected(int)));
	connect(mTabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(doRemoveSubTab(int))); 
	connect(mRspKeyDown, SIGNAL(onKeyDownHock(const int&)), mSubmitWidget, SLOT(doKeyDownHock(const int&))); 
	connect(mRspKeyDown, SIGNAL(onKeyDownHock(const int&)), mCancelWidget, SLOT(doKeyDownHock(const int&))); 
}

void USTPMainWindow::createPic()
{	
	QImage image(LINK_LABEL_WIDTH, LINK_LABEL_HEIGHT, QImage::Format_RGB32);
	image.load(QString("../image/red.png"));
	QImage scaleImage;
	if(image.width() > LINK_LABEL_WIDTH  || image.height() > LINK_LABEL_HEIGHT){
		scaleImage = image.scaled(QSize(LINK_LABEL_WIDTH, LINK_LABEL_HEIGHT));
	}
	mDisconnectPic = QPixmap::fromImage(scaleImage);
	
	image.load(QString("../image/green.png"));
	if(image.width() > LINK_LABEL_WIDTH  || image.height() > LINK_LABEL_HEIGHT){
		scaleImage = image.scaled(QSize(LINK_LABEL_WIDTH, LINK_LABEL_HEIGHT));
	}
	mLinkPic = QPixmap::fromImage(scaleImage);
}

void USTPMainWindow::createToolBar()
{	
	mToolBar = new QToolBar(this);
	mToolBar->setAllowedAreas(Qt::TopToolBarArea);
	mToolBar->setOrientation(Qt::Horizontal);
	mToolBar->setMovable(false);
	mToolBar->addAction(mMarketAction);
	mToolBar->addAction(mStrategyAction);	
	mToolBar->addAction(mNewTabAction);
	mToolBar->addAction(mKeyAction);
	mToolBar->addAction(mSaveAction);
	addToolBar(Qt::TopToolBarArea, mToolBar);
}	

void USTPMainWindow::createActions()
{
	mHelpAction = new QAction(QString(tr("关于(&A)")), this);
	mHelpAction->setShortcut(QKeySequence(QObject::tr("Ctrl+A")));
	
	mExitAction = new QAction(QIcon("../image/exit.png"), QString(tr("退出(&E)")), this);
	mExitAction->setShortcuts(QKeySequence::Quit);

	mStrategyAction = new QAction(QIcon("../image/event.png"), QString(tr("创建策略(&D)")), this);
	mStrategyAction->setShortcut(QKeySequence(QObject::tr("Ctrl+D")));

	mMarketAction = new QAction(QIcon("../image/md.png"), QString(tr("注册行情(&N)")), this);
	mMarketAction->setShortcut(QKeySequence(QObject::tr("Ctrl+N")));

	mNewTabAction = new QAction(QIcon("../image/subtab.png"), QString(tr("添加标签(&T)")), this);
	mNewTabAction->setShortcut(QKeySequence(QObject::tr("Ctrl+T")));

	mKeyAction = new QAction(QIcon("../image/key.png"), QString(tr("快捷按键(&S)")), this);
	mKeyAction->setShortcut(QKeySequence(QObject::tr("Ctrl+K")));

	mSaveAction = new QAction(QIcon("../image/save.png"), QString(tr("保存设置(&S)")), this);
	mSaveAction->setShortcut(QKeySequence(QObject::tr("Ctrl+S")));

	connect(mExitAction, SIGNAL(triggered()), qApp, SLOT(closeAllWindows()));
	connect(mHelpAction, SIGNAL(triggered()), this, SLOT(about()));
	connect(mStrategyAction, SIGNAL(triggered()), this, SLOT(doCreateStrategy()));
	connect(mMarketAction, SIGNAL(triggered()), this, SLOT(doSubscribeMarket()));
	connect(mNewTabAction, SIGNAL(triggered()), this, SLOT(doNewTab()));
	connect(mKeyAction, SIGNAL(triggered()), this, SLOT(doNewKey()));	
	connect(mSaveAction, SIGNAL(triggered()), this, SLOT(doSaveStrategy()));	
}

void USTPMainWindow::createMenus()
{
	mFileMenu = menuBar()->addMenu(tr("文件(&F)"));
	mFileMenu->addAction(mExitAction);

	mOperMenu = menuBar()->addMenu(tr("操作(&0)"));
	menuBar()->addSeparator();

	mHelpMenu = menuBar()->addMenu(tr("帮助(&H)"));
	mHelpMenu->addAction(mHelpAction);
}

void USTPMainWindow::createStatusBar()
{
	QStatusBar* bar = statusBar();
	mMessageLabel = new QLabel;
	mTradeLinkLabel = new QLabel;
	mTradeLinkLabel->setFixedSize(LINK_LABEL_WIDTH, LINK_LABEL_WIDTH);
	mTradeLinkLabel->setPixmap(mLinkPic);

	mMdLinkLabel = new QLabel;
	mMdLinkLabel->setFixedSize(LINK_LABEL_WIDTH, LINK_LABEL_WIDTH);
	mMdLinkLabel->setPixmap(mLinkPic);

	bar->addWidget(mMessageLabel, width() - 2 * LINK_LABEL_WIDTH);
	bar->addWidget(mMdLinkLabel);
	bar->addWidget(mTradeLinkLabel);
}

QDockWidget* USTPMainWindow::createDockWidget(const QString& title, QWidget* pWidget, Qt::DockWidgetAreas allowAreas, Qt::DockWidgetArea initArea)
{
	QDockWidget *dock = new QDockWidget(title, this);
	dock->setAllowedAreas(allowAreas);
	dock->setWidget(pWidget);
	addDockWidget(initArea, dock);
	mOperMenu->addAction(dock->toggleViewAction());
	return dock;
}

void USTPMainWindow::about()
{
	QMessageBox::about(this, tr("关于 套利交易系统"),
		tr("<b>程序化套利交易</b> 为一款基于飞马柜台的快速套利程序化交易系统。"));
}


void USTPMainWindow::doSubscribeMarket()
{	
	QString mdItem;
	USTPProfile::readItem(tr("[MdItem]"), mdItem);
	USTPMdDialog dlg(mMarketWidget, mSpeMarketWidget, mSubmitWidget, mdItem, this);
	dlg.exec();
}

void USTPMainWindow::doCreateStrategy()
{	
	USTPStrategyWidget* pWidget = qobject_cast<USTPStrategyWidget*>(mTabWidget->currentWidget());  
	if(pWidget != NULL){
		USTParamDialog dlg(pWidget, this);
		dlg.exec();
	}
}

void USTPMainWindow::doSaveStrategy()
{	
	int nMaxIndex = mTabWidget->count();
	if(nMaxIndex <= 0)
		return;
	QString groupList;
	for(int nIndex = 0; nIndex < nMaxIndex; nIndex++){
		QString tabName = mTabWidget->tabText(nIndex); 
		if (nIndex == 0){
			groupList = tabName;
		}else{
			groupList += tr(",");
			groupList += tabName;
		}
	}
	if(!USTPProfile::writeItem(tr("[GroupItem]"), groupList))
		ShowWarning(tr("组信息保存失败!"));
	QString itemList;
	for(int nIndex = 0; nIndex < nMaxIndex; nIndex++){
		USTPStrategyWidget* pWidget = qobject_cast<USTPStrategyWidget*>(mTabWidget->widget(nIndex));
		QString sublist = pWidget->getList();
		if(nIndex == 0){
			itemList = sublist;
		}else{
			itemList += tr(":");
			itemList += sublist;
		}
	}
	if(!USTPProfile::writeItem(tr("[TradeItem]"), itemList))
		ShowWarning(tr("报单列表项保存失败!"));
	ShowInfo(tr("列表信息保存成功."));
}

void  USTPMainWindow::doNewTab()
{	
	bool ok;
	QString groupName = QInputDialog::getText(this, tr("设置标签"),
		tr("组名:"), QLineEdit::Normal, tr("Tab"), &ok);
	if (ok && !groupName.isEmpty()){
		USTPStrategyWidget* pWidget = new USTPStrategyWidget(++mTabIndex, mBidKey, mAskKey, mMarketWidget, mOrderWidget, mCancelWidget, mTabWidget, this);
		mTabWidget->addTab(pWidget, mTabIcon, groupName);
	}
}

void  USTPMainWindow::doNewKey()
{
	USTPKeyDialog dlg(mBidKey, mAskKey, this);
	dlg.exec();
}

void USTPMainWindow::doUSTPTradeFrontConnected()
{
	mTradeLinkLabel->setPixmap(mLinkPic);
	USTPTradeApi::reqUserLogin(mBrokerId, mUserId, mPsw);
}

void USTPMainWindow::doUSTPTradeFrontDisconnected(int reason)
{
	mTradeLinkLabel->setPixmap(mDisconnectPic);
}

void USTPMainWindow::doUSTPMdFrontConnected()
{
	mMdLinkLabel->setPixmap(mLinkPic);
	USTPMdApi::reqUserLogin(mBrokerId, mUserId, mPsw);
}

void USTPMainWindow::doUSTPMdFrontDisconnected(int reason)
{
	mMdLinkLabel->setPixmap(mDisconnectPic);
}

void USTPMainWindow::doRemoveSubTab(int index)
{	
	USTPStrategyWidget* pWidget = qobject_cast<USTPStrategyWidget*>(mTabWidget->widget(index));
	int nCount = pWidget->rowCount();
	if(nCount > 0){
		ShowWarning(tr("当前标签报单列表数量不为0，不可移除."));
		return;
	}
	mTabWidget->removeTab(index);
	delete pWidget;
	pWidget = NULL;
}

void USTPMainWindow::doCreateNewKey(const int& bidKey, const int& askKey)
{	
	mBidKey = bidKey;
	mAskKey = askKey;
	emit onUpdateKey(bidKey, askKey);
}

#include "moc_USTPMainWindow.cpp"