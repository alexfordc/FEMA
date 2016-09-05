#include "USTPTradeSpi.h"
#include "USTPConfig.h"
#include "USTPLogger.h"
#include <QtCore/QThread>
#include <QtCore/QDebug>

USTPTradeSpi::USTPTradeSpi()
{
	moveToThread(&mTradeThread);
	mTradeThread.start();
}

USTPTradeSpi::~USTPTradeSpi()
{
	mTradeThread.quit();
	mTradeThread.wait();
}

void USTPTradeSpi::OnFrontConnected()
{	
	emit onUSTPTradeFrontConnected();
	QString data = QString(tr("[Trade-Connected]  "));
	USTPLogger::saveData(data);
}

void USTPTradeSpi::OnFrontDisconnected(int nReason)
{	
	emit onUSTPTradeFrontDisconnected(nReason);
	QString data = QString(tr("[Trade-Disconneted]  ")) +  QString::number(nReason);
	USTPLogger::saveData(data);
}

void USTPTradeSpi::OnRspError(CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
#ifdef _DEBUG
	QString data = QString(tr("[Trade-RspError]  ErrorId:")) + QString::number(pRspInfo->ErrorID) + tr("  ErrorMsg: ") + QString::fromLocal8Bit(pRspInfo->ErrorMsg);
	USTPLogger::saveData(data);
#endif
}

void USTPTradeSpi::OnRspUserLogin(CUstpFtdcRspUserLoginField *pRspUserLogin, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspUserLogin != NULL && pRspInfo != NULL){
		emit onUSTPTradeRspUserLogin(QString(pRspUserLogin->TradingDay), QString(pRspUserLogin->BrokerID), QString(pRspUserLogin->UserID),
			atoi(pRspUserLogin->MaxOrderLocalID), pRspInfo->ErrorID, QString::fromLocal8Bit(pRspInfo->ErrorMsg), bIsLast);
#ifdef _DEBUG
		QString data = QString(tr("[Trade-RspUserLogin]  UserId: ")) + QString(pRspUserLogin->UserID) + tr("  TradingDate: ") + QString(pRspUserLogin->TradingDay)
			+ tr("  ErrorMsg: ") + QString::fromLocal8Bit(pRspInfo->ErrorMsg);
		USTPLogger::saveData(data);
#endif
	}
}

void USTPTradeSpi::OnRspUserLogout(CUstpFtdcRspUserLogoutField *pRspUserLogout, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspUserLogout != NULL && pRspInfo != NULL){
		emit onUSTPTradeRspUserLogout(QString(pRspUserLogout->BrokerID), QString(pRspUserLogout->UserID), pRspInfo->ErrorID, QString::fromLocal8Bit(pRspInfo->ErrorMsg));
	}
}

void USTPTradeSpi::OnRspQryUserInvestor(CUstpFtdcRspUserInvestorField *pRspUserInvestor, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspUserInvestor != NULL && pRspInfo != NULL){
		emit onUSTPRspQryUserInvestor(QString(pRspUserInvestor->UserID), QString(pRspUserInvestor->InvestorID), pRspInfo->ErrorID, QString(pRspInfo->ErrorMsg), bIsLast);
#ifdef _DEBUG
		QString data = QString(tr("[RspQryInvestor]  UserId: ")) + QString(pRspUserInvestor->UserID) + tr("  InvestorId: ") + QString(pRspUserInvestor->InvestorID)
			+ tr("  ErrorMsg: ") + QString::fromLocal8Bit(pRspInfo->ErrorMsg);
		USTPLogger::saveData(data);
#endif
	}
}

void USTPTradeSpi::OnRspOrderInsert(CUstpFtdcInputOrderField *pInputOrder, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pInputOrder != NULL && pRspInfo != NULL){
		emit onUSTPRspOrderInsert(QString(pInputOrder->BrokerID), pInputOrder->Direction, QString(pInputOrder->ExchangeID), pInputOrder->HedgeFlag,
			QString(pInputOrder->InstrumentID), QString(pInputOrder->InvestorID), pInputOrder->OffsetFlag, pInputOrder->OrderPriceType, pInputOrder->TimeCondition,
			QString(pInputOrder->UserOrderLocalID), pInputOrder->LimitPrice, pInputOrder->Volume, pRspInfo->ErrorID, QString(pRspInfo->ErrorMsg));
#ifdef _DEBUG
		QString data = QString(tr("[RspOrderInsert]  InvestorId: ")) + QString(pInputOrder->InvestorID) + tr("  UserOrderLocalId: ") + QString(pInputOrder->UserOrderLocalID)
			+ tr("  InstrumentId: ") + QString(pInputOrder->InstrumentID) + tr("  OrderPrice: ") + QString::number(pInputOrder->LimitPrice);
		USTPLogger::saveData(data);
#endif
	}
}

