#include "USTPLoginDialog.h"
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <QtCore/QThread>
#include <Windows.h>
#include "USTPMdApi.h"
#include "USTPTradeApi.h"
#include "USTPConfig.h"
#include "USTPMutexId.h"
#include "USTPCtpLoader.h"
#include "USTPLogger.h"

USTPLoginDialog::USTPLoginDialog(QWidget* parent)
:QDialog(parent)
{	
	mBrokerId = USTPCtpLoader::getBrokerId();
	mUserLabel = new QLabel(tr("用户账号:"));
	mPasswordLabel = new QLabel(tr("登录密码:"));
	mUserEdit = new QLineEdit(tr("90084353"));
	mPasswordEdit = new QLineEdit(tr("123456"));
	mPasswordEdit->setEchoMode(QLineEdit::Password);
	
	mLoginBtn =  new QPushButton(tr("登  录"));
    mCancelBtn = new QPushButton(tr("取  消"));

    mBtnLayout = new QHBoxLayout;	
	mBtnLayout->addWidget(mLoginBtn);
	mBtnLayout->addSpacing(20);
	mBtnLayout->addWidget(mCancelBtn);

	mGridLayout = new QGridLayout;
	mGridLayout->addWidget(mUserLabel, 0, 0, 1, 1);
	mGridLayout->addWidget(mUserEdit, 0, 1, 1, 2);
	mGridLayout->addWidget(mPasswordLabel, 1, 0, 1, 1);
	mGridLayout->addWidget(mPasswordEdit, 1, 1, 1, 2);

    mViewLayout = new QVBoxLayout;
	mViewLayout->setMargin(180);
    mViewLayout->addLayout(mGridLayout);
    mViewLayout->addStretch(5);
    mViewLayout->addSpacing(30);
    mViewLayout->addLayout(mBtnLayout);
    setLayout(mViewLayout);
	setWindowIcon(QIcon("../image/title.png"));
    setWindowTitle(LOGIN_WINDOW_TITLE);
	QDesktopWidget *pDesk = QApplication::desktop();
	resize(LOGIN_WINDOW_WIDTH, LOGIN_WINDOW_HEIGHT);
	move((pDesk->width() - width()) / 2, (pDesk->height() - height()) / 2);
	initConnect();
}

USTPLoginDialog::~USTPLoginDialog()
{
}

void USTPLoginDialog::initConnect()
{	
	connect(mLoginBtn, SIGNAL(clicked()), this, SLOT(doUserLogin()));
	connect(mCancelBtn, SIGNAL(clicked()), this, SLOT(close()));
	
	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPTradeRspUserLogin(const QString&, const QString& , const QString& , const int& , const int& , const QString&, bool)),
		this, SLOT(doUSTPTradeRspUserLogin(const QString&, const QString& , const QString& , const int& , const int& , const QString&, bool)));

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRspQryUserInvestor(const QString&, const QString&, const int&, const QString&, bool)),
		this, SLOT(doUSTPRspQryUserInvestor(const QString&, const QString&, const int&, const QString&, bool)));

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRspQryInstrument(const QString&, const QString&, const QString&, const char&, const double&, const int&, const int&, const int&, const QString&, bool)),
		this, SLOT(doUSTPRspQryInstrument(const QString&, const QString&, const QString&, const char&, const double&, const int&, const int&, const int&, const QString&, bool)));

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRspQryInvestorPosition(const QString&, const char&, const int&, const int&, const char&, const QString&, const QString&, 
		const QString&, const int&, const QString&, bool)), this, SLOT(doUSTPRspQryInvestorPosition(const QString&, const char&, const int&, const int&, const char&, const QString&, 
		const QString&, const QString&, const int&, const QString&, bool)));

	connect(USTPCtpLoader::getMdSpi(), SIGNAL(onUSTPMdRspUserLogin(const QString& , const QString& , const int& , const QString&, bool)),
		this, SLOT(doUSTPMdRspUserLogin(const QString& , const QString& , const int& , const QString&, bool)));
}

void USTPLoginDialog::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.drawPixmap(rect(), QPixmap("../image/background.png"));
}

