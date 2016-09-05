#include "USTPUserStrategy.h"
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <Windows.h>
#include "USTPConfig.h"
#include "USTPLogger.h"
#include "USTPCtpLoader.h"
#include "USTPMutexId.h"
#include "USTPTradeApi.h"
#include "USTPStrategyWidget.h"
#include "USTPSubmitWidget.h"

#define USTP_FTDC_OS_ORDER_SUBMIT 'O'
#define USTP_FTDC_OS_CANCEL_SUBMIT 'C'
#define USTP_FTDC_OS_ORDER_NO_ORDER 'N'
#define USTP_FTDC_OS_ORDER_ERROR 'E'

#define FIRST_INSTRUMENT tr("FirstInstrument")
#define SECOND_INSTRUMENT tr("SecondInstrument")
#define DEFINE_ORDER_TIME 10
#define CANCEL_NUM 350

USTPStrategyBase::USTPStrategyBase(const QString& orderLabel, const QString& firstIns, const QString& secIns, const double& orderPriceTick, const int& qty, const char& bs,  const char& offset,
								   const char& hedge, const int& cancelFirstTime, const int& cancelSecTime, const int& cycleStall, const int& firstSlipPoint, const int& secSlipPoint, bool isAutoFirstCancel,
								   bool isAutoSecCancel, bool isCycle)
{	
	mOrderLabel = orderLabel;
	mOrderPrice = orderPriceTick;
	mFirstCancelTime = cancelFirstTime;
	mSecondCancelTime = cancelSecTime;
	mFirstSlipPoint = firstSlipPoint;
	mSecSlipPoint = secSlipPoint;
	mIsAutoFirstCancel = isAutoFirstCancel;
	mIsAutoSecondCancel = isAutoSecCancel;
	mOrderQty = qty;
	mCycleStall = cycleStall;
	mFirstIns = firstIns;
	mSecIns = secIns;
	mOffsetFlag = offset;
	mHedgeFlag = hedge;
	mBS = bs;
	mIsCycle = isCycle;
	mIsDeleted = false;
	mInsComplex = mFirstIns + tr("|") + mSecIns;
	mPriceTick = USTPMutexId::getInsPriceTick(firstIns);
	mInsPrecision = getInsPrcision(mPriceTick);
}

USTPStrategyBase::~USTPStrategyBase()
{

}

bool USTPStrategyBase::isInMarket(const char& status)
{
	if (USTP_FTDC_OS_PartTradedQueueing == status || USTP_FTDC_OS_PartTradedNotQueueing == status ||
		USTP_FTDC_OS_NoTradeQueueing == status || USTP_FTDC_OS_NoTradeNotQueueing == status){
			return true;
	}
	return false;
}

int USTPStrategyBase::getInsPrcision(const double& value)
{
	if (value >= VALUE_1){
		return 0;
	}else if(value * 10 >= VALUE_1){
		return 1;
	}else if(value * 100 >= VALUE_1){
		return 2;
	}else if(value * 1000 >= VALUE_1){
		return 3;
	}
	return 0;
}


USTPStarePriceArbitrage::USTPStarePriceArbitrage(const QString& orderLabel, const QString& firstIns, const QString& secIns, const double& orderPriceTick, const int& secPriceSlipPoint, const int& qty, const char& bs,  const char& offset, 
												 const char& hedge, const int& cancelFirstTime, const int& cancelSecTime, const int& cycleStall, bool isAutoFirstCancel, bool isAutoSecCancel, bool isCycle, USTPOrderWidget* pOrderWidget, 
												 USTPCancelWidget* pCancelWidget, USTPStrategyWidget* pStrategyWidget)
												 :USTPStrategyBase(orderLabel, firstIns, secIns, orderPriceTick, qty, bs, offset, hedge, cancelFirstTime, cancelSecTime, cycleStall, 0, secPriceSlipPoint, isAutoFirstCancel, isAutoSecCancel, isCycle)
{	
	moveToThread(&mStrategyThread);
	mStrategyThread.start();
	mFirstMarketBasePrice = 0.0;
	mFirstOrderBasePrice = 0.0;
	mFirstMarketOldBasePrice = 0.0;
	mFirstTradeQty = 0;
	mFirstRemainQty = mOrderQty;
	mSecondTradeQty = 0;
	mOrderType = 0;
	mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mSecInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mSecondSlipPrice = secPriceSlipPoint * mPriceTick;
	mStrategyLabel = "FollowPriceArbitrage_";
	mBrokerId = USTPCtpLoader::getBrokerId();
	mUserId = USTPMutexId::getUserId();
	mInvestorId = USTPMutexId::getInvestorId();
	mLimitSpread = USTPMutexId::getLimitSpread(mInsComplex) * mPriceTick;
	mIsCanMarket = (USTPMutexId::getInsMarketMaxVolume(secIns) > 0) ? true : false;
	initConnect(pStrategyWidget, pOrderWidget, pCancelWidget);	
	updateInitShow();
}

USTPStarePriceArbitrage::~USTPStarePriceArbitrage()
{
	mStrategyThread.quit();
	mStrategyThread.wait();
}

void USTPStarePriceArbitrage::initConnect(USTPStrategyWidget* pStrategyWidget, USTPOrderWidget* pOrderWidget, USTPCancelWidget* pCancelWidget)
{
	connect(USTPCtpLoader::getMdSpi(), SIGNAL(onUSTPRtnDepthMarketData(const QString&, const double&, const double&, const double&, const double&, const int&, const double&, const int&, const double&, const double&, const int&)), 
		this, SLOT(doUSTPRtnDepthMarketData(const QString&, const double&, const double&, const double&, const double&, const int&, const double&, const int&, const double&, const double&, const int&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)),
		this, SLOT(doUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)),
		this, SLOT(doUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pOrderWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pCancelWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(pCancelWidget, SIGNAL(onDelOrder(const QString& )), this, SLOT(doDelOrder(const QString& )), Qt::QueuedConnection);

	connect(this, SIGNAL(onOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), pStrategyWidget, SLOT(doOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, 
		const int&, const int&, const int&, const int&, const int&, bool, bool,bool, bool, bool, bool, bool, const int&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool, bool, bool, bool, bool, bool, const int&)), pCancelWidget, SLOT(doOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool, bool, bool, bool, bool, bool, const int&)), Qt::QueuedConnection);
}

void USTPStarePriceArbitrage::updateInitShow()
{
	mUserCustomLabel = mStrategyLabel + QString::number(USTPMutexId::getMutexId());
	emit onUpdateOrderShow(mUserCustomLabel, mFirstIns, mOrderLabel, 'N', mBS, 0.0, mOrderQty, mOrderQty, 0, mOffsetFlag, USTP_FTDC_OPT_LimitPrice, mHedgeFlag, mOrderPrice);
}

void USTPStarePriceArbitrage::doUSTPRtnDepthMarketData(const QString& instrumentId, const double& preSettlementPrice, const double& openPrice, const double& lastPrice,
													   const double& bidPrice, const int& bidVolume, const double& askPrice, const int& askVolume, const double& highestPrice, 
													   const double& lowestPrice, const int& volume)
{	
	if (mSecIns != instrumentId)	//监听第二腿行情
		return;
	mFirstMarketOldBasePrice = mFirstMarketBasePrice;
	if(USTP_FTDC_D_Buy == mBS)//记录实时第二腿的行情，作为第一腿报单时的参考	
		mFirstMarketBasePrice = bidPrice;
	else
		mFirstMarketBasePrice = askPrice;

	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){	//第一腿初次或者设定时间撤单后报单	
		double price = 0.0;
		if(USTP_FTDC_D_Buy == mBS){
			if(mFirstMarketBasePrice > mFirstMarketOldBasePrice + mLimitSpread)
				return;
			price = bidPrice + mOrderPrice;
			mFirstOrderBasePrice = bidPrice;
		}else{
			if(mFirstMarketOldBasePrice > mFirstMarketBasePrice + mLimitSpread)
				return;
			price = askPrice + mOrderPrice;
			mFirstOrderBasePrice = askPrice;
		}	
		if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus){
			if(mIsDeleted)
				return;
			orderInsert(mUserCustomLabel, FIRST_INSTRUMENT, mFirstIns, price, mBS, mFirstRemainQty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, true);
		}else
			submitOrder(FIRST_INSTRUMENT, mFirstIns, price, mBS, mFirstRemainQty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, true);

	}else if(USTPStrategyBase::isInMarket(mFirstInsStatus)){	//1.非设定时间撤单的情况，第一腿委托成功，第二腿行情发生变化，腿一撤单重发;2.设定时间撤单的情况，定时超时，根据行情触发。
		if((USTP_FTDC_D_Buy == mBS && ((bidPrice > mFirstOrderBasePrice) || (bidPrice < mFirstOrderBasePrice))) ||
			(USTP_FTDC_D_Sell == mBS && ((askPrice > mFirstOrderBasePrice) || (askPrice < mFirstOrderBasePrice)))){
				mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
				submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
		}
	}
}

void USTPStarePriceArbitrage::submitOrder(const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	if(isFirstIns && mIsDeleted)	//撤掉报单，合约一禁止下新单
		return;
	mUserCustomLabel = mStrategyLabel + QString::number(USTPMutexId::getMutexId());
	orderInsert(mUserCustomLabel, insLabel, instrument, orderPrice, direction, qty, priceType, timeCondition, isFirstIns);	
}

void USTPStarePriceArbitrage::orderInsert(const QString& insCustom, const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	if(USTPMutexId::getActionNum(mInsComplex) >= CANCEL_NUM)
		return;
	double adjustPrice = (priceType == USTP_FTDC_OPT_LimitPrice) ? orderPrice : 0.0;
	QString orderRef;
	USTPTradeApi::reqOrderInsert(insCustom, mBrokerId, mUserId, mInvestorId, instrument, priceType, timeCondition, adjustPrice, qty, direction, mOffsetFlag, mHedgeFlag, USTP_FTDC_VC_AV, orderRef);
	if(isFirstIns){
		mFirstInsStatus = USTP_FTDC_OS_ORDER_SUBMIT;
		mFirstOrderRef = orderRef;
	}else{
		mSecondRemainQty = qty;
		mSecInsStatus = USTP_FTDC_OS_ORDER_SUBMIT;
		mSecOrderRef = orderRef;
	}	
	emit onUpdateOrderShow(insCustom, instrument, mOrderLabel, 'N', direction, 0.0, qty, qty, 0, mOffsetFlag, priceType, mHedgeFlag, mOrderPrice);

	if(mIsAutoFirstCancel && isFirstIns)
		QTimer::singleShot(mFirstCancelTime, this, SLOT(doAutoCancelFirstIns()));
	else if(mIsAutoSecondCancel && isFirstIns == false){
		QTimer::singleShot(mSecondCancelTime, this, SLOT(doAutoCancelSecIns()));
	}
	//条件日志
#ifdef _DEBUG
	int nIsSecCancel = mIsAutoSecondCancel ? 1 : 0;
	int nIsSecIns = isFirstIns ? 0 : 1;
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderInsert]   Instrument: ")) + instrument + QString(tr("  UserCustom: ")) + insCustom + 
		QString(tr("  UserId: ")) + mUserId + QString(tr("  PriceType: ")) + QString(priceType) + QString(tr("  OrderPrice: ")) + QString::number(adjustPrice) + QString(tr("  OrderVolume: ")) + 
		QString::number(qty) + QString(tr("  Direction: ")) + QString(direction) + QString(tr("  SecAutoCancel: ")) + QString::number(nIsSecCancel) + QString(tr("  IsSecIns: ")) + QString::number(nIsSecIns);
	USTPLogger::saveData(data);
#endif	
}

void USTPStarePriceArbitrage::doUSTPRtnOrder(const QString& userCustom, const QString& userOrderLocalId, const QString& instrumentId, const char& direction, const double& orderPrice, const int& orderVolume,
											 const int& remainVolume, const int& tradeVolume, const char& offsetFlag, const char& priceType, const char& hedgeFlag, const char& orderStatus,
											 const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId, const QString& seatId, const char& timeCondition)
{	
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, orderStatus, direction, orderPrice, orderVolume, remainVolume, tradeVolume, offsetFlag, priceType, hedgeFlag, mOrderPrice);
	if(mFirstOrderRef == userOrderLocalId){
		mFirstInsStatus = orderStatus;
		mFirstRemainQty = remainVolume;

		if (USTP_FTDC_OS_Canceled == orderStatus && (false == mIsAutoFirstCancel)){
			double price = mFirstMarketBasePrice + mOrderPrice;
			mFirstOrderBasePrice = mFirstMarketBasePrice;
			submitOrder(FIRST_INSTRUMENT, mFirstIns, price, mBS, remainVolume, priceType, USTP_FTDC_TC_GFD, true);
		}

		if (USTP_FTDC_OS_Canceled == orderStatus && mIsDeleted){
			emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
				mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, false, mIsActionReferTick, mOrderType);
		}

		if(USTP_FTDC_OS_Canceled == orderStatus)
			USTPMutexId::updateActionNum(instrumentId);
	}else{
		mSecInsStatus = orderStatus;
		mSecondRemainQty = remainVolume;
		if(USTP_FTDC_OS_Canceled == orderStatus && remainVolume > 0){
			orderSecondIns(false, remainVolume, orderPrice, orderPrice);
		}
	}
}


void USTPStarePriceArbitrage::doUSTPRtnTrade(const QString& tradeId, const QString& instrumentId, const char& direction, const int& tradeVolume, const double& tradePrice,
											 const char& offsetFlag, const char& hedgeFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId,
											 const QString& seatId, const QString& userOrderLocalId, const QString& tradeTime)
{	
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	if(mFirstOrderRef == userOrderLocalId){
		mFirstTradeQty += tradeVolume;
		orderSecondIns(true, tradeVolume, 0.0, 0.0);
	}else{
		mSecondTradeQty += tradeVolume;
	}
	if(mFirstTradeQty >= mOrderQty && mSecondTradeQty >= mOrderQty){
		emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
			mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, true, mIsActionReferTick, mOrderType);
	}
}

void USTPStarePriceArbitrage::orderSecondIns(bool isInit, const int& qty, const double& bidPrice, const double& askPrice)
{	
	if (mIsCanMarket){
		if (USTP_FTDC_D_Buy == mBS)
			submitOrder(SECOND_INSTRUMENT, mSecIns, 0.0, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_BestPrice, USTP_FTDC_TC_GFD, false);
		else
			submitOrder(SECOND_INSTRUMENT, mSecIns, 0.0, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_BestPrice, USTP_FTDC_TC_GFD, false);
	}else{
		if(USTP_FTDC_D_Buy == mBS){
			if(isInit){
				double initPrice = mFirstOrderBasePrice - mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, initPrice, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}else{
				double price = bidPrice - mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, price, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}
		}else{
			if(isInit){
				double initPrice = mFirstOrderBasePrice + mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, initPrice, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}else{
				double price = askPrice + mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, price, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}
		}
	}
}

