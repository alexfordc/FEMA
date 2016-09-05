#include "USTPTradeApi.h"
#include <QtCore/QDebug>
#include<math.h>
#include "USTPMutexId.h"

#define CHAR_BUF_SIZE 128
#define INS_TICK 0.2
USTPTradeApi* USTPTradeApi::mThis = NULL;

bool USTPTradeApi::initialize(CUstpFtdcTraderApi* pTradeApi)
{	
	mThis = new USTPTradeApi();
	mThis->m_pTradeApi = pTradeApi;
	return true;
}

bool USTPTradeApi::finalize()
{
	if(mThis != NULL){
		delete mThis;
		mThis = NULL;
	}
	return true;
}

USTPTradeApi::USTPTradeApi()
{
	nRequestId = 0;
	m_pTradeApi = NULL;
}

USTPTradeApi::~USTPTradeApi()
{

}

int USTPTradeApi::reqUserLogin(const QString& brokerId, const QString& userId, const QString& password)
{
	if (mThis->m_pTradeApi != NULL){
		CUstpFtdcReqUserLoginField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, brokerId.toStdString().data());
		strcpy(req.UserID, userId.toStdString().data());
		strcpy(req.Password, password.toStdString().data());
		int nResult =mThis->m_pTradeApi->ReqUserLogin(&req, ++mThis->nRequestId);
		return nResult;
	}
	return -1;
}

int USTPTradeApi::reqUserLogout(const QString& brokerId, const QString& userId)
{
	if (mThis->m_pTradeApi != NULL){
		CUstpFtdcReqUserLogoutField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, brokerId.toStdString().data());
		strcpy(req.UserID, userId.toStdString().data());
		int nResult = mThis->m_pTradeApi->ReqUserLogout(&req, ++mThis->nRequestId);
		return nResult;
	}
	return -1;
}

int USTPTradeApi::reqQryInvestor(const QString& brokerId, const QString& userId)
{
	if (mThis->m_pTradeApi != NULL){
		CUstpFtdcQryUserInvestorField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, brokerId.toStdString().data());
		strcpy(req.UserID, userId.toStdString().data());
		int nResult = mThis->m_pTradeApi->ReqQryUserInvestor(&req,  ++mThis->nRequestId);
		return nResult;
	}
	return -1;
}

int USTPTradeApi::reqOrderInsert(const QString& userCustom, const QString& brokerId, const QString& userId, const QString& investorId, const QString& instrumentId, const char& priceType, 
								 const char& timeCondition, const double& orderPrice, const int& volume, const char& direction, const char& offsetFlag,
								 const char& hedgeFlag, const char& volumeCondition, QString& userLocalOrderId)
{
	if (mThis->m_pTradeApi != NULL){		
		QMutexLocker locker(&mThis->mOrderMutex);
		CUstpFtdcInputOrderField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, brokerId.toStdString().data());
		req.Direction = direction;
		strcpy(req.ExchangeID, "CFFEX");
		req.ForceCloseReason = USTP_FTDC_FCR_NotForceClose;
		req.HedgeFlag = hedgeFlag;
		strcpy(req.InstrumentID, instrumentId.toStdString().data());
		strcpy(req.InvestorID, investorId.toStdString().data());
		req.IsAutoSuspend = 0;
		req.LimitPrice = orderPrice;
		req.MinVolume = 0;
		req.OffsetFlag = offsetFlag;
		req.OrderPriceType = priceType;
		req.TimeCondition = timeCondition;
		strcpy(req.UserID, userId.toStdString().data());
		sprintf(req.UserOrderLocalID, "%012d", USTPMutexId::getUserLocalId());
		userLocalOrderId = QString(req.UserOrderLocalID);
		req.Volume = volume;
		req.VolumeCondition = volumeCondition;
		strcpy(req.UserCustom, userCustom.toStdString().data());
		int nResult = mThis->m_pTradeApi->ReqOrderInsert(&req, ++mThis->nRequestId);
		return nResult;
	}
	return -1;
}

int USTPTradeApi::reqOrderAction(const QString& brokerId, const QString& userId, const QString& investorId, const QString& instrumentId, const QString& orderRef)
{
	if (mThis->m_pTradeApi != NULL){
		QMutexLocker locker(&mThis->mOrderMutex);
		CUstpFtdcOrderActionField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, brokerId.toStdString().data());
		strcpy(req.UserID, userId.toStdString().data());
		strcpy(req.InvestorID, investorId.toStdString().data());
		strcpy(req.ExchangeID, "CFFEX");
		strcpy(req.UserOrderLocalID, orderRef.toStdString().data()); 
		sprintf(req.UserOrderActionLocalID, "%012d", USTPMutexId::getUserLocalId());
		req.ActionFlag = USTP_FTDC_AF_Delete;
		int nResult = mThis->m_pTradeApi->ReqOrderAction(&req, ++mThis->nRequestId);
		return nResult;
	}
	return -1;
}

int USTPTradeApi::reqQryInstrument( const QString& instrumentId)
{	
	if (mThis->m_pTradeApi != NULL){
		CUstpFtdcQryInstrumentField req;
		memset(&req, 0,sizeof(req));
		strcpy(req.InstrumentID, "");
		int nResult = mThis->m_pTradeApi->ReqQryInstrument(&req, ++mThis->nRequestId);
		return nResult;
	}
	return -1;
}

int USTPTradeApi::reqQryInvestorPosition(const QString& brokerId, const QString& investorId, const QString& instrumentId)
{	
	if (mThis->m_pTradeApi != NULL){
		CUstpFtdcQryInvestorPositionField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, brokerId.toStdString().data());
		strcpy(req.InvestorID, investorId.toStdString().data());
		strcpy(req.InstrumentID, instrumentId.toStdString().data());	
		int iRet = mThis->m_pTradeApi->ReqQryInvestorPosition(&req, ++mThis->nRequestId);
	}
	return -1;
}

#include "moc_USTPTradeApi.cpp"