void USTPTradeSpi::OnRspOrderAction(CUstpFtdcOrderActionField *pOrderAction, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{	
	if (pOrderAction != NULL && pRspInfo != NULL){
		emit onUSTPRspOrderAction(pOrderAction->ActionFlag, QString(pOrderAction->BrokerID), QString(pOrderAction->ExchangeID), QString(pOrderAction->InvestorID),
			QString(pOrderAction->OrderSysID), QString(pOrderAction->UserOrderActionLocalID), QString(pOrderAction->UserOrderLocalID), pOrderAction->LimitPrice, 
			pOrderAction->VolumeChange, pRspInfo->ErrorID, QString::fromLocal8Bit(pRspInfo->ErrorMsg));
#ifdef _DEBUG
		QString data = QString(tr("[RspOrderAction]  InvestorId: ")) + QString(pOrderAction->InvestorID) + tr("  UserOrderLocalId: ") + QString(pOrderAction->UserOrderLocalID)
			+ tr("  OrderActionLocalId: ") + QString(pOrderAction->UserOrderActionLocalID) + tr("  ErrorMsg: ") + QString::fromLocal8Bit(pRspInfo->ErrorMsg);
		USTPLogger::saveData(data);
#endif
	}
}

void USTPTradeSpi::OnRtnTrade(CUstpFtdcTradeField *pTrade)
{
	if (pTrade != NULL){
		emit onUSTPRtnTrade(QString(pTrade->TradeID), QString(pTrade->InstrumentID), pTrade->Direction, pTrade->TradeVolume, pTrade->TradePrice,
			pTrade->OffsetFlag, pTrade->HedgeFlag, QString(pTrade->BrokerID), QString(pTrade->ExchangeID), QString(pTrade->InvestorID), 
			QString(pTrade->OrderSysID), QString(pTrade->SeatID), QString(pTrade->UserOrderLocalID), QString(pTrade->TradeTime));
#ifdef _DEBUG
		QString data = QString(tr("[OnRtnTrade]  InvestorId: ")) + QString(pTrade->InvestorID) + tr("  UserOrderLocalId: ") + QString(pTrade->UserOrderLocalID)
			+ tr("  InstrumentId: ") + QString(pTrade->InstrumentID) + tr("  Direction: ") + QString(pTrade->Direction) + tr("  TradePrice: ") + 
			QString::number(pTrade->TradePrice) + tr("  TradeTime: ") + QString(pTrade->TradeTime);
		USTPLogger::saveData(data);
#endif
	}
}

void USTPTradeSpi::OnRtnOrder(CUstpFtdcOrderField *pOrder)
{	
	if (pOrder != NULL){
		emit onUSTPRtnOrder(QString(pOrder->UserCustom), QString(pOrder->UserOrderLocalID), QString(pOrder->InstrumentID), pOrder->Direction, pOrder->LimitPrice, pOrder->Volume,
			pOrder->VolumeRemain, pOrder->VolumeTraded, pOrder->OffsetFlag, pOrder->OrderPriceType, pOrder->HedgeFlag, pOrder->OrderStatus,
			QString(pOrder->BrokerID), QString(pOrder->ExchangeID), QString(pOrder->InvestorID), QString(pOrder->OrderSysID), QString(pOrder->SeatID), 
			pOrder->TimeCondition);
#ifdef _DEBUG
		QString data = QString(tr("[OnRtnOrder]  InvestorId: ")) + QString(pOrder->InvestorID) + tr("  UserOrderLocalId: ") + QString(pOrder->UserOrderLocalID)
			+ tr("  InstrumentId: ") + QString(pOrder->InstrumentID) + tr("  Direction: ") + QString(pOrder->Direction) + tr("  OrderPrice: ") + 
			QString::number(pOrder->LimitPrice) + tr("  OrderStatus: ") + QString(pOrder->OrderStatus) + tr("  UserCustom: ") + QString(pOrder->UserCustom);
		USTPLogger::saveData(data);
#endif
	}
}

void USTPTradeSpi::OnErrRtnOrderInsert(CUstpFtdcInputOrderField *pInputOrder, CUstpFtdcRspInfoField *pRspInfo)
{
	if (pInputOrder != NULL && pRspInfo != NULL){
		emit onUSTPErrRtnOrderInsert(QString(pInputOrder->UserCustom), QString(pInputOrder->BrokerID), pInputOrder->Direction, QString(pInputOrder->ExchangeID), pInputOrder->HedgeFlag,
			QString(pInputOrder->InstrumentID), QString(pInputOrder->InvestorID), pInputOrder->OffsetFlag, pInputOrder->OrderPriceType, pInputOrder->TimeCondition,
			QString(pInputOrder->UserOrderLocalID), pInputOrder->LimitPrice, pInputOrder->Volume, pRspInfo->ErrorID, QString::fromLocal8Bit(pRspInfo->ErrorMsg));
#ifdef _DEBUG
		QString data = QString(tr("[ErrRtnOrderInsert]  InvestorId: ")) + QString(pInputOrder->InvestorID) + tr("  UserOrderLocalId: ") + QString(pInputOrder->UserOrderLocalID)
			+ tr("  InstrumentId: ") + QString(pInputOrder->InstrumentID) + tr("  Direction: ") + QString(pInputOrder->Direction) + tr("  OrderPrice: ") + 
			QString::number(pInputOrder->LimitPrice) + tr("  UserCustom: ") + QString(pInputOrder->UserCustom) + tr("  ErrorMsg: ") +  QString::fromLocal8Bit(pRspInfo->ErrorMsg);
		USTPLogger::saveData(data);
#endif
	}
}

void USTPTradeSpi::OnErrRtnOrderAction(CUstpFtdcOrderActionField *pOrderAction, CUstpFtdcRspInfoField *pRspInfo)
{
	if (pOrderAction != NULL && pRspInfo != NULL){
		emit onUSTPErrRtnOrderAction(pOrderAction->ActionFlag, QString(pOrderAction->BrokerID), QString(pOrderAction->ExchangeID), QString(pOrderAction->InvestorID),
			QString(pOrderAction->OrderSysID), QString(pOrderAction->UserOrderActionLocalID), QString(pOrderAction->UserOrderLocalID), pOrderAction->LimitPrice, 
			pOrderAction->VolumeChange, pRspInfo->ErrorID, QString::fromLocal8Bit(pRspInfo->ErrorMsg));
#ifdef _DEBUG
		QString data = QString(tr("[ErrRtnOrderAction]  InvestorId: ")) + QString(pOrderAction->InvestorID) + tr("  UserOrderLocalId: ") + QString(pOrderAction->UserOrderLocalID)
			+ tr("  OrderActionLocalId: ") + QString(pOrderAction->UserOrderActionLocalID) + tr("  ErrorMsg: ") + QString::fromLocal8Bit(pRspInfo->ErrorMsg);
		USTPLogger::saveData(data);
#endif
	}
}

void USTPTradeSpi::OnRtnInstrumentStatus(CUstpFtdcInstrumentStatusField *pInstrumentStatus)
{
	if (pInstrumentStatus != NULL){
		emit onUSTPRtnInstrumentStatus(QString(pInstrumentStatus->ExchangeID), QString(pInstrumentStatus->InstrumentID), pInstrumentStatus->InstrumentStatus,
			QString(pInstrumentStatus->ProductID), pInstrumentStatus->PriceTick, pInstrumentStatus->VolumeMultiple);
	}
}

void USTPTradeSpi::OnRspQryInstrument(CUstpFtdcRspInstrumentField *pRspInstrument, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspInstrument != NULL && pRspInfo != NULL){
		emit onUSTPRspQryInstrument(QString(pRspInstrument->ExchangeID), QString(pRspInstrument->ProductID), QString(pRspInstrument->InstrumentID), pRspInstrument->InstrumentStatus,
			 pRspInstrument->PriceTick, pRspInstrument->VolumeMultiple, pRspInstrument->MaxMarketOrderVolume, pRspInfo->ErrorID, QString::fromLocal8Bit(pRspInfo->ErrorMsg), bIsLast);
	}

}