void USTPStarePriceArbitrage::doUSTPErrRtnOrderInsert(const QString& userCustom, const QString& brokerId, const char& direction, const QString& exchangeId, const char& hedgeFlag,
													  const QString& instrumentId, const QString& investorId, const char& offsetFlag, const char& priceType, const char& timeCondition,
													  const QString& userOrderLocalId, const double& orderPrice, const int& volume, const int& errorId, const QString& errorMsg)
{	
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	if(mFirstOrderRef == userOrderLocalId)	//设置合约状态
		mFirstInsStatus = USTP_FTDC_OS_ORDER_ERROR;
	else
		mSecInsStatus = USTP_FTDC_OS_ORDER_ERROR;

	switch (errorId){
	case 12:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'D', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice); //重复的报单
		break;
	case 31:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'P', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//平仓位不足
		break;
	case 36:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'Z', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//	资金不足
		break;
	case 8129:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'J', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//交易编码不存在
		break;
	default:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'W', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);
		break;
	}
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderInsert] UserCustom: ")) + userCustom +  QString(tr("  UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}

void USTPStarePriceArbitrage::doUSTPErrRtnOrderAction(const char& actionFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId,
													  const QString& orderSysId, const QString& userActionLocalId, const QString& userOrderLocalId, const double& orderPrice, 
													  const int& volumeChange, const int& errorId, const QString& errorMsg)
{
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderAction] UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  UserActionLocalId: ")) + userActionLocalId  + QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}


void USTPStarePriceArbitrage::doAutoCancelFirstIns()
{	
	if(isInMarket(mFirstInsStatus)){
		mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
		submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
	}
}

void USTPStarePriceArbitrage::doAutoCancelSecIns()
{	
	if(isInMarket(mSecInsStatus)){
		mSecInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
		submitAction(SECOND_INSTRUMENT, mSecOrderRef, mSecIns);
	}
}

void USTPStarePriceArbitrage::doDelOrder(const QString& orderStyle)
{
	if(orderStyle == mOrderLabel){
		mIsDeleted = true;
		if(isInMarket(mFirstInsStatus)){
			mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
			submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
		}else if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus || USTP_FTDC_OS_ORDER_ERROR == mFirstInsStatus || USTP_FTDC_OS_Canceled == mFirstInsStatus){
			emit onUpdateOrderShow(mUserCustomLabel, mFirstIns, mOrderLabel, USTP_FTDC_OS_Canceled, mBS, 0.0, mOrderQty, mOrderQty, 0, mOffsetFlag, USTP_FTDC_OPT_LimitPrice, mHedgeFlag, mOrderPrice);
			emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
				mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, false, mIsActionReferTick, mOrderType);
		}
		QString data = mOrderLabel + QString(tr("  [DoDelOrder]   mFirstInsStatus: ")) + QString(mFirstInsStatus);
		USTPLogger::saveData(data);
	}
}

void USTPStarePriceArbitrage::submitAction(const QString& insLabel, const QString& orderLocalId, const QString& instrument)
{	
	USTPTradeApi::reqOrderAction(mBrokerId, mUserId, mInvestorId, instrument, orderLocalId);
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderAction]   UserLocalOrderId: ")) + orderLocalId + QString(tr("  InstrumentId: ")) + instrument;
	USTPLogger::saveData(data);
#endif	
}


USTPConditionArbitrage::USTPConditionArbitrage(const QString& orderLabel, const QString& firstIns, const QString& secIns, const double& orderPriceTick, const int& firstPriceSlipPoint, const int& secPriceSlipPoint, 
											   const int& qty, const char& bs,  const char& offset, const char& hedge, const int& cancelFirstTime, const int& cancelSecTime, const int& cycleStall, const int& actionReferNum, 
											   const int& actionSuperNum, bool isAutoFirstCancel, bool isAutoSecCancel, bool isCycle, bool isOppentPrice, bool isDefineOrder, bool isReferTick, USTPOrderWidget* pOrderWidget,USTPCancelWidget* pCancelWidget, 
											   USTPStrategyWidget* pStrategyWidget) :USTPStrategyBase(orderLabel, firstIns, secIns, orderPriceTick, qty, bs, offset, hedge, cancelFirstTime, cancelSecTime, cycleStall, firstPriceSlipPoint,
											   secPriceSlipPoint, isAutoFirstCancel, isAutoSecCancel, isCycle)
{	
	moveToThread(&mStrategyThread);
	mStrategyThread.start();
	mFirstMarketBasePrice = 0.0;
	mSecondMarketBasePrice = 0.0;
	mSecondOrderBasePrice = 0.0;
	mFirstOrderPrice = 0.0;
	mFirstBidMarketPrice = 0.0;
	mFirstAskMarketPrice = 0.0;
	mOrderType = 1;
	mFirstTradeQty = 0;
	mFirstRemainQty = mOrderQty;
	mSecondTradeQty = 0;
	mFirstBidMarketVolume = 0;
	mFirstAskMarketVolume = 0;
	mCurrentReferIndex = 0;
	mActionReferNum = actionReferNum;
	mActionSuperNum = actionSuperNum;
	mIsDefineOrder = isDefineOrder;
	mIsOppnentPrice = isOppentPrice;
	mIsActionReferTick = isReferTick;
	mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mSecInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mFirstSlipPrice = firstPriceSlipPoint * mPriceTick;
	mSecondSlipPrice = secPriceSlipPoint * mPriceTick;
	mActionSuperSlipPrice = actionSuperNum * mPriceTick;
	mStrategyLabel = "ConditionArbitrage_";
	mBrokerId = USTPCtpLoader::getBrokerId();
	mUserId = USTPMutexId::getUserId();
	mInvestorId = USTPMutexId::getInvestorId();
	mReferenceIns = USTPMutexId::getReferenceIns();
	mIsCanMarket = (USTPMutexId::getInsMarketMaxVolume(secIns) > 0) ? true : false;
	initConnect(pStrategyWidget, pOrderWidget, pCancelWidget);	
	updateInitShow();
}

USTPConditionArbitrage::~USTPConditionArbitrage()
{
	mStrategyThread.quit();
	mStrategyThread.wait();
}

void USTPConditionArbitrage::initConnect(USTPStrategyWidget* pStrategyWidget, USTPOrderWidget* pOrderWidget, USTPCancelWidget* pCancelWidget)
{
	connect(USTPCtpLoader::getMdSpi(), SIGNAL(onUSTPRtnDepthMarketData(const QString&, const double&, const double&, const double&, const double&, const int&, const double&, const int&, const double&, const double&, const int&)), 
		this, SLOT(doUSTPRtnDepthMarketData(const QString&, const double&, const double&, const double&, const double&, const int&, const double&, const int&, const double&, const double&, const int&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)),
		this, SLOT(doUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)),
		this, SLOT(doUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pOrderWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pCancelWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(pCancelWidget, SIGNAL(onDelOrder(const QString& )), this, SLOT(doDelOrder(const QString& )), Qt::QueuedConnection);

	connect(this, SIGNAL(onOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), pStrategyWidget, SLOT(doOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), pCancelWidget, SLOT(doOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), Qt::QueuedConnection);
}

void USTPConditionArbitrage::updateInitShow()
{	
	if(mIsDeleted)	//如果手工撤单，则不更新
		return;
	mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mUserCustomLabel = mStrategyLabel + QString::number(USTPMutexId::getMutexId());
	emit onUpdateOrderShow(mUserCustomLabel, mFirstIns, mOrderLabel, 'N', mBS, 0.0, mFirstRemainQty, mFirstRemainQty, mFirstTradeQty, mOffsetFlag, USTP_FTDC_OPT_LimitPrice, mHedgeFlag, mOrderPrice);
}

void USTPConditionArbitrage::doUSTPRtnDepthMarketData(const QString& instrumentId, const double& preSettlementPrice, const double& openPrice, const double& lastPrice,
													  const double& bidPrice, const int& bidVolume, const double& askPrice, const int& askVolume, const double& highestPrice, 
													  const double& lowestPrice, const int& volume)
{	
	if(mIsDefineOrder && mReferenceIns == instrumentId)
		QTimer::singleShot(DEFINE_ORDER_TIME, this, SLOT(doDefineTimeOrderFirstIns()));

	if ((mFirstIns != instrumentId) &&(mSecIns != instrumentId))	//监听第一，二腿行情
		return;
	if(USTP_FTDC_D_Buy == mBS && mFirstIns == instrumentId)
		mFirstMarketBasePrice = bidPrice;
	else if(USTP_FTDC_D_Sell == mBS && mFirstIns == instrumentId)
		mFirstMarketBasePrice = askPrice;
	else if(USTP_FTDC_D_Buy == mBS && mSecIns == instrumentId)
		mSecondMarketBasePrice = bidPrice;
	else if(USTP_FTDC_D_Sell == mBS && mSecIns == instrumentId)
		mSecondMarketBasePrice = askPrice;
	if(mFirstIns == instrumentId){
		mFirstBidMarketPrice = bidPrice;
		mFirstAskMarketPrice = askPrice;
		mFirstBidMarketVolume = bidVolume;
		mFirstAskMarketVolume = askVolume;
	}
	if((mFirstMarketBasePrice < 0.1) || (mSecondMarketBasePrice < 0.1) || (mFirstBidMarketPrice < 0.1) || (mFirstAskMarketPrice < 0.1))	//保证两腿都收到行情
		return;
	double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;
	if(mIsOppnentPrice)
		opponentPriceOrder(fSecBasePrice);
	else
		noOpponentPriceOrder(instrumentId, fSecBasePrice);
}

void USTPConditionArbitrage::opponentPriceOrder(const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单
		if(USTP_FTDC_D_Buy == mBS && mFirstAskMarketPrice <= basePrice){	//买委托满足下单条件
			mFirstOrderPrice = mFirstAskMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_IOC);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstBidMarketPrice >= basePrice){//卖委托满足下单条件
			mFirstOrderPrice = mFirstBidMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_IOC);
		}else{
			if(USTP_FTDC_OS_Canceled == mFirstInsStatus){		
				updateInitShow();
			}
		}
	}
}

