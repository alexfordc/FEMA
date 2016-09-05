#ifndef USTP_MD_SPI_H
#define USTP_MD_SPI_H

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include "USTPFtdcMduserApi.h"

class USTPMdSpi : public QObject, public CUstpFtdcMduserSpi
{
	Q_OBJECT
public:

	USTPMdSpi();

	virtual~USTPMdSpi();

	virtual void OnFrontConnected();

	virtual void OnFrontDisconnected(int nReason);

	virtual void OnRspUserLogin(CUstpFtdcRspUserLoginField *pRspUserLogin, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	virtual void OnRtnDepthMarketData(CUstpFtdcDepthMarketDataField *pDepthMarketData);

signals:
	void onUSTPMdFrontConnected();
	void onUSTPMdFrontDisconnected(int reason);
	void onUSTPMdRspUserLogin(const QString& brokerId, const QString& userId, const int& errorId, const QString& errorMsg, bool bIsLast);
	void onUSTPRtnDepthMarketData(const QString& instrumentId, const double& preSettlementPrice, const double& openPrice, const double& lastPrice,
		const double& bidPrice, const int& bidVolume, const double& askPrice, const double& askVolume, const double& highestPrice, const double& lowestPrice);

private:
	QThread mMdThread;
};

#endif