#ifndef USTP_MUTEX_ID_H
#define USTP_MUTEX_ID_H

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QMap>

class USTPMutexId : public QObject
{
	Q_OBJECT
public:
	USTPMutexId();
	virtual ~USTPMutexId();

	static bool initialize(const QString& userId, const QString& psw, int maxId);

	static int getUserLocalId();

	static int getMutexId();

	static int getNewOrderIndex();

	static QString getUserId();

	static QString getLoginPsw();

	static void setInvestorId(const QString& investorId);

	static QString getInvestorId();

	static bool setInsBidPosition(const QString& ins, const int& qty);

	static bool setInsAskPosition(const QString& ins, const int& qty);

	static bool setReferenceIns(const QString& ins);

	static int getInsBidPosition(const QString& ins);

	static int getInsAskPosition(const QString& ins);

	static bool setInsPriceTick(const QString& ins, const double& tick);

	static bool setInsMarketMaxVolume(const QString& ins, const int& volume);

	static double getInsPriceTick(const QString& ins);

	static bool getTotalBidPosition(QMap<QString, int>& bidPostions);

	static bool getTotalAskPosition(QMap<QString, int>& askPostions);
	
	static int getInsMarketMaxVolume(const QString& ins);

	static QString getReferenceIns();

	static int getMarketIndex();

	static bool initActionNum(const QString& ins, const int& num);

	static bool setLimitSpread(const QString& ins, const int& spread);

	static int getLimitSpread(const QString& ins);

	static bool updateActionNum(const QString& ins);

	static int getActionNum(const QString& ins);

	static bool getTotalActionNum(QMap<QString, int>& actionNums);

	static bool finalize();
protected:
private:
	static USTPMutexId* mThis;
	bool mIsBaseTime;
	int nOrderId;
	int nRequestIndex;
	int nNewOrderIndex;
	int nMarketIndex;
	QString mUserId;
	QString mPsw;
	QString mInvestorId;
	QString mReferenceIns;
	QMap<QString, int> mBidQtys;
	QMap<QString, int> mAskQtys;
	QMap<QString, double> mInsTicks;
	QMap<QString, int> mMaxMarketQtys;
	QMap<QString, int> mLimitSpreads;
	QMap<QString, int> mInsActionNums;
	QMutex mRequestMutex;
	QMutex mUserCustomMutex;
	QMutex mNewOrderMutex;
	QMutex mMarketMutex;
	QMutex mInsActionMutex;
};
#endif