void USTPConditionArbitrage::noOpponentPriceOrder(const QString& instrument, const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单

		if(USTP_FTDC_D_Buy == mBS && mFirstMarketBasePrice <= basePrice){	//条件单买委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice + mFirstSlipPrice;
			if(mFirstOrderPrice > basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstMarketBasePrice >= basePrice){//条件单卖委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice - mFirstSlipPrice;
			if(mFirstOrderPrice < basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else{
			if(USTP_FTDC_OS_Canceled == mFirstInsStatus){
				updateInitShow();
			}
		}
	}else if(USTPStrategyBase::isInMarket(mFirstInsStatus)){//1.非设定时间撤单的情况，第一腿委托成功，第一腿行情发生变化，腿一撤单重发;2.设定时间撤单的情况，定时超时，根据行情触发。
		if(mIsActionReferTick && (mFirstIns == instrument)){
			if(USTP_FTDC_D_Buy == mBS){
				if(mFirstMarketBasePrice > mFirstOrderPrice)
					mCurrentReferIndex++;
				else
					mCurrentReferIndex = 0;
			}else{
				if(mFirstMarketBasePrice < mFirstOrderPrice)
					mCurrentReferIndex++;
				else
					mCurrentReferIndex = 0;
			}
		}else if(mIsActionReferTick && (mSecIns == instrument)){
			if(mCurrentReferIndex > 0)
				mCurrentReferIndex++;
		}

		double actionBidBasePrice = basePrice + mActionSuperSlipPrice;
		double actionAskBasePrice = basePrice - mActionSuperSlipPrice;

		if((mSecIns == instrument) && ((USTP_FTDC_D_Buy == mBS && (mFirstOrderPrice > actionBidBasePrice)) || (USTP_FTDC_D_Sell == mBS && (mFirstOrderPrice < actionAskBasePrice)))){
			cancelFirstIns();
		}else if((mSecIns == instrument) && ((USTP_FTDC_D_Buy == mBS && (mFirstMarketBasePrice > mFirstOrderPrice) && (mFirstMarketBasePrice < basePrice)) || 
			(USTP_FTDC_D_Sell == mBS && (mFirstMarketBasePrice < mFirstOrderPrice) && (mFirstMarketBasePrice > basePrice)))){
				cancelFirstIns();
		}else if((USTP_FTDC_D_Buy == mBS && (mIsActionReferTick && (mCurrentReferIndex >= mActionReferNum)) && (mFirstMarketBasePrice < basePrice)) || 
			(USTP_FTDC_D_Sell == mBS && (mIsActionReferTick && (mCurrentReferIndex >= mActionReferNum)) && (mFirstMarketBasePrice > basePrice))){
				cancelFirstIns();
		}else if((mFirstMarketBasePrice == mFirstOrderPrice) && ((USTP_FTDC_D_Buy == mBS && (mFirstOrderPrice < basePrice) && (mFirstRemainQty < mFirstBidMarketVolume)) || 
			(USTP_FTDC_D_Sell == mBS && (mFirstOrderPrice > basePrice) && (mFirstRemainQty < mFirstAskMarketVolume)))){
				if (USTP_FTDC_D_Buy == mBS)
					mFirstMarketBasePrice += mPriceTick;
				else
					mFirstMarketBasePrice -= mPriceTick;	
				cancelFirstIns();
		}
	}
}

void USTPConditionArbitrage::defineTimeOrder(const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单

		if(USTP_FTDC_D_Buy == mBS && mFirstMarketBasePrice <= basePrice){	//条件单买委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice + mFirstSlipPrice;
			if(mFirstOrderPrice > basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstMarketBasePrice >= basePrice){//条件单卖委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice - mFirstSlipPrice;
			if(mFirstOrderPrice < basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else{
			if(USTP_FTDC_OS_Canceled == mFirstInsStatus){
				updateInitShow();
			}
		}
	}
}

void USTPConditionArbitrage::switchFirstInsOrder(const char& tCondition)
{
	if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus){
		if(mIsDeleted)
			return;
		orderInsert(mUserCustomLabel, FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, USTP_FTDC_OPT_LimitPrice, tCondition, true);
	}else
		submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, USTP_FTDC_OPT_LimitPrice, tCondition, true);
}

void USTPConditionArbitrage::submitOrder(const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	if(isFirstIns && mIsDeleted)	//撤掉报单，合约一禁止下新单
		return;
	mUserCustomLabel = mStrategyLabel + QString::number(USTPMutexId::getMutexId());
	orderInsert(mUserCustomLabel, insLabel, instrument, orderPrice, direction, qty, priceType, timeCondition, isFirstIns);	
}

void USTPConditionArbitrage::orderInsert(const QString& insCustom, const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	if(USTPMutexId::getActionNum(mInsComplex) >= CANCEL_NUM)
		return;
	double adjustPrice = (priceType == USTP_FTDC_OPT_LimitPrice) ? orderPrice : 0.0;
	QString orderRef;
	USTPTradeApi::reqOrderInsert(insCustom, mBrokerId, mUserId, mInvestorId, instrument, priceType, timeCondition, adjustPrice, qty, direction, mOffsetFlag, mHedgeFlag, USTP_FTDC_VC_AV, orderRef);
	if(isFirstIns){
		mFirstInsStatus = USTP_FTDC_OS_ORDER_SUBMIT;
		mFirstOrderRef = orderRef;
		mCurrentReferIndex = 0;
	}else{
		mSecondRemainQty = qty;
		mSecInsStatus = USTP_FTDC_OS_ORDER_SUBMIT;
		mSecOrderRef = orderRef;
	}	
	emit onUpdateOrderShow(insCustom, instrument, mOrderLabel, 'N', direction, 0.0, qty, qty, 0, mOffsetFlag, priceType, mHedgeFlag, mOrderPrice);

	if(mIsAutoFirstCancel && isFirstIns)
		QTimer::singleShot(mFirstCancelTime, this, SLOT(doAutoCancelFirstIns()));
	else if(mIsAutoSecondCancel && isFirstIns == false){
		QTimer::singleShot(mSecondCancelTime, this, SLOT(doAutoCancelSecIns()));
	}
#ifdef _DEBUG
	int nIsSecCancel = mIsAutoSecondCancel ? 1 : 0;
	int nIsSecIns = isFirstIns ? 0 : 1;
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderInsert]   Instrument: ")) + instrument + QString(tr("  UserCustom: ")) + insCustom + 
		QString(tr("  UserId: ")) + mUserId + QString(tr("  PriceType: ")) + QString(priceType) + QString(tr("  OrderPrice: ")) + QString::number(adjustPrice) + QString(tr("  OrderVolume: ")) + 
		QString::number(qty) + QString(tr("  Direction: ")) + QString(direction) + QString(tr("  SecAutoCancel: ")) + QString::number(nIsSecCancel) + QString(tr("  IsSecIns: ")) + QString::number(nIsSecIns);
	USTPLogger::saveData(data);
#endif	
}

void USTPConditionArbitrage::doUSTPRtnOrder(const QString& userCustom, const QString& userOrderLocalId, const QString& instrumentId, const char& direction, const double& orderPrice, const int& orderVolume,
											const int& remainVolume, const int& tradeVolume, const char& offsetFlag, const char& priceType, const char& hedgeFlag, const char& orderStatus,
											const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId, const QString& seatId, const char& timeCondition)
{	
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, orderStatus, direction, orderPrice, orderVolume, remainVolume, tradeVolume, offsetFlag, priceType, hedgeFlag, mOrderPrice);
	if(mFirstOrderRef == userOrderLocalId){
		mFirstInsStatus = orderStatus;
		mFirstRemainQty = remainVolume;

		if (USTP_FTDC_OS_Canceled == orderStatus && (false == mIsAutoFirstCancel) && (!mIsOppnentPrice)){
			double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;
			if(USTP_FTDC_D_Buy == direction && mFirstMarketBasePrice <= fSecBasePrice){	//买委托满足下单条件
				mFirstOrderPrice = mFirstMarketBasePrice + mFirstSlipPrice;
				if(mFirstOrderPrice > fSecBasePrice)
					mFirstOrderPrice = fSecBasePrice;
				mSecondOrderBasePrice = mSecondMarketBasePrice;
				submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, priceType, USTP_FTDC_TC_GFD, true);

			}else if(USTP_FTDC_D_Sell == direction && mFirstMarketBasePrice >= fSecBasePrice){//卖委托满足下单条件
				mFirstOrderPrice = mFirstMarketBasePrice - mFirstSlipPrice;
				if(mFirstOrderPrice < fSecBasePrice)
					mFirstOrderPrice = fSecBasePrice;
				mSecondOrderBasePrice = mSecondMarketBasePrice;
				submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, priceType, USTP_FTDC_TC_GFD, true);
			}
		}

		if (USTP_FTDC_OS_Canceled == orderStatus && mIsDeleted){
			emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
				mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, mIsOppnentPrice, mIsDefineOrder, false, mIsActionReferTick, mOrderType);
		}
		if(USTP_FTDC_OS_Canceled == orderStatus)
			USTPMutexId::updateActionNum(instrumentId);
	}else{
		mSecInsStatus = orderStatus;
		mSecondRemainQty = remainVolume;
		if(USTP_FTDC_OS_Canceled == orderStatus && remainVolume > 0){
			orderSecondIns(false, remainVolume, orderPrice, orderPrice);
		}
	}
}


void USTPConditionArbitrage::doUSTPRtnTrade(const QString& tradeId, const QString& instrumentId, const char& direction, const int& tradeVolume, const double& tradePrice,
											const char& offsetFlag, const char& hedgeFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId,
											const QString& seatId, const QString& userOrderLocalId, const QString& tradeTime)
{
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	if(mFirstOrderRef == userOrderLocalId){
		mFirstTradeQty += tradeVolume;
		orderSecondIns(true, tradeVolume, 0.0, 0.0);
	}else{
		mSecondTradeQty += tradeVolume;
	}
	if(mFirstTradeQty >= mOrderQty && mSecondTradeQty >= mOrderQty){
		emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
			mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, true, mIsActionReferTick, mOrderType);
	}
}

void USTPConditionArbitrage::orderSecondIns(bool isInit, const int& qty, const double& bidPrice, const double& askPrice)
{	
	if (mIsCanMarket){
		if (USTP_FTDC_D_Buy == mBS)
			submitOrder(SECOND_INSTRUMENT, mSecIns, 0.0, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_BestPrice, USTP_FTDC_TC_GFD, false);
		else
			submitOrder(SECOND_INSTRUMENT, mSecIns, 0.0, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_BestPrice, USTP_FTDC_TC_GFD, false);
	}else{
		if(USTP_FTDC_D_Buy == mBS){
			if(isInit){
				double initPrice = mSecondOrderBasePrice - mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, initPrice, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}else{
				double price = bidPrice - mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, price, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}
		}else{
			if(isInit){
				double initPrice = mSecondOrderBasePrice + mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, initPrice, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}else{
				double price = askPrice + mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, price, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}
		}
	}
}

void USTPConditionArbitrage::doUSTPErrRtnOrderInsert(const QString& userCustom, const QString& brokerId, const char& direction, const QString& exchangeId, const char& hedgeFlag,
													 const QString& instrumentId, const QString& investorId, const char& offsetFlag, const char& priceType, const char& timeCondition,
													 const QString& userOrderLocalId, const double& orderPrice, const int& volume, const int& errorId, const QString& errorMsg)
{	
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	if(mFirstOrderRef == userOrderLocalId)	//设置合约状态
		mFirstInsStatus = USTP_FTDC_OS_ORDER_ERROR;
	else
		mSecInsStatus = USTP_FTDC_OS_ORDER_ERROR;

	switch (errorId){
	case 12:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'D', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice); //重复的报单
		break;
	case 31:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'P', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//平仓位不足
		break;
	case 36:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'Z', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//	资金不足
		break;
	case 8129:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'J', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//交易编码不存在
		break;
	default:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'W', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);
		break;
	}
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderInsert] UserCustom: ")) + userCustom +  QString(tr("  UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}

void USTPConditionArbitrage::doUSTPErrRtnOrderAction(const char& actionFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId,
													 const QString& orderSysId, const QString& userActionLocalId, const QString& userOrderLocalId, const double& orderPrice, 
													 const int& volumeChange, const int& errorId, const QString& errorMsg)
{
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderAction] UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  UserActionLocalId: ")) + userActionLocalId  + QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}


void USTPConditionArbitrage::doAutoCancelFirstIns()
{	
	if(isInMarket(mFirstInsStatus))
		cancelFirstIns();
}

void USTPConditionArbitrage::cancelFirstIns()
{
	mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
	submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
}

void USTPConditionArbitrage::doAutoCancelSecIns()
{	
	if(isInMarket(mSecInsStatus)){
		mSecInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
		submitAction(SECOND_INSTRUMENT, mSecOrderRef, mSecIns);
	}
}

void USTPConditionArbitrage::doDefineTimeOrderFirstIns()
{
	if((mFirstMarketBasePrice < 0.1) || (mSecondMarketBasePrice < 0.1) || (mFirstBidMarketPrice < 0.1) || (mFirstAskMarketPrice < 0.1))	//保证两腿都收到行情
		return;
	double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;
	if(mIsOppnentPrice)
		opponentPriceOrder(fSecBasePrice);
	else
		defineTimeOrder(fSecBasePrice);
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [DefineTimeOrder] InstrumentId: ")) + mFirstIns + QString(tr("  mSecondMarketBasePrice: ")) + QString::number(fSecBasePrice) +
		QString(tr("  ReferenceIns: ")) + mReferenceIns;
	USTPLogger::saveData(data);
#endif
}

void USTPConditionArbitrage::doDelOrder(const QString& orderStyle)
{
	if(orderStyle == mOrderLabel){
		mIsDeleted = true;
		if(isInMarket(mFirstInsStatus)){
			mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
			submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
		}else if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus || USTP_FTDC_OS_ORDER_ERROR == mFirstInsStatus || USTP_FTDC_OS_Canceled == mFirstInsStatus){
			emit onUpdateOrderShow(mUserCustomLabel, mFirstIns, mOrderLabel, USTP_FTDC_OS_Canceled, mBS, 0.0, mOrderQty, mOrderQty, 0, mOffsetFlag, USTP_FTDC_OPT_LimitPrice, mHedgeFlag, mOrderPrice);
			emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
				mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, false, mIsActionReferTick, mOrderType);
		}
		QString data = mOrderLabel + QString(tr("  [DoDelOrder]   mFirstInsStatus: ")) + QString(mFirstInsStatus);
		USTPLogger::saveData(data);
	}
}

void USTPConditionArbitrage::submitAction(const QString& insLabel, const QString& orderLocalId, const QString& instrument)
{	
	USTPTradeApi::reqOrderAction(mBrokerId, mUserId, mInvestorId, instrument, orderLocalId);
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderAction]   UserLocalOrderId: ")) + orderLocalId + QString(tr("  InstrumentId: ")) + instrument;
	USTPLogger::saveData(data);
#endif	
}


USTPOpponentArbitrage::USTPOpponentArbitrage(const QString& orderLabel, const QString& firstIns, const QString& secIns, const double& orderPriceTick, const int& firstPriceSlipPoint, const int& secPriceSlipPoint, 
											 const int& qty, const char& bs,  const char& offset, const char& hedge, const int& cancelFirstTime, const int& cancelSecTime, const int& cycleStall, const int& actionReferNum, 
											 const int& actionSuperNum, bool isAutoFirstCancel, bool isAutoSecCancel, bool isCycle, bool isOppentPrice, bool isDefineOrder, bool isReferTick, USTPOrderWidget* pOrderWidget,USTPCancelWidget* pCancelWidget,
											 USTPStrategyWidget* pStrategyWidget) :USTPStrategyBase(orderLabel, firstIns, secIns, orderPriceTick, qty, bs, offset, hedge, cancelFirstTime, cancelSecTime, cycleStall, firstPriceSlipPoint,
											 secPriceSlipPoint, isAutoFirstCancel, isAutoSecCancel, isCycle)
{	
	moveToThread(&mStrategyThread);
	mStrategyThread.start();
	mFirstMarketBasePrice = 0.0;
	mSecondMarketBasePrice = 0.0;
	mSecondOrderBasePrice = 0.0;
	mFirstOrderPrice = 0.0;
	mFirstBidMarketPrice = 0.0;
	mFirstAskMarketPrice = 0.0;
	mOrderType = 2;
	mFirstTradeQty = 0;
	mFirstRemainQty = mOrderQty;
	mSecondTradeQty = 0;
	mFirstBidMarketVolume = 0;
	mFirstAskMarketVolume = 0;
	mCurrentReferIndex = 0;
	mActionReferNum = actionReferNum;
	mActionSuperNum = actionSuperNum;
	mIsDefineOrder = isDefineOrder;
	mIsOppnentPrice = isOppentPrice;
	mIsActionReferTick = isReferTick;
	mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mSecInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mFirstSlipPrice = firstPriceSlipPoint * mPriceTick;
	mSecondSlipPrice = secPriceSlipPoint * mPriceTick;
	mActionSuperSlipPrice = actionSuperNum * mPriceTick;
	mStrategyLabel = "OpponentPriceArbitrage_";
	mBrokerId = USTPCtpLoader::getBrokerId();
	mUserId = USTPMutexId::getUserId();
	mInvestorId = USTPMutexId::getInvestorId();
	mReferenceIns = USTPMutexId::getReferenceIns();
	mIsCanMarket = (USTPMutexId::getInsMarketMaxVolume(secIns) > 0) ? true : false;
	initConnect(pStrategyWidget, pOrderWidget, pCancelWidget);	
	updateInitShow();
}

USTPOpponentArbitrage::~USTPOpponentArbitrage()
{
	mStrategyThread.quit();
	mStrategyThread.wait();
}

