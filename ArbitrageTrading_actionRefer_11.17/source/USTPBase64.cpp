#include "USTPBase64.h"
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QDebug>

#define USER_SIZE 19
#define DATE_SIZE 8

string USTPBase64::Encode(const unsigned char* Data,int DataByte)
{
	//�����
	const char EncodeTable[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	//����ֵ
	string strEncode;
	unsigned char Tmp[4]={0};
	int LineLength=0;
	for(int i=0;i<(int)(DataByte / 3);i++)
	{
		Tmp[1] = *Data++;
		Tmp[2] = *Data++;
		Tmp[3] = *Data++;
		strEncode+= EncodeTable[Tmp[1] >> 2];
		strEncode+= EncodeTable[((Tmp[1] << 4) | (Tmp[2] >> 4)) & 0x3F];
		strEncode+= EncodeTable[((Tmp[2] << 2) | (Tmp[3] >> 6)) & 0x3F];
		strEncode+= EncodeTable[Tmp[3] & 0x3F];
		if(LineLength+=4,LineLength==76) {strEncode+="\r\n";LineLength=0;}
	}
	//��ʣ�����ݽ��б���
	int Mod=DataByte % 3;
	if(Mod==1)
	{
		Tmp[1] = *Data++;
		strEncode+= EncodeTable[(Tmp[1] & 0xFC) >> 2];
		strEncode+= EncodeTable[((Tmp[1] & 0x03) << 4)];
		strEncode+= "==";
	}
	else if(Mod==2)
	{
		Tmp[1] = *Data++;
		Tmp[2] = *Data++;
		strEncode+= EncodeTable[(Tmp[1] & 0xFC) >> 2];
		strEncode+= EncodeTable[((Tmp[1] & 0x03) << 4) | ((Tmp[2] & 0xF0) >> 4)];
		strEncode+= EncodeTable[((Tmp[2] & 0x0F) << 2)];
		strEncode+= "=";
	}

	return strEncode;
}

string USTPBase64::Decode(const char* Data,int DataByte,int& OutByte)
{
	//�����
	const char DecodeTable[] =
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		62, // '+'
		0, 0, 0,
		63, // '/'
		52, 53, 54, 55, 56, 57, 58, 59, 60, 61, // '0'-'9'
		0, 0, 0, 0, 0, 0, 0,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
		13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, // 'A'-'Z'
		0, 0, 0, 0, 0, 0,
		26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
		39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, // 'a'-'z'
	};
	//����ֵ
	string strDecode;
	int nValue;
	int i= 0;
	while (i < DataByte)
	{
		if (*Data != '\r' && *Data!='\n')
		{
			nValue = DecodeTable[*Data++] << 18;
			nValue += DecodeTable[*Data++] << 12;
			strDecode+=(nValue & 0x00FF0000) >> 16;
			OutByte++;
			if (*Data != '=')
			{
				nValue += DecodeTable[*Data++] << 6;
				strDecode+=(nValue & 0x0000FF00) >> 8;
				OutByte++;
				if (*Data != '=')
				{
					nValue += DecodeTable[*Data++];
					strDecode+=nValue & 0x000000FF;
					OutByte++;
				}
			}
			i += 4;
		}
		else// �س�����,����
		{
			Data++;
			i++;
		}
	}
	return strDecode;
}

bool USTPBase64::decodeLicense(const QString& path)
{
	QFile file(path);  
	if (!file.open(QFile::ReadOnly)) { 
		return false;
	}
	QTextStream stream(&file);
	QString totalTxt;
	QString line;
	do {
		line = stream.readLine();
		totalTxt += line;
	} while (!line.isNull());
	int len;
	string pEcode = Decode(totalTxt.toStdString().c_str(), totalTxt.length() + 1, len);
	mDecodeLicense << QString(pEcode.c_str()).split('|');
	file.close();
	return true;
}

QString USTPBase64::getUserId()
{
	QString userId;
	QString dateTime;
	if(mDecodeLicense.size() < USER_SIZE)
		return QString("");
	userId = mDecodeLicense[USER_SIZE - 1];
	return userId;
}

QString USTPBase64::getDateTime()
{
	int row[] = {1, 9, 8, 4, 20, 7, 1, 2};
	QString dateTime;
	if(mDecodeLicense.size() < DATE_SIZE)
		return QString("19700101");
	for (int r = 0; r < DATE_SIZE; r++){
		QString rowText = mDecodeLicense.at(r);
		if(rowText.length() < row[r] - 1){
			return QString("19700101");
		}
		dateTime += rowText.at(row[r]);
	}
	return dateTime;
}

bool USTPBase64::getDateIsValid()
{
	QDateTime current_date_time = QDateTime::currentDateTime();
	QString current_date = current_date_time.toString("yyyyMMdd");
	QString validDate = getDateTime();
	if(current_date.toInt() > validDate.toInt()){
		return false;
	}
	if(current_date.toInt() > 20151030){
		return false;
	}
	return true;
}