void USTPLoginDialog::doUserLogin()
{	
	mUserId = mUserEdit->text();
	mPassword = mPasswordEdit->text();
	if ((!mUserId.isEmpty()) && (!mPassword.isEmpty())){
		QString authUser = USTPCtpLoader::getAuthUser();
		if(authUser != mUserId){
			ShowWarning(tr("用户无登录权限."));
			return;
		}

		int nMdResult = USTPMdApi::reqUserLogin(mBrokerId, mUserId, mPassword);
		if(nMdResult != 0){
			ShowWarning(tr("行情登录请求发送失败."));
		}
	}else{
		ShowWarning(tr("用户账号或密码为空."));
	}
}

void USTPLoginDialog::doUSTPMdRspUserLogin(const QString& brokerId, const QString& userId, const int& errorId, const QString& errorMsg, bool bIsLast)
{
	if (errorId >= 0){
		int nTradeResult = USTPTradeApi::reqUserLogin(mBrokerId, mUserId, mPassword);
		if(nTradeResult != 0)
			ShowWarning(tr("交易登录请求发送失败."));
	}
}

void USTPLoginDialog::doUSTPTradeRspUserLogin(const QString& tradingDate, const QString& brokerId, const QString& userId, const int& maxLocalId, const int& errorId, const QString& errorMsg, bool bIsLast)
{
	if (errorId == 0){
		QDateTime current_date_time = QDateTime::currentDateTime();
		QString current_date = current_date_time.toString("yyyyMMdd");
		if(tradingDate.toInt() > current_date.toInt()){
			ShowWarning(tr("系统时间不合法."));
			return;
		}
		USTPMutexId::initialize(userId, mPassword, maxLocalId);
		int nResult = USTPTradeApi::reqQryInvestor(brokerId, userId);
		if(nResult != 0)
			ShowWarning(tr("查询客户请求发送失败."));
	}else
		ShowWarning(errorMsg);
}

void USTPLoginDialog::doUSTPRspQryUserInvestor(const QString& userId, const QString& investorId, const int& errorId, const QString& errorMsg, bool bIsLast)
{
	if (errorId == 0){
		USTPMutexId::setInvestorId(investorId);
		if(bIsLast){
			::Sleep(1000);
			int nResult = USTPTradeApi::reqQryInstrument("");
			if(nResult != 0)
				ShowWarning(tr("查询合约请求发送失败."));
		}
	}else
		ShowWarning(errorMsg);
}

void USTPLoginDialog::doUSTPRspQryInstrument(const QString& exchangeId, const QString& productId, const QString& instrumentId, const char& instrumentStatus, const double& priceTick, 
							const int& volumeMultiple, const int& maxMarketVolume, const int& errorId, const QString& errorMsg, bool bIsLast)
{	
	if (errorId == 0){
		USTPMutexId::setInsPriceTick(instrumentId, priceTick);
		USTPMutexId::setInsMarketMaxVolume(instrumentId, maxMarketVolume);
		if(productId == tr("IF") && maxMarketVolume > 0)
			USTPMutexId::setReferenceIns(instrumentId);
		if(bIsLast){
			::Sleep(1000);
			int nResult = USTPTradeApi::reqQryInvestorPosition(mBrokerId, USTPMutexId::getInvestorId(), "");
			//if (nResult != 0)
			//	ShowWarning(tr("查询投资者持仓请求发送失败."));
		}

	}else
		ShowWarning(errorMsg);
}

void  USTPLoginDialog::doUSTPRspQryInvestorPosition(const QString& instrumentId, const char& direction, const int& position, const int& yPosition, const char& hedgeFlag,
								  const QString& brokerId, const QString& exchangeId, const QString& investorId, const int& errorId, const QString& errorMsg, bool bIsLast)
{
	if (errorId == 0){
		if(direction == '0' && instrumentId != ""){
			if (position > 0) 
				USTPMutexId::setInsBidPosition(instrumentId, position);
		}else if(direction == '1' && instrumentId != ""){
			if (position > 0) 
				USTPMutexId::setInsAskPosition(instrumentId, position);
		}
		if(bIsLast){
			accept();
		}
	}else
		ShowWarning(errorMsg);
}

#include "moc_USTPLoginDialog.cpp"