void USTPTradeSpi::OnRspQryInvestorPosition(CUstpFtdcRspInvestorPositionField *pRspInvestorPosition, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if(pRspInvestorPosition == NULL){
		emit onUSTPRspQryInvestorPosition("", -1, 0, 0, '1', "", "","", pRspInfo->ErrorID, QString::fromLocal8Bit(pRspInfo->ErrorMsg), bIsLast);
		return;
	}
	if (pRspInvestorPosition != NULL  && pRspInfo != NULL){	
		emit onUSTPRspQryInvestorPosition(QString(pRspInvestorPosition->InstrumentID), pRspInvestorPosition->Direction, pRspInvestorPosition->Position, 
			pRspInvestorPosition->YdPosition, pRspInvestorPosition->HedgeFlag, QString(pRspInvestorPosition->BrokerID), QString(pRspInvestorPosition->ExchangeID),
			QString(pRspInvestorPosition->InvestorID), pRspInfo->ErrorID, QString::fromLocal8Bit(pRspInfo->ErrorMsg), bIsLast);
#ifdef _DEBUG
		QString data = QString(tr("[RspQryInvestorPosition]  InvestorId: ")) + QString(pRspInvestorPosition->InvestorID) + tr("  InstrumentId: ") + QString(pRspInvestorPosition->InstrumentID)
			+ tr("  Direction: ") + QString(pRspInvestorPosition->Direction) + tr("  Postion: ") + QString::number(pRspInvestorPosition->Position) + tr("  YPostion: ") +
			QString::number(pRspInvestorPosition->YdPosition);
		USTPLogger::saveData(data);
#endif
	}
}

#include "moc_USTPTradeSpi.cpp"