void USTPOpponentArbitrage::initConnect(USTPStrategyWidget* pStrategyWidget, USTPOrderWidget* pOrderWidget, USTPCancelWidget* pCancelWidget)
{
	connect(USTPCtpLoader::getMdSpi(), SIGNAL(onUSTPRtnDepthMarketData(const QString&, const double&, const double&, const double&, const double&, const int&, const double&, const int&, const double&, const double&, const int&)), 
		this, SLOT(doUSTPRtnDepthMarketData(const QString&, const double&, const double&, const double&, const double&, const int&, const double&, const int&, const double&, const double&, const int&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)),
		this, SLOT(doUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)),
		this, SLOT(doUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pOrderWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pCancelWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(pCancelWidget, SIGNAL(onDelOrder(const QString& )), this, SLOT(doDelOrder(const QString& )), Qt::QueuedConnection);

	connect(this, SIGNAL(onOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), pStrategyWidget, SLOT(doOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), pCancelWidget, SLOT(doOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), Qt::QueuedConnection);
}

void USTPOpponentArbitrage::updateInitShow()
{	
	if(mIsDeleted)	//如果手工撤单，则不更新
		return;
	mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mUserCustomLabel = mStrategyLabel + QString::number(USTPMutexId::getMutexId());
	emit onUpdateOrderShow(mUserCustomLabel, mFirstIns, mOrderLabel, 'N', mBS, 0.0, mFirstRemainQty, mFirstRemainQty, mFirstTradeQty, mOffsetFlag, USTP_FTDC_OPT_LimitPrice, mHedgeFlag, mOrderPrice);
}

void USTPOpponentArbitrage::doUSTPRtnDepthMarketData(const QString& instrumentId, const double& preSettlementPrice, const double& openPrice, const double& lastPrice,
													 const double& bidPrice, const int& bidVolume, const double& askPrice, const int& askVolume, const double& highestPrice, 
													 const double& lowestPrice, const int& volume)
{	
	if(mIsDefineOrder && mReferenceIns == instrumentId)
		QTimer::singleShot(DEFINE_ORDER_TIME, this, SLOT(doDefineTimeOrderFirstIns()));

	if ((mFirstIns != instrumentId) &&(mSecIns != instrumentId))	//监听第一，二腿行情
		return;
	if(USTP_FTDC_D_Buy == mBS && mFirstIns == instrumentId)
		mFirstMarketBasePrice = bidPrice;
	else if(USTP_FTDC_D_Sell == mBS && mFirstIns == instrumentId)
		mFirstMarketBasePrice = askPrice;
	else if(USTP_FTDC_D_Buy == mBS && mSecIns == instrumentId)
		mSecondMarketBasePrice = bidPrice;
	else if(USTP_FTDC_D_Sell == mBS && mSecIns == instrumentId)
		mSecondMarketBasePrice = askPrice;
	if(mFirstIns == instrumentId){
		mFirstBidMarketPrice = bidPrice;
		mFirstAskMarketPrice = askPrice;
	}
	if((mFirstMarketBasePrice < 0.1) || (mSecondMarketBasePrice < 0.1) || (mFirstBidMarketPrice < 0.1) || (mFirstAskMarketPrice < 0.1))	//保证两腿都收到行情
		return;
	double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;

	if(mIsOppnentPrice)
		opponentPriceOrder(fSecBasePrice);
	else
		noOpponentPriceOrder(instrumentId, fSecBasePrice);

}

void USTPOpponentArbitrage::opponentPriceOrder(const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单
		if(USTP_FTDC_D_Buy == mBS && mFirstAskMarketPrice <= basePrice){	//买委托满足下单条件
			mFirstOrderPrice = mFirstAskMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_IOC);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstBidMarketPrice >= basePrice){//卖委托满足下单条件
			mFirstOrderPrice = mFirstBidMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_IOC);
		}else{
			if(USTP_FTDC_OS_Canceled == mFirstInsStatus){
				updateInitShow();
			}
		}
	}
}

void USTPOpponentArbitrage::noOpponentPriceOrder(const QString& instrument, const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单

		if(USTP_FTDC_D_Buy == mBS && mFirstAskMarketPrice <= basePrice){	//对价单买委托满足下单条件
			mFirstOrderPrice = mFirstAskMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstBidMarketPrice >= basePrice){//对价单卖委托满足下单条件
			mFirstOrderPrice = mFirstBidMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Buy == mBS && mFirstMarketBasePrice <= basePrice){	//条件单买委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice + mFirstSlipPrice;
			if(mFirstOrderPrice > basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstMarketBasePrice >= basePrice){//条件单卖委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice - mFirstSlipPrice;
			if(mFirstOrderPrice < basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else{
			if(USTP_FTDC_OS_Canceled == mFirstInsStatus){
				updateInitShow();
			}
		}
	}else if(USTPStrategyBase::isInMarket(mFirstInsStatus)){//1.非设定时间撤单的情况，第一腿委托成功，第一腿行情发生变化，腿一撤单重发;2.设定时间撤单的情况，定时超时，根据行情触发。
		if(mIsActionReferTick && (mFirstIns == instrument)){
			if(USTP_FTDC_D_Buy == mBS){
				if(mFirstMarketBasePrice > mFirstOrderPrice)
					mCurrentReferIndex++;
				else
					mCurrentReferIndex = 0;
			}else{
				if(mFirstMarketBasePrice < mFirstOrderPrice)
					mCurrentReferIndex++;
				else
					mCurrentReferIndex = 0;
			}
		}else if(mIsActionReferTick && (mSecIns == instrument)){
			if(mCurrentReferIndex > 0)
				mCurrentReferIndex++;
		}
		
		double actionBidBasePrice = basePrice + mActionSuperSlipPrice;
		double actionAskBasePrice = basePrice - mActionSuperSlipPrice;

		if((mSecIns == instrument) && ((USTP_FTDC_D_Buy == mBS && (mFirstOrderPrice > actionBidBasePrice)) || (USTP_FTDC_D_Sell == mBS && (mFirstOrderPrice < actionAskBasePrice)))){
			cancelFirstIns();
		}else if((mFirstIns == instrument) && ((USTP_FTDC_D_Buy == mBS && (mFirstAskMarketPrice <= basePrice)) || (USTP_FTDC_D_Sell == mBS && (mFirstBidMarketPrice >= basePrice)))){
			cancelFirstIns();
		}else if((mSecIns == instrument) && ((USTP_FTDC_D_Buy == mBS && (mFirstMarketBasePrice > mFirstOrderPrice) && (mFirstMarketBasePrice < basePrice)) || 
			(USTP_FTDC_D_Sell == mBS && (mFirstMarketBasePrice < mFirstOrderPrice) && (mFirstMarketBasePrice > basePrice)))){
				cancelFirstIns();
		}else if((USTP_FTDC_D_Buy == mBS && (mIsActionReferTick && (mCurrentReferIndex >= mActionReferNum)) && (mFirstMarketBasePrice < basePrice)) || 
			(USTP_FTDC_D_Sell == mBS && (mIsActionReferTick && (mCurrentReferIndex >= mActionReferNum)) && (mFirstMarketBasePrice > basePrice))){
				cancelFirstIns();
		}else if((mFirstMarketBasePrice == mFirstOrderPrice) && ((USTP_FTDC_D_Buy == mBS && (mFirstOrderPrice < basePrice) && (mFirstRemainQty < mFirstBidMarketVolume)) || 
			(USTP_FTDC_D_Sell == mBS && (mFirstOrderPrice > basePrice) && (mFirstRemainQty < mFirstAskMarketVolume)))){
				if (USTP_FTDC_D_Buy == mBS)
					mFirstMarketBasePrice += mPriceTick;
				else
					mFirstMarketBasePrice -= mPriceTick;	
				cancelFirstIns();
		}
	}
}

void USTPOpponentArbitrage::defineTimeOrder(const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单

		if(USTP_FTDC_D_Buy == mBS && mFirstAskMarketPrice <= basePrice){	//对价单买委托满足下单条件
			mFirstOrderPrice = mFirstAskMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstBidMarketPrice >= basePrice){//对价单卖委托满足下单条件
			mFirstOrderPrice = mFirstBidMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Buy == mBS && mFirstMarketBasePrice <= basePrice){	//条件单买委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice + mFirstSlipPrice;
			if(mFirstOrderPrice > basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstMarketBasePrice >= basePrice){//条件单卖委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice - mFirstSlipPrice;
			if(mFirstOrderPrice < basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else{
			if(USTP_FTDC_OS_Canceled == mFirstInsStatus){
				updateInitShow();
			}
		}
	}
}

void USTPOpponentArbitrage::switchFirstInsOrder(const char& tCondition)
{
	if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus){
		if(mIsDeleted)
			return;
		orderInsert(mUserCustomLabel, FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, USTP_FTDC_OPT_LimitPrice, tCondition, true);
	}
	else
		submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, USTP_FTDC_OPT_LimitPrice, tCondition, true);
}

void USTPOpponentArbitrage::submitOrder(const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	if(isFirstIns && mIsDeleted)	//撤掉报单，合约一禁止下新单
		return;
	mUserCustomLabel = mStrategyLabel + QString::number(USTPMutexId::getMutexId());
	orderInsert(mUserCustomLabel, insLabel, instrument, orderPrice, direction, qty, priceType, timeCondition, isFirstIns);	
}

void USTPOpponentArbitrage::orderInsert(const QString& insCustom, const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	if(USTPMutexId::getActionNum(mInsComplex) >= CANCEL_NUM)
		return;
	double adjustPrice = (priceType == USTP_FTDC_OPT_LimitPrice) ? orderPrice : 0.0;
	QString orderRef;
	USTPTradeApi::reqOrderInsert(insCustom, mBrokerId, mUserId, mInvestorId, instrument, priceType, timeCondition, adjustPrice, qty, direction, mOffsetFlag, mHedgeFlag, USTP_FTDC_VC_AV, orderRef);
	if(isFirstIns){
		mFirstInsStatus = USTP_FTDC_OS_ORDER_SUBMIT;
		mFirstOrderRef = orderRef;
		mCurrentReferIndex = 0;
	}else{
		mSecondRemainQty = qty;
		mSecInsStatus = USTP_FTDC_OS_ORDER_SUBMIT;
		mSecOrderRef = orderRef;
	}	
	emit onUpdateOrderShow(insCustom, instrument, mOrderLabel, 'N', direction, 0.0, qty, qty, 0, mOffsetFlag, priceType, mHedgeFlag, mOrderPrice);

	if(mIsAutoFirstCancel && isFirstIns)
		QTimer::singleShot(mFirstCancelTime, this, SLOT(doAutoCancelFirstIns()));
	else if(mIsAutoSecondCancel && isFirstIns == false){
		QTimer::singleShot(mSecondCancelTime, this, SLOT(doAutoCancelSecIns()));
	}
#ifdef _DEBUG
	int nIsSecCancel = mIsAutoSecondCancel ? 1 : 0;
	int nIsSecIns = isFirstIns ? 0 : 1;
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderInsert]   Instrument: ")) + instrument + QString(tr("  UserCustom: ")) + insCustom + 
		QString(tr("  UserId: ")) + mUserId + QString(tr("  PriceType: ")) + QString(priceType) + QString(tr("  OrderPrice: ")) + QString::number(adjustPrice) + QString(tr("  OrderVolume: ")) + 
		QString::number(qty) + QString(tr("  Direction: ")) + QString(direction) + QString(tr("  SecAutoCancel: ")) + QString::number(nIsSecCancel) + QString(tr("  IsSecIns: ")) + QString::number(nIsSecIns);
	USTPLogger::saveData(data);
#endif	
}

void USTPOpponentArbitrage::doUSTPRtnOrder(const QString& userCustom, const QString& userOrderLocalId, const QString& instrumentId, const char& direction, const double& orderPrice, const int& orderVolume,
										   const int& remainVolume, const int& tradeVolume, const char& offsetFlag, const char& priceType, const char& hedgeFlag, const char& orderStatus,
										   const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId, const QString& seatId, const char& timeCondition)
{	
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, orderStatus, direction, orderPrice, orderVolume, remainVolume, tradeVolume, offsetFlag, priceType, hedgeFlag, mOrderPrice);
	if(mFirstOrderRef == userOrderLocalId){
		mFirstInsStatus = orderStatus;
		mFirstRemainQty = remainVolume;
		if (USTP_FTDC_OS_Canceled == orderStatus && (false == mIsAutoFirstCancel) && (!mIsOppnentPrice)){
			double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;
			if(USTP_FTDC_D_Buy == direction && mFirstAskMarketPrice <= fSecBasePrice){	//买委托满足下单条件
				mFirstOrderPrice = mFirstAskMarketPrice + mFirstSlipPrice;
				if(mFirstOrderPrice > fSecBasePrice)
					mFirstOrderPrice = fSecBasePrice;
				mSecondOrderBasePrice = mSecondMarketBasePrice;
				submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, priceType, USTP_FTDC_TC_GFD, true);

			}else if(USTP_FTDC_D_Sell == direction && mFirstBidMarketPrice >= fSecBasePrice){//卖委托满足下单条件
				mFirstOrderPrice = mFirstBidMarketPrice - mFirstSlipPrice;
				if(mFirstOrderPrice < fSecBasePrice)
					mFirstOrderPrice = fSecBasePrice;
				mSecondOrderBasePrice = mSecondMarketBasePrice;
				submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, priceType, USTP_FTDC_TC_GFD, true);
			}
		}

		if (USTP_FTDC_OS_Canceled == orderStatus && mIsDeleted){
			emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
				mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, mIsOppnentPrice, mIsDefineOrder, false, mIsActionReferTick, mOrderType);
		}
		if(USTP_FTDC_OS_Canceled == orderStatus)
			USTPMutexId::updateActionNum(instrumentId);
	}else{
		mSecInsStatus = orderStatus;
		mSecondRemainQty = remainVolume;
		if(USTP_FTDC_OS_Canceled == orderStatus && remainVolume > 0){
			orderSecondIns(false, remainVolume, orderPrice, orderPrice);
		}
	}
}


void USTPOpponentArbitrage::doUSTPRtnTrade(const QString& tradeId, const QString& instrumentId, const char& direction, const int& tradeVolume, const double& tradePrice,
										   const char& offsetFlag, const char& hedgeFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId,
										   const QString& seatId, const QString& userOrderLocalId, const QString& tradeTime)
{
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	if(mFirstOrderRef == userOrderLocalId){
		mFirstTradeQty += tradeVolume;
		orderSecondIns(true, tradeVolume, 0.0, 0.0);
	}else{
		mSecondTradeQty += tradeVolume;
	}
	if(mFirstTradeQty >= mOrderQty && mSecondTradeQty >= mOrderQty){
		emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
			mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, true, mIsActionReferTick, mOrderType);
	}
}

void USTPOpponentArbitrage::orderSecondIns(bool isInit, const int& qty, const double& bidPrice, const double& askPrice)
{	
	if (mIsCanMarket){
		if (USTP_FTDC_D_Buy == mBS)
			submitOrder(SECOND_INSTRUMENT, mSecIns, 0.0, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_BestPrice, USTP_FTDC_TC_GFD, false);
		else
			submitOrder(SECOND_INSTRUMENT, mSecIns, 0.0, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_BestPrice, USTP_FTDC_TC_GFD, false);
	}else{
		if(USTP_FTDC_D_Buy == mBS){
			if(isInit){
				double initPrice = mSecondOrderBasePrice - mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, initPrice, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}else{
				double price = bidPrice - mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, price, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}
		}else{
			if(isInit){
				double initPrice = mSecondOrderBasePrice + mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, initPrice, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}else{
				double price = askPrice + mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, price, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}
		}
	}
}

