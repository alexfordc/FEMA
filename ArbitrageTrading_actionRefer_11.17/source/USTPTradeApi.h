#ifndef USTP_TRADE_API_H
#define USTP_TRADE_API_H

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include "USTPFtdcTraderApi.h"

class USTPTradeApi : public QObject
{
	Q_OBJECT
public:

	USTPTradeApi();

	virtual~USTPTradeApi();

public:

	static bool initialize(CUstpFtdcTraderApi* pTradeApi);

	static bool finalize();

	static int reqUserLogin(const QString& brokerId, const QString& userId, const QString& password);
	
	static int reqUserLogout(const QString& brokerId, const QString& userId);

	static int reqQryInvestor(const QString& brokerId, const QString& userId);
	
	static int reqOrderInsert(const QString& userCustom, const QString& brokerId, const QString& userId, const QString& investorId, const QString& instrumentId, const char& priceType, 
		const char& timeCondition, const double& orderPrice, const int& volume, const char& direction, const char& offsetFlag, const char& hedgeFlag, 
		const char& volumeCondition, QString& userLocalOrderId);

	static int reqOrderAction(const QString& brokerId, const QString& userId, const QString& investorId, const QString& instrumentId, const QString& orderRef);

	static int reqQryInstrument( const QString& instrumentId);

	static int reqQryInvestorPosition(const QString& brokerId, const QString& investorId, const QString& instrumentId);

private:
	static USTPTradeApi* mThis;
	CUstpFtdcTraderApi* m_pTradeApi;
	int nRequestId;
	int mUserLocalId;
	QMutex mOrderMutex;
};

#endif