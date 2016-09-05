#ifndef USTP_OMS_SPI_H
#define USTP_OMS_SPI_H

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include "USTPFtdcTraderApi.h"

class USTPTradeSpi : public QObject, public CUstpFtdcTraderSpi
{
	Q_OBJECT
public:

	USTPTradeSpi();

	virtual~USTPTradeSpi();

	virtual void OnFrontConnected();

	virtual void OnFrontDisconnected(int nReason);

	virtual void OnRspError(CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	virtual void OnRspUserLogin(CUstpFtdcRspUserLoginField *pRspUserLogin, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	virtual void OnRspUserLogout(CUstpFtdcRspUserLogoutField *pRspUserLogout, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	virtual void OnRspQryUserInvestor(CUstpFtdcRspUserInvestorField *pRspUserInvestor, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	virtual void OnRspOrderInsert(CUstpFtdcInputOrderField *pInputOrder, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	virtual void OnRspOrderAction(CUstpFtdcOrderActionField *pOrderAction, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	virtual void OnRtnTrade(CUstpFtdcTradeField *pTrade);

	virtual void OnRtnOrder(CUstpFtdcOrderField *pOrder);

	virtual void OnErrRtnOrderInsert(CUstpFtdcInputOrderField *pInputOrder, CUstpFtdcRspInfoField *pRspInfo);

	virtual void OnErrRtnOrderAction(CUstpFtdcOrderActionField *pOrderAction, CUstpFtdcRspInfoField *pRspInfo);

	virtual void OnRtnInstrumentStatus(CUstpFtdcInstrumentStatusField *pInstrumentStatus);

	virtual void OnRspQryInstrument(CUstpFtdcRspInstrumentField *pRspInstrument, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	virtual void OnRspQryInvestorPosition(CUstpFtdcRspInvestorPositionField *pRspInvestorPosition, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

signals:

	void onUSTPTradeFrontConnected();

	void onUSTPTradeFrontDisconnected(int reason);

	void onUSTPTradeRspUserLogin(const QString& tradingDate, const QString& brokerId, const QString& userId, const int& maxLocalId, const int& errorId, const QString& errorMsg, bool bIsLast);

	void onUSTPTradeRspUserLogout(const QString& brokerId, const QString& userId, const int& errorId, const QString& errorMsg);

	void onUSTPRspQryUserInvestor(const QString& userId, const QString& investorId, const int& errorId, const QString& errorMsg, bool bIsLast);

	void onUSTPRspOrderInsert(const QString& brokerId, const char& direction, const QString& exchangeId, const char& hedgeFlag,
		const QString& instrumentId, const QString& investorId, const char& offsetFlag, const char& priceType, const char& timeCondition,
		const QString& userOrderLocalId, const double& orderPrice, const int& volume, const int& errorId, const QString& errorMsg);

	void onUSTPRspOrderAction(const char& actionFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId,
		const QString& orderSysId, const QString& userActionLocalId, const QString& userOrderLocalId, const double& orderPrice, 
		const int& volumeChange, const int& errorId, const QString& errorMsg);

	void onUSTPRtnTrade(const QString& tradeId, const QString& instrumentId, const char& direction, const int& tradeVolume, const double& tradePrice,
		const char& offsetFlag, const char& hedgeFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId,
		const QString& seatId, const QString& userOrderLocalId, const QString& tradeTime);

	void onUSTPRtnOrder(const QString& userCustom, const QString& userOrderLocalId, const QString& instrumentId, const char& direction, const double& orderPrice, const int& orderVolume,
		const int& remainVolume, const int& tradeVolume, const char& offsetFlag, const char& priceType, const char& hedgeFlag, const char& orderStatus,
		const QString& brokerId, const QString& exchangeId, const QString& investorId, const QString& orderSysId, const QString& seatId, const char& timeCondition);

	void onUSTPErrRtnOrderInsert(const QString& userCustom, const QString& brokerId, const char& direction, const QString& exchangeId, const char& hedgeFlag,
		const QString& instrumentId, const QString& investorId, const char& offsetFlag, const char& priceType, const char& timeCondition,
		const QString& userOrderLocalId, const double& orderPrice, const int& volume, const int& errorId, const QString& errorMsg);

	void onUSTPErrRtnOrderAction(const char& actionFlag, const QString& brokerId, const QString& exchangeId, const QString& investorId,
		const QString& orderSysId, const QString& userActionLocalId, const QString& userOrderLocalId, const double& orderPrice, 
		const int& volumeChange, const int& errorId, const QString& errorMsg);

	void onUSTPRtnInstrumentStatus(const QString& exchangeId, const QString& instrumentId, const char& instrumentStatus, const QString& productId,
		const double& priceTick, const int& volumeMultiple);
	
	void onUSTPRspQryInstrument(const QString& exchangeId, const QString& productId, const QString& instrumentId, const char& instrumentStatus, const double& priceTick, 
		const int& volumeMultiple, const int& maxMarketVolume, const int& errorId, const QString& errorMsg, bool bIsLast);
	
	void onUSTPRspQryInvestorPosition(const QString& instrumentId, const char& direction, const int& position, const int& yPosition, const char& hedgeFlag,
		const QString& brokerId, const QString& exchangeId, const QString& investorId, const int& errorId, const QString& errorMsg, bool bIsLast);	

private:
	QThread mTradeThread;
};

#endif