void USTPOpponentArbitrage::doUSTPErrRtnOrderInsert(const QString& userCustom, const QString& brokerId, const char& direction, const QString& exchangeId, const char& hedgeFlag,
													const QString& instrumentId, const QString& investorId, const char& offsetFlag, const char& priceType, const char& timeCondition,
													const QString& userOrderLocalId, const double& orderPrice, const int& volume, const int& errorId, const QString& errorMsg)
{	
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	if(mFirstOrderRef == userOrderLocalId)	//设置合约状态
		mFirstInsStatus = USTP_FTDC_OS_ORDER_ERROR;
	else
		mSecInsStatus = USTP_FTDC_OS_ORDER_ERROR;

	switch (errorId){
	case 12:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'D', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice); //重复的报单
		break;
	case 31:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'P', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//平仓位不足
		break;
	case 36:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'Z', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//	资金不足
		break;
	case 8129:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'J', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//交易编码不存在
		break;
	default:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'W', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);
		break;
	}
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderInsert] UserCustom: ")) + userCustom +  QString(tr("  UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}

void USTPOpponentArbitrage::doUSTPErrRtnOrderAction(const char& actionFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId,
													const QString& orderSysId, const QString& userActionLocalId, const QString& userOrderLocalId, const double& orderPrice, 
													const int& volumeChange, const int& errorId, const QString& errorMsg)
{
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderAction] UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  UserActionLocalId: ")) + userActionLocalId  + QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}


void USTPOpponentArbitrage::doAutoCancelFirstIns()
{	
	if(isInMarket(mFirstInsStatus))
		cancelFirstIns();
}

void USTPOpponentArbitrage::cancelFirstIns()
{
	mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
	submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
}

void USTPOpponentArbitrage::doAutoCancelSecIns()
{	
	if(isInMarket(mSecInsStatus)){
		mSecInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
		submitAction(SECOND_INSTRUMENT, mSecOrderRef, mSecIns);
	}
}

void USTPOpponentArbitrage::doDefineTimeOrderFirstIns()
{
	if((mFirstMarketBasePrice < 0.1) || (mSecondMarketBasePrice < 0.1) || (mFirstBidMarketPrice < 0.1) || (mFirstAskMarketPrice < 0.1))	//保证两腿都收到行情
		return;
	double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;
	if(mIsOppnentPrice)
		opponentPriceOrder(fSecBasePrice);
	else
		defineTimeOrder(fSecBasePrice);
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [DefineTimeOrder] InstrumentId: ")) + mFirstIns + QString(tr("  mSecondMarketBasePrice: ")) + QString::number(fSecBasePrice) +
		QString(tr("  ReferenceIns: ")) + mReferenceIns;
	USTPLogger::saveData(data);
#endif
}

void USTPOpponentArbitrage::doDelOrder(const QString& orderStyle)
{
	if(orderStyle == mOrderLabel){
		mIsDeleted = true;
		if(isInMarket(mFirstInsStatus)){
			mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
			submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
		}else if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus || USTP_FTDC_OS_ORDER_ERROR == mFirstInsStatus || USTP_FTDC_OS_Canceled == mFirstInsStatus){
			emit onUpdateOrderShow(mUserCustomLabel, mFirstIns, mOrderLabel, USTP_FTDC_OS_Canceled, mBS, 0.0, mOrderQty, mOrderQty, 0, mOffsetFlag, USTP_FTDC_OPT_LimitPrice, mHedgeFlag, mOrderPrice);
			emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
				mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, false, mIsActionReferTick, mOrderType);
		}
		QString data = mOrderLabel + QString(tr("  [DoDelOrder]   mFirstInsStatus: ")) + QString(mFirstInsStatus);
		USTPLogger::saveData(data);
	}
}

void USTPOpponentArbitrage::submitAction(const QString& insLabel, const QString& orderLocalId, const QString& instrument)
{	
	USTPTradeApi::reqOrderAction(mBrokerId, mUserId, mInvestorId, instrument, orderLocalId);
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderAction]   UserLocalOrderId: ")) + orderLocalId + QString(tr("  InstrumentId: ")) + instrument;
	USTPLogger::saveData(data);
#endif	
}


USTPConditionStareArbitrage::USTPConditionStareArbitrage(const QString& orderLabel, const QString& firstIns, const QString& secIns, const double& orderPriceTick, const int& firstPriceSlipPoint, const int& secPriceSlipPoint, 
														 const int& qty, const char& bs,  const char& offset, const char& hedge, const int& cancelFirstTime, const int& cancelSecTime, const int& cycleStall, bool isAutoFirstCancel, bool isAutoSecCancel,
														 bool isCycle, bool isOppentPrice, bool isDefineOrder, USTPOrderWidget* pOrderWidget,USTPCancelWidget* pCancelWidget, USTPStrategyWidget* pStrategyWidget) :USTPStrategyBase(orderLabel, 
														 firstIns, secIns, orderPriceTick, qty, bs, offset, hedge, cancelFirstTime, cancelSecTime, cycleStall, firstPriceSlipPoint, secPriceSlipPoint, isAutoFirstCancel, isAutoSecCancel, isCycle)
{	
	moveToThread(&mStrategyThread);
	mStrategyThread.start();
	mFirstMarketBasePrice = 0.0;
	mSecondMarketBasePrice = 0.0;
	mSecondOrderBasePrice = 0.0;
	mFirstOrderPrice = 0.0;
	mFirstBidMarketPrice = 0.0;
	mFirstAskMarketPrice = 0.0;
	mOrderType = 3;
	mFirstTradeQty = 0;
	mFirstRemainQty = mOrderQty;
	mSecondTradeQty = 0;
	mFirstBidMarketVolume = 0;
	mFirstAskMarketVolume = 0;
	mIsDefineOrder = isDefineOrder;
	mIsOppnentPrice = isOppentPrice;
	mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mSecInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mFirstSlipPrice = firstPriceSlipPoint * mPriceTick;
	mSecondSlipPrice = secPriceSlipPoint * mPriceTick;
	mStrategyLabel = "ConditionStareArbitrage_";
	mBrokerId = USTPCtpLoader::getBrokerId();
	mUserId = USTPMutexId::getUserId();
	mInvestorId = USTPMutexId::getInvestorId();
	mReferenceIns = USTPMutexId::getReferenceIns();
	mIsCanMarket = (USTPMutexId::getInsMarketMaxVolume(secIns) > 0) ? true : false;
	initConnect(pStrategyWidget, pOrderWidget, pCancelWidget);	
	updateInitShow();
}

USTPConditionStareArbitrage::~USTPConditionStareArbitrage()
{
	mStrategyThread.quit();
	mStrategyThread.wait();
}

void USTPConditionStareArbitrage::initConnect(USTPStrategyWidget* pStrategyWidget, USTPOrderWidget* pOrderWidget, USTPCancelWidget* pCancelWidget)
{
	connect(USTPCtpLoader::getMdSpi(), SIGNAL(onUSTPRtnDepthMarketData(const QString&, const double&, const double&, const double&, const double&, const int&, const double&, const int&, const double&, const double&, const int&)), 
		this, SLOT(doUSTPRtnDepthMarketData(const QString&, const double&, const double&, const double&, const double&, const int&, const double&, const int&, const double&, const double&, const int&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)),
		this, SLOT(doUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)),
		this, SLOT(doUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pOrderWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pCancelWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(pCancelWidget, SIGNAL(onDelOrder(const QString& )), this, SLOT(doDelOrder(const QString& )), Qt::QueuedConnection);

	connect(this, SIGNAL(onOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), pStrategyWidget, SLOT(doOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), pCancelWidget, SLOT(doOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), Qt::QueuedConnection);
}

void USTPConditionStareArbitrage::updateInitShow()
{	
	if(mIsDeleted)	//如果手工撤单，则不更新
		return;
	mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mUserCustomLabel = mStrategyLabel + QString::number(USTPMutexId::getMutexId());
	emit onUpdateOrderShow(mUserCustomLabel, mFirstIns, mOrderLabel, 'N', mBS, 0.0, mFirstRemainQty, mFirstRemainQty, mFirstTradeQty, mOffsetFlag, USTP_FTDC_OPT_LimitPrice, mHedgeFlag, mOrderPrice);
}

void USTPConditionStareArbitrage::doUSTPRtnDepthMarketData(const QString& instrumentId, const double& preSettlementPrice, const double& openPrice, const double& lastPrice,
														   const double& bidPrice, const int& bidVolume, const double& askPrice, const int& askVolume, const double& highestPrice, 
														   const double& lowestPrice, const int& volume)
{	
	if(mIsDefineOrder && mReferenceIns == instrumentId)
		QTimer::singleShot(DEFINE_ORDER_TIME, this, SLOT(doDefineTimeOrderFirstIns()));

	if ((mFirstIns != instrumentId) &&(mSecIns != instrumentId))	//监听第一，二腿行情
		return;
	if(USTP_FTDC_D_Buy == mBS && mFirstIns == instrumentId)
		mFirstMarketBasePrice = bidPrice;
	else if(USTP_FTDC_D_Sell == mBS && mFirstIns == instrumentId)
		mFirstMarketBasePrice = askPrice;
	else if(USTP_FTDC_D_Buy == mBS && mSecIns == instrumentId)
		mSecondMarketBasePrice = bidPrice;
	else if(USTP_FTDC_D_Sell == mBS && mSecIns == instrumentId)
		mSecondMarketBasePrice = askPrice;
	if(mFirstIns == instrumentId){
		mFirstBidMarketPrice = bidPrice;
		mFirstAskMarketPrice = askPrice;
		mFirstBidMarketVolume = bidVolume;
		mFirstAskMarketVolume = askVolume;
	}
	if((mFirstMarketBasePrice < 0.1) || (mSecondMarketBasePrice < 0.1) || (mFirstBidMarketPrice < 0.1) || (mFirstAskMarketPrice < 0.1))	//保证两腿都收到行情
		return;
	double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;
	if(mIsOppnentPrice)
		opponentPriceOrder(fSecBasePrice);
	else
		noOpponentPriceOrder(instrumentId, fSecBasePrice);
}

void USTPConditionStareArbitrage::opponentPriceOrder(const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单
		if(USTP_FTDC_D_Buy == mBS){	//买委托满足下单条件
			if(mFirstAskMarketPrice <= basePrice)
				mFirstOrderPrice = mFirstAskMarketPrice;
			else
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_IOC);

		}else{//卖委托满足下单条件
			if(mFirstBidMarketPrice >= basePrice)
				mFirstOrderPrice = mFirstBidMarketPrice;
			else
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_IOC);
		}
	}
}

void USTPConditionStareArbitrage::noOpponentPriceOrder(const QString& instrument, const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单

		if(USTP_FTDC_D_Buy == mBS){	//条件单买委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice + mFirstSlipPrice;
			if(mFirstOrderPrice > basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else{//条件单卖委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice - mFirstSlipPrice;
			if(mFirstOrderPrice < basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}
	}else if(USTPStrategyBase::isInMarket(mFirstInsStatus)){//1.非设定时间撤单的情况，第一腿委托成功，第一腿行情发生变化，腿一撤单重发;2.设定时间撤单的情况，定时超时，根据行情触发。
		if((mSecIns == instrument) && ((USTP_FTDC_D_Buy == mBS && (mFirstOrderPrice > basePrice)) || (USTP_FTDC_D_Sell == mBS && (mFirstOrderPrice < basePrice)))){
			cancelFirstIns();
		}else if((mFirstIns == instrument) && ((USTP_FTDC_D_Buy == mBS && (mFirstMarketBasePrice > mFirstOrderPrice)) || (USTP_FTDC_D_Sell == mBS && (mFirstMarketBasePrice < mFirstOrderPrice)))){
			cancelFirstIns();
		}else if((mFirstMarketBasePrice == mFirstOrderPrice) && ((USTP_FTDC_D_Buy == mBS && (mFirstOrderPrice < basePrice) && (mFirstRemainQty < mFirstBidMarketVolume)) || 
			(USTP_FTDC_D_Sell == mBS && (mFirstOrderPrice > basePrice) && (mFirstRemainQty < mFirstAskMarketVolume)))){
				if (USTP_FTDC_D_Buy == mBS)
					mFirstMarketBasePrice += mPriceTick;
				else
					mFirstMarketBasePrice -= mPriceTick;	
				cancelFirstIns();
		}
	}
}

void USTPConditionStareArbitrage::defineTimeOrder(const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单

		if(USTP_FTDC_D_Buy == mBS){	//条件单买委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice + mFirstSlipPrice;
			if(mFirstOrderPrice > basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);
		}else{//条件单卖委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice - mFirstSlipPrice;
			if(mFirstOrderPrice < basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);
		}
	}
}

void USTPConditionStareArbitrage::switchFirstInsOrder(const char& tCondition)
{
	if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus){
		if(mIsDeleted)
			return;
		orderInsert(mUserCustomLabel, FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, USTP_FTDC_OPT_LimitPrice, tCondition, true);
	}else
		submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, USTP_FTDC_OPT_LimitPrice, tCondition, true);
}

void USTPConditionStareArbitrage::submitOrder(const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	if(isFirstIns && mIsDeleted)	//撤掉报单，合约一禁止下新单
		return;
	mUserCustomLabel = mStrategyLabel + QString::number(USTPMutexId::getMutexId());
	orderInsert(mUserCustomLabel, insLabel, instrument, orderPrice, direction, qty, priceType, timeCondition, isFirstIns);	
}

void USTPConditionStareArbitrage::orderInsert(const QString& insCustom, const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	if(USTPMutexId::getActionNum(mInsComplex) >= CANCEL_NUM)
		return;
	double adjustPrice = (priceType == USTP_FTDC_OPT_LimitPrice) ? orderPrice : 0.0;
	QString orderRef;
	USTPTradeApi::reqOrderInsert(insCustom, mBrokerId, mUserId, mInvestorId, instrument, priceType, timeCondition, adjustPrice, qty, direction, mOffsetFlag, mHedgeFlag, USTP_FTDC_VC_AV, orderRef);
	if(isFirstIns){
		mFirstInsStatus = USTP_FTDC_OS_ORDER_SUBMIT;
		mFirstOrderRef = orderRef;
	}else{
		mSecondRemainQty = qty;
		mSecInsStatus = USTP_FTDC_OS_ORDER_SUBMIT;
		mSecOrderRef = orderRef;
	}	
	emit onUpdateOrderShow(insCustom, instrument, mOrderLabel, 'N', direction, 0.0, qty, qty, 0, mOffsetFlag, priceType, mHedgeFlag, mOrderPrice);

	if(mIsAutoFirstCancel && isFirstIns)
		QTimer::singleShot(mFirstCancelTime, this, SLOT(doAutoCancelFirstIns()));
	else if(mIsAutoSecondCancel && isFirstIns == false){
		QTimer::singleShot(mSecondCancelTime, this, SLOT(doAutoCancelSecIns()));
	}
#ifdef _DEBUG
	int nIsSecCancel = mIsAutoSecondCancel ? 1 : 0;
	int nIsSecIns = isFirstIns ? 0 : 1;
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderInsert]   Instrument: ")) + instrument + QString(tr("  UserCustom: ")) + insCustom + 
		QString(tr("  UserId: ")) + mUserId + QString(tr("  PriceType: ")) + QString(priceType) + QString(tr("  OrderPrice: ")) + QString::number(adjustPrice) + QString(tr("  OrderVolume: ")) + 
		QString::number(qty) + QString(tr("  Direction: ")) + QString(direction) + QString(tr("  SecAutoCancel: ")) + QString::number(nIsSecCancel) + QString(tr("  IsSecIns: ")) + QString::number(nIsSecIns);
	USTPLogger::saveData(data);
#endif	
}

void USTPConditionStareArbitrage::doUSTPRtnOrder(const QString& userCustom, const QString& userOrderLocalId, const QString& instrumentId, const char& direction, const double& orderPrice, const int& orderVolume,
												 const int& remainVolume, const int& tradeVolume, const char& offsetFlag, const char& priceType, const char& hedgeFlag, const char& orderStatus,
												 const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId, const QString& seatId, const char& timeCondition)
{	
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, orderStatus, direction, orderPrice, orderVolume, remainVolume, tradeVolume, offsetFlag, priceType, hedgeFlag, mOrderPrice);
	if(mFirstOrderRef == userOrderLocalId){
		mFirstInsStatus = orderStatus;
		mFirstRemainQty = remainVolume;

		if (USTP_FTDC_OS_Canceled == orderStatus && (false == mIsAutoFirstCancel) && (!mIsOppnentPrice)){
			double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;
			if(USTP_FTDC_D_Buy == direction && mFirstMarketBasePrice <= fSecBasePrice){	//买委托满足下单条件
				mFirstOrderPrice = mFirstMarketBasePrice + mFirstSlipPrice;
				if(mFirstOrderPrice > fSecBasePrice)
					mFirstOrderPrice = fSecBasePrice;
				mSecondOrderBasePrice = mSecondMarketBasePrice;
				submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, priceType, USTP_FTDC_TC_GFD, true);

			}else if(USTP_FTDC_D_Sell == direction && mFirstMarketBasePrice >= fSecBasePrice){//卖委托满足下单条件
				mFirstOrderPrice = mFirstMarketBasePrice - mFirstSlipPrice;
				if(mFirstOrderPrice < fSecBasePrice)
					mFirstOrderPrice = fSecBasePrice;
				mSecondOrderBasePrice = mSecondMarketBasePrice;
				submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, priceType, USTP_FTDC_TC_GFD, true);
			}
		}

		if (USTP_FTDC_OS_Canceled == orderStatus && mIsDeleted){
			emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
				mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, mIsOppnentPrice, mIsDefineOrder, false, mIsActionReferTick, mOrderType);
		}
		if(USTP_FTDC_OS_Canceled == orderStatus)
			USTPMutexId::updateActionNum(instrumentId);
	}else{
		mSecInsStatus = orderStatus;
		mSecondRemainQty = remainVolume;
		if(USTP_FTDC_OS_Canceled == orderStatus && remainVolume > 0){
			orderSecondIns(false, remainVolume, orderPrice, orderPrice);
		}
	}
}


void USTPConditionStareArbitrage::doUSTPRtnTrade(const QString& tradeId, const QString& instrumentId, const char& direction, const int& tradeVolume, const double& tradePrice,
												 const char& offsetFlag, const char& hedgeFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId,
												 const QString& seatId, const QString& userOrderLocalId, const QString& tradeTime)
{
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	if(mFirstOrderRef == userOrderLocalId){
		mFirstTradeQty += tradeVolume;
		orderSecondIns(true, tradeVolume, 0.0, 0.0);
	}else{
		mSecondTradeQty += tradeVolume;
	}
	if(mFirstTradeQty >= mOrderQty && mSecondTradeQty >= mOrderQty){
		emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
			mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, true, mIsActionReferTick, mOrderType);
	}
}

void USTPConditionStareArbitrage::orderSecondIns(bool isInit, const int& qty, const double& bidPrice, const double& askPrice)
{	
	if (mIsCanMarket){
		if (USTP_FTDC_D_Buy == mBS)
			submitOrder(SECOND_INSTRUMENT, mSecIns, 0.0, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_BestPrice, USTP_FTDC_TC_GFD, false);
		else
			submitOrder(SECOND_INSTRUMENT, mSecIns, 0.0, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_BestPrice, USTP_FTDC_TC_GFD, false);
	}else{
		if(USTP_FTDC_D_Buy == mBS){
			if(isInit){
				double initPrice = mSecondOrderBasePrice - mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, initPrice, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}else{
				double price = bidPrice - mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, price, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}
		}else{
			if(isInit){
				double initPrice = mSecondOrderBasePrice + mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, initPrice, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}else{
				double price = askPrice + mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, price, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}
		}
	}
}

void USTPConditionStareArbitrage::doUSTPErrRtnOrderInsert(const QString& userCustom, const QString& brokerId, const char& direction, const QString& exchangeId, const char& hedgeFlag,
														  const QString& instrumentId, const QString& investorId, const char& offsetFlag, const char& priceType, const char& timeCondition,
														  const QString& userOrderLocalId, const double& orderPrice, const int& volume, const int& errorId, const QString& errorMsg)
{	
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	if(mFirstOrderRef == userOrderLocalId)	//设置合约状态
		mFirstInsStatus = USTP_FTDC_OS_ORDER_ERROR;
	else
		mSecInsStatus = USTP_FTDC_OS_ORDER_ERROR;

	switch (errorId){
	case 12:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'D', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice); //重复的报单
		break;
	case 31:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'P', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//平仓位不足
		break;
	case 36:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'Z', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//	资金不足
		break;
	case 8129:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'J', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//交易编码不存在
		break;
	default:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'W', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);
		break;
	}
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderInsert] UserCustom: ")) + userCustom +  QString(tr("  UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}

void USTPConditionStareArbitrage::doUSTPErrRtnOrderAction(const char& actionFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId,
														  const QString& orderSysId, const QString& userActionLocalId, const QString& userOrderLocalId, const double& orderPrice, 
														  const int& volumeChange, const int& errorId, const QString& errorMsg)
{
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderAction] UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  UserActionLocalId: ")) + userActionLocalId  + QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}


void USTPConditionStareArbitrage::doAutoCancelFirstIns()
{	
	if(isInMarket(mFirstInsStatus))
		cancelFirstIns();
}

void USTPConditionStareArbitrage::cancelFirstIns()
{
	mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
	submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
}

void USTPConditionStareArbitrage::doAutoCancelSecIns()
{	
	if(isInMarket(mSecInsStatus)){
		mSecInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
		submitAction(SECOND_INSTRUMENT, mSecOrderRef, mSecIns);
	}
}

void USTPConditionStareArbitrage::doDefineTimeOrderFirstIns()
{
	if((mFirstMarketBasePrice < 0.1) || (mSecondMarketBasePrice < 0.1) || (mFirstBidMarketPrice < 0.1) || (mFirstAskMarketPrice < 0.1))	//保证两腿都收到行情
		return;
	double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;
	if(mIsOppnentPrice)
		opponentPriceOrder(fSecBasePrice);
	else
		defineTimeOrder(fSecBasePrice);
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [DefineTimeOrder] InstrumentId: ")) + mFirstIns + QString(tr("  mSecondMarketBasePrice: ")) + QString::number(fSecBasePrice) +
		QString(tr("  ReferenceIns: ")) + mReferenceIns;
	USTPLogger::saveData(data);
#endif
}

void USTPConditionStareArbitrage::doDelOrder(const QString& orderStyle)
{
	if(orderStyle == mOrderLabel){
		mIsDeleted = true;
		if(isInMarket(mFirstInsStatus)){
			mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
			submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
		}else if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus || USTP_FTDC_OS_ORDER_ERROR == mFirstInsStatus || USTP_FTDC_OS_Canceled == mFirstInsStatus){
			emit onUpdateOrderShow(mUserCustomLabel, mFirstIns, mOrderLabel, USTP_FTDC_OS_Canceled, mBS, 0.0, mOrderQty, mOrderQty, 0, mOffsetFlag, USTP_FTDC_OPT_LimitPrice, mHedgeFlag, mOrderPrice);
			emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
				mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, false, mIsActionReferTick, mOrderType);
		}
		QString data = mOrderLabel + QString(tr("  [DoDelOrder]   mFirstInsStatus: ")) + QString(mFirstInsStatus);
		USTPLogger::saveData(data);
	}
}

void USTPConditionStareArbitrage::submitAction(const QString& insLabel, const QString& orderLocalId, const QString& instrument)
{	
	USTPTradeApi::reqOrderAction(mBrokerId, mUserId, mInvestorId, instrument, orderLocalId);
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderAction]   UserLocalOrderId: ")) + orderLocalId + QString(tr("  InstrumentId: ")) + instrument;
	USTPLogger::saveData(data);
#endif	
}


USTPOpponentStareArbitrage::USTPOpponentStareArbitrage(const QString& orderLabel, const QString& firstIns, const QString& secIns, const double& orderPriceTick, const int& firstPriceSlipPoint, const int& secPriceSlipPoint, 
													   const int& qty, const char& bs,  const char& offset, const char& hedge, const int& cancelFirstTime, const int& cancelSecTime, const int& cycleStall, bool isAutoFirstCancel, bool isAutoSecCancel,
													   bool isCycle, bool isOppentPrice, bool isDefineOrder, USTPOrderWidget* pOrderWidget,USTPCancelWidget* pCancelWidget, USTPStrategyWidget* pStrategyWidget) :USTPStrategyBase(orderLabel, 
													   firstIns, secIns, orderPriceTick, qty, bs, offset, hedge, cancelFirstTime, cancelSecTime, cycleStall, firstPriceSlipPoint, secPriceSlipPoint, isAutoFirstCancel, isAutoSecCancel, isCycle)
{	
	moveToThread(&mStrategyThread);
	mStrategyThread.start();
	mFirstMarketBasePrice = 0.0;
	mSecondMarketBasePrice = 0.0;
	mSecondOrderBasePrice = 0.0;
	mFirstOrderPrice = 0.0;
	mFirstBidMarketPrice = 0.0;
	mFirstAskMarketPrice = 0.0;
	mOrderType = 4;
	mFirstTradeQty = 0;
	mFirstRemainQty = mOrderQty;
	mSecondTradeQty = 0;
	mFirstBidMarketVolume = 0;
	mFirstAskMarketVolume = 0;
	mIsDefineOrder = isDefineOrder;
	mIsOppnentPrice = isOppentPrice;
	mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mSecInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mFirstSlipPrice = firstPriceSlipPoint * mPriceTick;
	mSecondSlipPrice = secPriceSlipPoint * mPriceTick;
	mStrategyLabel = "OpponentStareArbitrage_";
	mBrokerId = USTPCtpLoader::getBrokerId();
	mUserId = USTPMutexId::getUserId();
	mInvestorId = USTPMutexId::getInvestorId();
	mReferenceIns = USTPMutexId::getReferenceIns();
	mIsCanMarket = (USTPMutexId::getInsMarketMaxVolume(secIns) > 0) ? true : false;
	initConnect(pStrategyWidget, pOrderWidget, pCancelWidget);	
	updateInitShow();
}

USTPOpponentStareArbitrage::~USTPOpponentStareArbitrage()
{
	mStrategyThread.quit();
	mStrategyThread.wait();
}

void USTPOpponentStareArbitrage::initConnect(USTPStrategyWidget* pStrategyWidget, USTPOrderWidget* pOrderWidget, USTPCancelWidget* pCancelWidget)
{
	connect(USTPCtpLoader::getMdSpi(), SIGNAL(onUSTPRtnDepthMarketData(const QString&, const double&, const double&, const double&, const double&, const int&, const double&, const int&, const double&, const double&, const int&)), 
		this, SLOT(doUSTPRtnDepthMarketData(const QString&, const double&, const double&, const double&, const double&, const int&, const double&, const int&, const double&, const double&, const int&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)),
		this, SLOT(doUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)),
		this, SLOT(doUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pOrderWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pCancelWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(pCancelWidget, SIGNAL(onDelOrder(const QString& )), this, SLOT(doDelOrder(const QString& )), Qt::QueuedConnection);

	connect(this, SIGNAL(onOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), pStrategyWidget, SLOT(doOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), pCancelWidget, SLOT(doOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool,bool, bool, bool, bool, bool, const int&)), Qt::QueuedConnection);
}

void USTPOpponentStareArbitrage::updateInitShow()
{	
	if(mIsDeleted)	//如果手工撤单，则不更新
		return;
	mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mUserCustomLabel = mStrategyLabel + QString::number(USTPMutexId::getMutexId());
	emit onUpdateOrderShow(mUserCustomLabel, mFirstIns, mOrderLabel, 'N', mBS, 0.0, mFirstRemainQty, mFirstRemainQty, mFirstTradeQty, mOffsetFlag, USTP_FTDC_OPT_LimitPrice, mHedgeFlag, mOrderPrice);
}

void USTPOpponentStareArbitrage::doUSTPRtnDepthMarketData(const QString& instrumentId, const double& preSettlementPrice, const double& openPrice, const double& lastPrice,
														  const double& bidPrice, const int& bidVolume, const double& askPrice, const int& askVolume, const double& highestPrice, 
														  const double& lowestPrice, const int& volume)
{	
	if(mIsDefineOrder && mReferenceIns == instrumentId)
		QTimer::singleShot(DEFINE_ORDER_TIME, this, SLOT(doDefineTimeOrderFirstIns()));

	if ((mFirstIns != instrumentId) &&(mSecIns != instrumentId))	//监听第一，二腿行情
		return;
	if(USTP_FTDC_D_Buy == mBS && mFirstIns == instrumentId)
		mFirstMarketBasePrice = bidPrice;
	else if(USTP_FTDC_D_Sell == mBS && mFirstIns == instrumentId)
		mFirstMarketBasePrice = askPrice;
	else if(USTP_FTDC_D_Buy == mBS && mSecIns == instrumentId)
		mSecondMarketBasePrice = bidPrice;
	else if(USTP_FTDC_D_Sell == mBS && mSecIns == instrumentId)
		mSecondMarketBasePrice = askPrice;
	if(mFirstIns == instrumentId){
		mFirstBidMarketPrice = bidPrice;
		mFirstAskMarketPrice = askPrice;
	}
	if((mFirstMarketBasePrice < 0.1) || (mSecondMarketBasePrice < 0.1) || (mFirstBidMarketPrice < 0.1) || (mFirstAskMarketPrice < 0.1))	//保证两腿都收到行情
		return;
	double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;
	if(mIsOppnentPrice)
		opponentPriceOrder(fSecBasePrice);
	else
		noOpponentPriceOrder(instrumentId, fSecBasePrice);
}

void USTPOpponentStareArbitrage::opponentPriceOrder(const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单
		if(USTP_FTDC_D_Buy == mBS){	//买委托满足下单条件
			if(mFirstAskMarketPrice <= basePrice)
				mFirstOrderPrice = mFirstAskMarketPrice;
			else
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_IOC);

		}else{//卖委托满足下单条件
			if(mFirstBidMarketPrice >= basePrice)
				mFirstOrderPrice = mFirstBidMarketPrice;
			else
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_IOC);
		}
	}
}


void USTPOpponentStareArbitrage::noOpponentPriceOrder(const QString& instrument, const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单

		if(USTP_FTDC_D_Buy == mBS && mFirstAskMarketPrice <= basePrice){	//对价单买委托满足下单条件
			mFirstOrderPrice = mFirstAskMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstBidMarketPrice >= basePrice){//对价单卖委托满足下单条件
			mFirstOrderPrice = mFirstBidMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Buy == mBS && mFirstMarketBasePrice <= basePrice){	//条件单买委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice + mFirstSlipPrice;
			if(mFirstOrderPrice > basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstMarketBasePrice >= basePrice){//条件单卖委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice - mFirstSlipPrice;
			if(mFirstOrderPrice < basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else{
			mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);
		}

	}else if(USTPStrategyBase::isInMarket(mFirstInsStatus)){//1.非设定时间撤单的情况，第一腿委托成功，第一腿行情发生变化，腿一撤单重发;2.设定时间撤单的情况，定时超时，根据行情触发。
		if((mSecIns == instrument) && ((USTP_FTDC_D_Buy == mBS && (mFirstOrderPrice > basePrice)) || (USTP_FTDC_D_Sell == mBS && (mFirstOrderPrice < basePrice)))){
			cancelFirstIns();
		}else if((mFirstIns == instrument) && ((USTP_FTDC_D_Buy == mBS && (mFirstMarketBasePrice > mFirstOrderPrice)) || (USTP_FTDC_D_Sell == mBS && (mFirstMarketBasePrice < mFirstOrderPrice)))){
			cancelFirstIns();
		}else if((mFirstMarketBasePrice == mFirstOrderPrice) && ((USTP_FTDC_D_Buy == mBS && (mFirstOrderPrice < basePrice) && (mFirstRemainQty < mFirstBidMarketVolume)) || 
			(USTP_FTDC_D_Sell == mBS && (mFirstOrderPrice > basePrice) && (mFirstRemainQty < mFirstAskMarketVolume)))){
				if (USTP_FTDC_D_Buy == mBS)
					mFirstMarketBasePrice += mPriceTick;
				else
					mFirstMarketBasePrice -= mPriceTick;	
				cancelFirstIns();
		}
	}
}

void USTPOpponentStareArbitrage::defineTimeOrder(const double& basePrice)
{
	if(((USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus) || (USTP_FTDC_OS_Canceled == mFirstInsStatus))){//第一腿没有下单或者已撤单

		if(USTP_FTDC_D_Buy == mBS && mFirstAskMarketPrice <= basePrice){	//对价单买委托满足下单条件
			mFirstOrderPrice = mFirstAskMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstBidMarketPrice >= basePrice){//对价单卖委托满足下单条件
			mFirstOrderPrice = mFirstBidMarketPrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Buy == mBS && mFirstMarketBasePrice <= basePrice){	//条件单买委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice + mFirstSlipPrice;
			if(mFirstOrderPrice > basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else if(USTP_FTDC_D_Sell == mBS && mFirstMarketBasePrice >= basePrice){//条件单卖委托满足下单条件
			mFirstOrderPrice = mFirstMarketBasePrice - mFirstSlipPrice;
			if(mFirstOrderPrice < basePrice)
				mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);

		}else{
			mFirstOrderPrice = basePrice;
			mSecondOrderBasePrice = mSecondMarketBasePrice;
			switchFirstInsOrder(USTP_FTDC_TC_GFD);
		}
	}
}

void USTPOpponentStareArbitrage::switchFirstInsOrder(const char& tCondition)
{
	if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus){
		if(mIsDeleted)
			return;
		orderInsert(mUserCustomLabel, FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, USTP_FTDC_OPT_LimitPrice, tCondition, true);
	}
	else
		submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, USTP_FTDC_OPT_LimitPrice, tCondition, true);
}

void USTPOpponentStareArbitrage::submitOrder(const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	if(isFirstIns && mIsDeleted)	//撤掉报单，合约一禁止下新单
		return;
	mUserCustomLabel = mStrategyLabel + QString::number(USTPMutexId::getMutexId());
	orderInsert(mUserCustomLabel, insLabel, instrument, orderPrice, direction, qty, priceType, timeCondition, isFirstIns);	
}

void USTPOpponentStareArbitrage::orderInsert(const QString& insCustom, const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	if(USTPMutexId::getActionNum(mInsComplex) >= CANCEL_NUM)
		return;
	double adjustPrice = (priceType == USTP_FTDC_OPT_LimitPrice) ? orderPrice : 0.0;
	QString orderRef;
	USTPTradeApi::reqOrderInsert(insCustom, mBrokerId, mUserId, mInvestorId, instrument, priceType, timeCondition, adjustPrice, qty, direction, mOffsetFlag, mHedgeFlag, USTP_FTDC_VC_AV, orderRef);
	if(isFirstIns){
		mFirstInsStatus = USTP_FTDC_OS_ORDER_SUBMIT;
		mFirstOrderRef = orderRef;
	}else{
		mSecondRemainQty = qty;
		mSecInsStatus = USTP_FTDC_OS_ORDER_SUBMIT;
		mSecOrderRef = orderRef;
	}	
	emit onUpdateOrderShow(insCustom, instrument, mOrderLabel, 'N', direction, 0.0, qty, qty, 0, mOffsetFlag, priceType, mHedgeFlag, mOrderPrice);

	if(mIsAutoFirstCancel && isFirstIns)
		QTimer::singleShot(mFirstCancelTime, this, SLOT(doAutoCancelFirstIns()));
	else if(mIsAutoSecondCancel && isFirstIns == false){
		QTimer::singleShot(mSecondCancelTime, this, SLOT(doAutoCancelSecIns()));
	}
#ifdef _DEBUG
	int nIsSecCancel = mIsAutoSecondCancel ? 1 : 0;
	int nIsSecIns = isFirstIns ? 0 : 1;
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderInsert]   Instrument: ")) + instrument + QString(tr("  UserCustom: ")) + insCustom + 
		QString(tr("  UserId: ")) + mUserId + QString(tr("  PriceType: ")) + QString(priceType) + QString(tr("  OrderPrice: ")) + QString::number(adjustPrice) + QString(tr("  OrderVolume: ")) + 
		QString::number(qty) + QString(tr("  Direction: ")) + QString(direction) + QString(tr("  SecAutoCancel: ")) + QString::number(nIsSecCancel) + QString(tr("  IsSecIns: ")) + QString::number(nIsSecIns);
	USTPLogger::saveData(data);
#endif	
}

void USTPOpponentStareArbitrage::doUSTPRtnOrder(const QString& userCustom, const QString& userOrderLocalId, const QString& instrumentId, const char& direction, const double& orderPrice, const int& orderVolume,
												const int& remainVolume, const int& tradeVolume, const char& offsetFlag, const char& priceType, const char& hedgeFlag, const char& orderStatus,
												const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId, const QString& seatId, const char& timeCondition)
{	
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, orderStatus, direction, orderPrice, orderVolume, remainVolume, tradeVolume, offsetFlag, priceType, hedgeFlag, mOrderPrice);
	if(mFirstOrderRef == userOrderLocalId){
		mFirstInsStatus = orderStatus;
		mFirstRemainQty = remainVolume;
		if (USTP_FTDC_OS_Canceled == orderStatus && (false == mIsAutoFirstCancel) && (!mIsOppnentPrice)){
			double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;
			if(USTP_FTDC_D_Buy == direction && mFirstMarketBasePrice <= fSecBasePrice){	//买委托满足下单条件
				mFirstOrderPrice = mFirstMarketBasePrice + mFirstSlipPrice;
				if(mFirstOrderPrice > fSecBasePrice)
					mFirstOrderPrice = fSecBasePrice;
				mSecondOrderBasePrice = mSecondMarketBasePrice;
				submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, priceType, USTP_FTDC_TC_GFD, true);

			}else if(USTP_FTDC_D_Sell == direction && mFirstMarketBasePrice >= fSecBasePrice){//卖委托满足下单条件
				mFirstOrderPrice = mFirstMarketBasePrice - mFirstSlipPrice;
				if(mFirstOrderPrice < fSecBasePrice)
					mFirstOrderPrice = fSecBasePrice;
				mSecondOrderBasePrice = mSecondMarketBasePrice;
				submitOrder(FIRST_INSTRUMENT, mFirstIns, mFirstOrderPrice, mBS, mFirstRemainQty, priceType, USTP_FTDC_TC_GFD, true);
			}
		}

		if (USTP_FTDC_OS_Canceled == orderStatus && mIsDeleted){
			emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
				mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, mIsOppnentPrice, mIsDefineOrder, false, mIsActionReferTick, mOrderType);
		}
		if(USTP_FTDC_OS_Canceled == orderStatus)
			USTPMutexId::updateActionNum(instrumentId);
	}else{
		mSecInsStatus = orderStatus;
		mSecondRemainQty = remainVolume;
		if(USTP_FTDC_OS_Canceled == orderStatus && remainVolume > 0){
			orderSecondIns(false, remainVolume, orderPrice, orderPrice);
		}
	}
}


void USTPOpponentStareArbitrage::doUSTPRtnTrade(const QString& tradeId, const QString& instrumentId, const char& direction, const int& tradeVolume, const double& tradePrice,
												const char& offsetFlag, const char& hedgeFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId,
												const QString& seatId, const QString& userOrderLocalId, const QString& tradeTime)
{
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	if(mFirstOrderRef == userOrderLocalId){
		mFirstTradeQty += tradeVolume;
		orderSecondIns(true, tradeVolume, 0.0, 0.0);
	}else{
		mSecondTradeQty += tradeVolume;
	}
	if(mFirstTradeQty >= mOrderQty && mSecondTradeQty >= mOrderQty){
		emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
			mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, true, mIsActionReferTick, mOrderType);
	}
}

void USTPOpponentStareArbitrage::orderSecondIns(bool isInit, const int& qty, const double& bidPrice, const double& askPrice)
{	
	if (mIsCanMarket){
		if (USTP_FTDC_D_Buy == mBS)
			submitOrder(SECOND_INSTRUMENT, mSecIns, 0.0, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_BestPrice, USTP_FTDC_TC_GFD, false);
		else
			submitOrder(SECOND_INSTRUMENT, mSecIns, 0.0, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_BestPrice, USTP_FTDC_TC_GFD, false);
	}else{
		if(USTP_FTDC_D_Buy == mBS){
			if(isInit){
				double initPrice = mSecondOrderBasePrice - mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, initPrice, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}else{
				double price = bidPrice - mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, price, USTP_FTDC_D_Sell, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}
		}else{
			if(isInit){
				double initPrice = mSecondOrderBasePrice + mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, initPrice, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}else{
				double price = askPrice + mSecondSlipPrice;
				submitOrder(SECOND_INSTRUMENT, mSecIns, price, USTP_FTDC_D_Buy, qty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, false);
			}
		}
	}
}

void USTPOpponentStareArbitrage::doUSTPErrRtnOrderInsert(const QString& userCustom, const QString& brokerId, const char& direction, const QString& exchangeId, const char& hedgeFlag,
														 const QString& instrumentId, const QString& investorId, const char& offsetFlag, const char& priceType, const char& timeCondition,
														 const QString& userOrderLocalId, const double& orderPrice, const int& volume, const int& errorId, const QString& errorMsg)
{	
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
	if(mFirstOrderRef == userOrderLocalId)	//设置合约状态
		mFirstInsStatus = USTP_FTDC_OS_ORDER_ERROR;
	else
		mSecInsStatus = USTP_FTDC_OS_ORDER_ERROR;

	switch (errorId){
	case 12:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'D', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice); //重复的报单
		break;
	case 31:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'P', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//平仓位不足
		break;
	case 36:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'Z', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//	资金不足
		break;
	case 8129:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'J', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//交易编码不存在
		break;
	default:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'W', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);
		break;
	}
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderInsert] UserCustom: ")) + userCustom +  QString(tr("  UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}

void USTPOpponentStareArbitrage::doUSTPErrRtnOrderAction(const char& actionFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId,
														 const QString& orderSysId, const QString& userActionLocalId, const QString& userOrderLocalId, const double& orderPrice, 
														 const int& volumeChange, const int& errorId, const QString& errorMsg)
{
	if(mFirstOrderRef != userOrderLocalId && mSecOrderRef != userOrderLocalId)
		return;
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderAction] UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  UserActionLocalId: ")) + userActionLocalId  + QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}


void USTPOpponentStareArbitrage::doAutoCancelFirstIns()
{	
	if(isInMarket(mFirstInsStatus))
		cancelFirstIns();
}

void USTPOpponentStareArbitrage::cancelFirstIns()
{
	mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
	submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
}

void USTPOpponentStareArbitrage::doAutoCancelSecIns()
{	
	if(isInMarket(mSecInsStatus)){
		mSecInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
		submitAction(SECOND_INSTRUMENT, mSecOrderRef, mSecIns);
	}
}

void USTPOpponentStareArbitrage::doDefineTimeOrderFirstIns()
{
	if((mFirstMarketBasePrice < 0.1) || (mSecondMarketBasePrice < 0.1) || (mFirstBidMarketPrice < 0.1) || (mFirstAskMarketPrice < 0.1))	//保证两腿都收到行情
		return;
	double fSecBasePrice = mSecondMarketBasePrice + mOrderPrice;
	if(mIsOppnentPrice)
		opponentPriceOrder(fSecBasePrice);
	else
		defineTimeOrder(fSecBasePrice);
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [DefineTimeOrder] InstrumentId: ")) + mFirstIns + QString(tr("  mSecondMarketBasePrice: ")) + QString::number(fSecBasePrice) +
		QString(tr("  ReferenceIns: ")) + mReferenceIns;
	USTPLogger::saveData(data);
#endif
}

void USTPOpponentStareArbitrage::doDelOrder(const QString& orderStyle)
{
	if(orderStyle == mOrderLabel){
		mIsDeleted = true;
		if(isInMarket(mFirstInsStatus)){
			mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
			submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
		}else if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus || USTP_FTDC_OS_ORDER_ERROR == mFirstInsStatus || USTP_FTDC_OS_Canceled == mFirstInsStatus){
			emit onUpdateOrderShow(mUserCustomLabel, mFirstIns, mOrderLabel, USTP_FTDC_OS_Canceled, mBS, 0.0, mOrderQty, mOrderQty, 0, mOffsetFlag, USTP_FTDC_OPT_LimitPrice, mHedgeFlag, mOrderPrice);
			emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
				mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, false, mIsActionReferTick, mOrderType);
		}
		QString data = mOrderLabel + QString(tr("  [DoDelOrder]   mFirstInsStatus: ")) + QString(mFirstInsStatus);
		USTPLogger::saveData(data);
	}
}


void USTPOpponentStareArbitrage::submitAction(const QString& insLabel, const QString& orderLocalId, const QString& instrument)
{	
	USTPTradeApi::reqOrderAction(mBrokerId, mUserId, mInvestorId, instrument, orderLocalId);
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderAction]   UserLocalOrderId: ")) + orderLocalId + QString(tr("  InstrumentId: ")) + instrument;
	USTPLogger::saveData(data);
#endif	
}


USTPSpeculateOrder::USTPSpeculateOrder(const QString& orderLabel, const QString& speLabel, const QString& firstIns, const QString& secIns, const double& orderPriceTick, const int& qty, const char& bs,  const char& offset,
									   const char& hedge, const int& cancelFirstTime, const int& cancelSecTime, const int& cycleStall, const int& firstSlipPoint, const int& secSlipPoint, bool isAutoFirstCancel, bool isAutoSecCancel, bool isCycle,
									   USTPOrderWidget* pOrderWidget, USTPCancelWidget* pCancelWidget, USTPSubmitWidget* pSubmitWidget)
									   :USTPStrategyBase(orderLabel, firstIns, secIns, orderPriceTick, qty, bs, offset, hedge, 0, 0, 0, 0, 0, false, false, false)
{		
	moveToThread(&mStrategyThread);
	mStrategyThread.start();
	mOrderType = 5;
	mTempOrderQty = 0;
	mTempOrderPrice = 0.0;
	mSpeOrderLabel = speLabel;
	mStrategyLabel = "SpeculateOrder_";
	mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	mBrokerId = USTPCtpLoader::getBrokerId();
	mUserId = USTPMutexId::getUserId();
	mInvestorId = USTPMutexId::getInvestorId();
	initConnect(pSubmitWidget, pOrderWidget, pCancelWidget);
}

USTPSpeculateOrder::~USTPSpeculateOrder()
{
	mStrategyThread.quit();
	mStrategyThread.wait();
}

void USTPSpeculateOrder::initConnect(USTPSubmitWidget* pSubmitWidget, USTPOrderWidget* pOrderWidget, USTPCancelWidget* pCancelWidget)
{
	connect(pSubmitWidget, SIGNAL(onSubmitOrder(const QString&, const QString&, const QString&, const char&, const char&, const char&, const int&, const double&)), 
		this, SLOT(doSubmitOrder(const QString&, const QString&, const QString&, const char&, const char&, const char&, const int&, const double&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)),
		this, SLOT(doUSTPRtnOrder(const QString&, const QString&, const QString&, const char&, const double&, const int&,
		const int&, const int&, const char&, const char&, const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const char&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderInsert(const QString&, const QString&, const char&, const QString&, const char&,
		const QString&, const QString&, const char&, const char&, const char&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)),
		this, SLOT(doUSTPErrRtnOrderAction(const char&, const QString&, const QString&, const QString&,
		const QString&, const QString&, const QString&, const double&, const int&, const int&, const QString&)), Qt::QueuedConnection);

	connect(USTPCtpLoader::getTradeSpi(), SIGNAL(onUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)),
		this, SLOT(doUSTPRtnTrade(const QString&, const QString&, const char&, const int&, const double&,
		const char&, const char&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, const QString&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pOrderWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(this, SIGNAL(onUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), 
		pCancelWidget, SLOT(doUpdateOrderShow(const QString&, const QString&, const QString&, const char&, const char&, const double& , const int&, const int&, const int&, const char&, const char&, const char&, const double&)), Qt::QueuedConnection);

	connect(pCancelWidget, SIGNAL(onDelOrder(const QString& )), this, SLOT(doDelOrder(const QString& )), Qt::QueuedConnection);

	connect(this, SIGNAL(onOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool, bool, bool, bool, bool, bool, const int&)), pCancelWidget, SLOT(doOrderFinished(const QString&, const QString&, const QString&, const double&, const int&, const int&, const int&, const char&,  const char&, const char&, const int&, const int&, const int&, const int&, const int&, 
		bool, bool, bool, bool, bool, bool, bool, const int&)), Qt::QueuedConnection);
}

void USTPSpeculateOrder::doSubmitOrder(const QString& orderLabel, const QString& speLabel, const QString& ins, const char& direction, const char& hedgeFlag, const char& offsetFlag, const int& volume, const double& orderPrice)
{	
	if(mSpeOrderLabel != speLabel)
		return;
#ifdef _DEBUG
	QString data = orderLabel + tr("  [SubmitOrder]   Instrument: ") + ins + QString(tr("  OrderPrice: ")) + 
		QString::number(orderPrice) + QString(tr("  OrderVolume: ")) + QString::number(volume);
	USTPLogger::saveData(data);
#endif
	if(USTPStrategyBase::isInMarket(mFirstInsStatus)){
		mTempOrderLabel = orderLabel;
		mTempOffsetFlag = offsetFlag;
		mTempHedgeFlag = hedgeFlag;
		mTempBS = direction;
		mTempFirstIns = ins;
		mTempOrderQty = volume;
		mTempOrderPrice = orderPrice;
		mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
		submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);

	}else if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus || USTP_FTDC_OS_Canceled == mFirstInsStatus){
		mOrderLabel = orderLabel;
		mOffsetFlag = offsetFlag;
		mHedgeFlag = hedgeFlag;
		mBS = direction;
		mFirstIns = ins;
		mOrderQty = volume;
		mOrderPrice = orderPrice;
		mFirstRemainQty = volume;
		submitOrder(FIRST_INSTRUMENT, ins, orderPrice, direction, volume, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, true);
	}
}

void USTPSpeculateOrder::submitOrder(const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	mUserCustomLabel = mStrategyLabel + QString::number(USTPMutexId::getMutexId());
	orderInsert(mUserCustomLabel, insLabel, instrument, orderPrice, direction, qty, priceType, timeCondition, isFirstIns);	

}

void USTPSpeculateOrder::orderInsert(const QString& insCustom, const QString& insLabel, const QString& instrument, const double& orderPrice, const char& direction, const int& qty, const char& priceType, const char& timeCondition, bool isFirstIns)
{	
	double adjustPrice = (priceType == USTP_FTDC_OPT_LimitPrice) ? orderPrice : 0.0;
	QString orderRef;
	mFirstInsStatus = USTP_FTDC_OS_ORDER_SUBMIT;
	USTPTradeApi::reqOrderInsert(insCustom, mBrokerId, mUserId, mInvestorId, instrument, priceType, timeCondition, adjustPrice, qty, direction, mOffsetFlag, mHedgeFlag, USTP_FTDC_VC_AV, orderRef);
	mFirstOrderRef = orderRef;
	emit onUpdateOrderShow(insCustom, instrument, mOrderLabel, 'N', direction, 0.0, qty, qty, 0, mOffsetFlag, priceType, mHedgeFlag, mOrderPrice);
#ifdef _DEBUG
	int nIsSecCancel = mIsAutoSecondCancel ? 1 : 0;
	int nIsSecIns = isFirstIns ? 0 : 1;
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderInsert]   Instrument: ")) + instrument + QString(tr("  OrderRef: ")) + orderRef + 
		QString(tr("  UserId: ")) + mUserId + QString(tr("  PriceType: ")) + QString(priceType) + QString(tr("  OrderPrice: ")) + QString::number(adjustPrice) + QString(tr("  OrderVolume: ")) + 
		QString::number(qty) + QString(tr("  Direction: ")) + QString(direction) + QString(tr("  SecAutoCancel: ")) + QString::number(nIsSecCancel) + QString(tr("  IsSecIns: ")) + QString::number(nIsSecIns);
	USTPLogger::saveData(data);
#endif	
}

void USTPSpeculateOrder::doUSTPRtnOrder(const QString& userCustom, const QString& userOrderLocalId, const QString& instrumentId, const char& direction, const double& orderPrice, const int& orderVolume,
										const int& remainVolume, const int& tradeVolume, const char& offsetFlag, const char& priceType, const char& hedgeFlag, const char& orderStatus,
										const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId, const QString& seatId, const char& timeCondition)
{	
	if(mFirstOrderRef != userOrderLocalId)
		return;
	emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, orderStatus, direction, orderPrice, orderVolume, remainVolume, tradeVolume, offsetFlag, priceType, hedgeFlag, orderPrice);
	mFirstInsStatus = orderStatus;
	mFirstRemainQty = remainVolume;
	if (USTP_FTDC_OS_Canceled == orderStatus){		
		USTPMutexId::updateActionNum(instrumentId);
		emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
			mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, false, mIsActionReferTick, mOrderType);
#ifdef _DEBUG
		QString data = mOrderLabel + QString(tr("  [")) + userOrderLocalId + QString(tr("-OrderCancel]   Instrument: ")) + instrumentId + QString(tr("  TempOrderLabel: ")) + mTempOrderLabel + 
			QString(tr("  TempOrderPrice: ")) + QString::number(mTempOrderPrice) + QString(tr("  OrderVolume: ")) + QString::number(mOrderQty);
		USTPLogger::saveData(data);
#endif	
		if(mTempOrderLabel == "")
			return;
		mOrderLabel = mTempOrderLabel;
		mFirstIns = mTempFirstIns;
		mOrderPrice = mTempOrderPrice;
		mBS = mTempBS;
		mOrderQty = mTempOrderQty;
		mOffsetFlag = mTempOffsetFlag;
		mHedgeFlag = mTempHedgeFlag;
		submitOrder(FIRST_INSTRUMENT, mFirstIns, mOrderPrice, mBS, mOrderQty, USTP_FTDC_OPT_LimitPrice, USTP_FTDC_TC_GFD, true);
	}else if(USTP_FTDC_OS_AllTraded == orderStatus){
		emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
			mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, true, mIsActionReferTick, mOrderType);
		mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
	}
}


void USTPSpeculateOrder::doUSTPRtnTrade(const QString& tradeId, const QString& instrumentId, const char& direction, const int& tradeVolume, const double& tradePrice,
										const char& offsetFlag, const char& hedgeFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId,
										const QString& seatId, const QString& userOrderLocalId, const QString& tradeTime)
{
}


void USTPSpeculateOrder::doUSTPErrRtnOrderInsert(const QString& userCustom, const QString& brokerId, const char& direction, const QString& exchangeId, const char& hedgeFlag,
												 const QString& instrumentId, const QString& investorId, const char& offsetFlag, const char& priceType, const char& timeCondition,
												 const QString& userOrderLocalId, const double& orderPrice, const int& volume, const int& errorId, const QString& errorMsg)
{	
	if(mFirstOrderRef != userOrderLocalId)
		return;

	if(mFirstIns == instrumentId)
		mFirstInsStatus = USTP_FTDC_OS_ORDER_ERROR;

	switch (errorId){
	case 22:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'D', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice); //重复的报单
		break;
	case 31:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'Z', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//	资金不足
		break;
	case 42:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'S', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//	结算结果未确认
		break;
	case 50:
	case 51:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'P', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);	//平仓位不足
		break;
	default:
		emit onUpdateOrderShow(userCustom, instrumentId, mOrderLabel, 'W', direction, orderPrice, volume, volume, 0, offsetFlag, priceType, hedgeFlag, mOrderPrice);
		break;
	}
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderInsert] UserCustom: ")) + userCustom +  QString(tr("  UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}

void USTPSpeculateOrder::doUSTPErrRtnOrderAction(const char& actionFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId,
												 const QString& orderSysId, const QString& userActionLocalId, const QString& userOrderLocalId, const double& orderPrice, 
												 const int& volumeChange, const int& errorId, const QString& errorMsg)
{
#ifdef _DEBUG
	if(mFirstOrderRef != userOrderLocalId)
		return;
	QString data = mOrderLabel + QString(tr("  [ErrRtnOrderAction] UserOrderLocalId: ")) + userOrderLocalId + 
		QString(tr("  UserActionLocalId: ")) + userActionLocalId  + QString(tr("  ErrorId: ")) + QString::number(errorId) + QString(tr("  ErrorMsg: ")) + errorMsg;
	USTPLogger::saveData(data);
#endif
}

void USTPSpeculateOrder::doDelOrder(const QString& orderStyle)
{
	if(orderStyle == mOrderLabel){
		if(isInMarket(mFirstInsStatus)){
			mTempOrderLabel = "";
			mFirstInsStatus = USTP_FTDC_OS_CANCEL_SUBMIT;
			submitAction(FIRST_INSTRUMENT, mFirstOrderRef, mFirstIns);
		}else if(USTP_FTDC_OS_ORDER_NO_ORDER == mFirstInsStatus || USTP_FTDC_OS_ORDER_ERROR == mFirstInsStatus || USTP_FTDC_OS_Canceled == mFirstInsStatus){
			emit onUpdateOrderShow(mUserCustomLabel, mFirstIns, mOrderLabel, USTP_FTDC_OS_Canceled, mBS, 0.0, mOrderQty, mOrderQty, 0, mOffsetFlag, USTP_FTDC_OPT_LimitPrice, mHedgeFlag, mOrderPrice);
			emit onOrderFinished(mOrderLabel, mFirstIns, mSecIns, mOrderPrice, mFirstSlipPoint, mSecSlipPoint, mOrderQty, mBS, mOffsetFlag, mHedgeFlag, mFirstCancelTime,
				mSecondCancelTime, mCycleStall, mActionReferNum, mActionSuperNum, mIsAutoFirstCancel, mIsAutoSecondCancel, mIsCycle, false, false, false, mIsActionReferTick, mOrderType);
			mFirstInsStatus = USTP_FTDC_OS_ORDER_NO_ORDER;
		}
		QString data = mOrderLabel + QString(tr("  [DoDelOrder]   mFirstInsStatus: ")) + QString(mFirstInsStatus);
		USTPLogger::saveData(data);
	}
}

void USTPSpeculateOrder::submitAction(const QString& insLabel, const QString& orderLocalId, const QString& instrument)
{	
	USTPTradeApi::reqOrderAction(mBrokerId, mUserId, mInvestorId, instrument, orderLocalId);
#ifdef _DEBUG
	QString data = mOrderLabel + QString(tr("  [")) + insLabel + QString(tr("-OrderAction]   UserLocalOrderId: ")) + orderLocalId + QString(tr("  InstrumentId: ")) + instrument;
	USTPLogger::saveData(data);
#endif
}

#include "moc_USTPUserStrategy.cpp"