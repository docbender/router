#define VERSION				"1.17"
#define DEFAULT_SOCKET_PORT			55555
#define DEFAULT_ADMIN_PORT			55554
#define DEFAULT_MAX_MODULE_COUNT	1024
#define DEFAULT_MODULE_WARN			10000
#define DEFAULT_MODULE_END			5000
#define DEFAULT_MODULE_DISCARD		300000
#define DEFAULT_ADMIN_WARN			60000
#define DEFAULT_ADMIN_END			10000
#define DEFAULT_ACTIVITY			1
//#define CHECK_TIME			5000

#define MSG_BEGIN			"\xDE\xAD"
#define MSG_END				"\xBE\xEF"
#define MSG_HEAD_SIZE		7
#define MSG_FOOT_SIZE		2
#define MSG_LENGTH_POS		2
#define MSG_TYPE_POS		6

#define MSGTYPE_INIT		0
#define MSGTYPE_MAPPING		255
#define MSGTYPE_POINTS		1
#define MSGTYPE_GETPOINTS	3
#define MSGTYPE_ASK			252
#define MSGTYPE_ANSWER		6

#define POINTTYPE_NONE		0
#define POINTTYPE_BOOL		1
#define POINTTYPE_INT8		2
#define POINTTYPE_UINT8		3
#define POINTTYPE_INT16		4
#define POINTTYPE_UINT16	5
#define POINTTYPE_INT32		6
#define POINTTYPE_UINT32	7
#define POINTTYPE_INT64		8
#define POINTTYPE_UINT64	9
#define POINTTYPE_FLOAT		10
#define POINTTYPE_DOUBLE	11
#define POINTTYPE_STRING	12
#define POINTTYPE_STREAM	13
#define POINTTYPE_XML		14
#define POINTTYPE_BASE64	15

#define POINTSIZE_NONE		0
#define POINTSIZE_BOOL		1
#define POINTSIZE_INT8		1
#define POINTSIZE_UINT8		1
#define POINTSIZE_INT16		2
#define POINTSIZE_UINT16	2
#define POINTSIZE_INT32		4
#define POINTSIZE_UINT32	4
#define POINTSIZE_INT64		8
#define POINTSIZE_UINT64	8
#define POINTSIZE_FLOAT		4
#define POINTSIZE_DOUBLE	8
#define POINTSIZE_STRING	0xFFFFFFFF
#define POINTSIZE_STREAM	0xFFFFFFFF
#define POINTSIZE_XML		0xFFFFFFFF
#define POINTSIZE_BASE64	0xFFFFFFFF

#define STATE_MODULE_ACTIVE 0x1
#define STATE_MODULE_CONNECTED 0x2

#include <winsock2.h>

#include <iostream>
#include <list>
#include <vector>
#include <map>
#include <comdef.h>
//#include <tchar.h>
#include <sstream>
#include <fstream>
#include <iomanip>
//#include <ctime>
//#include <windows.h>
//#include <stdio.h>
//#include "myaes.h"
#include <regex>

//#pragma comment (lib, "cppauth")


using namespace std;

struct data_module
{
	SOCKET SockHandle;
	sockaddr_in SockInfo;
	string ModuleName;
	string ReadBuffer;
	string WriteBuffer;
	unsigned int LastMessagePos;
	list<unsigned int> Producing;
	list<unsigned int> Consuming;
	unsigned long long WarnTime;
	unsigned long long EndTime;
	bool ConsumingAll;
	bool ProducingAll;
	uint64_t StartTime = 0;
	uint64_t StateChangedTime = 0;
	int State;
	int StateBeforeChange;
};

struct admin_module
{
	SOCKET SockHandle;
	sockaddr_in SockInfo;
	string ReadBuffer;
	string WriteBuffer;
	unsigned long long WarnTime;
	unsigned long long EndTime;
	string SupportedVersion;
};

typedef list<data_module>::iterator module_iter;
typedef list<admin_module>::iterator admin_iter;
typedef map<string, unsigned int>::iterator map_iter;

struct data_point
{
	string PointName;
	list<module_iter> Producer;
	list<module_iter> Consumer;
	unsigned char PointType;
	unsigned char PointQuality;
	string PointValue;
	module_iter LastChangedBy;
	uint64_t Changed = 0;
	bool Forced;
};

list<admin_module> AdminList;
list<data_module> ModuleList;
vector<data_point> PointsVector;
map<string, unsigned int> PointsMap;
map<unsigned char, unsigned int> TypeMap;
map<unsigned int, unsigned int> ForcedMap;
unsigned long long GlobalRunTime = 0;
uint64_t StartTime = 0;

ostream *msg_out = &cout;

SERVICE_STATUS          ServiceStatus;
SERVICE_STATUS_HANDLE   hStatus;

int socket_port, admin_port, max_module_count, module_warn, module_end, module_discard, admin_warn, admin_end;

#ifndef _SECURE_SCL
#define _SECURE_SCL 0
#endif

void Init()
{
	TypeMap[POINTTYPE_NONE] = POINTSIZE_NONE;
	TypeMap[POINTTYPE_BOOL] = POINTSIZE_BOOL;
	TypeMap[POINTTYPE_INT8] = POINTSIZE_INT8;
	TypeMap[POINTTYPE_UINT8] = POINTSIZE_UINT8;
	TypeMap[POINTTYPE_INT16] = POINTSIZE_INT16;
	TypeMap[POINTTYPE_UINT16] = POINTSIZE_UINT16;
	TypeMap[POINTTYPE_INT32] = POINTSIZE_INT32;
	TypeMap[POINTTYPE_UINT32] = POINTSIZE_UINT32;
	TypeMap[POINTTYPE_INT64] = POINTSIZE_INT64;
	TypeMap[POINTTYPE_UINT64] = POINTSIZE_UINT64;
	TypeMap[POINTTYPE_FLOAT] = POINTSIZE_FLOAT;
	TypeMap[POINTTYPE_DOUBLE] = POINTSIZE_DOUBLE;
	TypeMap[POINTTYPE_STRING] = POINTSIZE_STRING;
	TypeMap[POINTTYPE_STREAM] = POINTSIZE_STREAM;
	TypeMap[POINTTYPE_XML] = POINTSIZE_XML;
	TypeMap[POINTTYPE_BASE64] = POINTSIZE_BASE64;

	data_point NewPoint;
	NewPoint.PointName = "$Active";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_BOOL;
	NewPoint.PointValue.assign("\000", 1);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestINT8";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_INT8;
	NewPoint.PointValue.assign("\000", 1);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestUINT8";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_UINT8;
	NewPoint.PointValue.assign("\000", 1);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestINT16";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_INT16;
	NewPoint.PointValue.assign("\000\000", 2);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestUINT16";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_UINT16;
	NewPoint.PointValue.assign("\000\000", 2);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestINT32";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_INT32;
	NewPoint.PointValue.assign("\000\000\000\000", 4);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestUINT32";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_UINT32;
	NewPoint.PointValue.assign("\000\000\000\000", 4);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestINT64";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_INT64;
	NewPoint.PointValue.assign("\000\000\000\000\000\000\000\000", 8);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestUINT64";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_UINT64;
	NewPoint.PointValue.assign("\000\000\000\000\000\000\000\000", 8);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestFLOAT";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_FLOAT;
	NewPoint.PointValue.assign("\000\000\000\000", 4);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestDOUBLE";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_DOUBLE;
	NewPoint.PointValue.assign("\000\000\000\000\000\000\000\000", 8);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestString";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_STRING;
	NewPoint.PointValue.assign("\000\000\000\000", 4);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestStream";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_STREAM;
	NewPoint.PointValue.assign("\000\000\000\000\000\000", 6);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestXML";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_XML;
	NewPoint.PointValue.assign("\000\000\000\000", 4);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));

	NewPoint.PointName = "$TestBase64";
	NewPoint.PointQuality = 255;
	NewPoint.PointType = POINTTYPE_BASE64;
	NewPoint.PointValue.assign("\000\000\000\000", 4);
	NewPoint.Forced = false;
	NewPoint.LastChangedBy = ModuleList.end();
	PointsVector.push_back(NewPoint);
	PointsMap.insert(pair<string, int>(NewPoint.PointName, PointsVector.size() - 1));
}

uint64_t getNow()
{
	FILETIME now;
	GetSystemTimeAsFileTime(&now);
	uint64_t ll_now = (LONGLONG)now.dwLowDateTime + ((LONGLONG)(now.dwHighDateTime) << 32LL);

	return ll_now - 116444736000000000LL;
}

bool supportAdminThisVersion(admin_iter &admin, string version)
{
	if ((*admin).SupportedVersion.length() == 0)
		return false;

	int avMajor = 0, avMinor = 0, tvMajor = 0, tvMinor = 0;

	size_t pos = (*admin).SupportedVersion.find_first_of('.');
	if (pos == -1)
	{
		try
		{
			avMajor = std::stoi((*admin).SupportedVersion);
		}
		catch (exception e)
		{
			return false;
		}
	}
	else if (pos < (*admin).SupportedVersion.length() - 1)
	{
		try
		{
			avMajor = std::stoi((*admin).SupportedVersion);
			avMinor = std::stoi((*admin).SupportedVersion.substr(pos + 1));
		}
		catch (exception e)
		{
			return false;
		}
	}
	else
		return false;

	pos = version.find_first_of('.');
	if (pos == -1)
	{
		try
		{
			tvMajor = std::stoi(version);
		}
		catch (exception e)
		{
			return false;
		}
	}
	else if (pos < version.length() - 1)
	{
		try
		{
			tvMajor = std::stoi(version);
			tvMinor = std::stoi(version.substr(pos + 1));
		}
		catch (exception e)
		{
			return false;
		}
	}
	else
		return false;


	if (avMajor > tvMajor)
		return true;
	else if (avMajor == tvMajor && avMinor >= tvMinor)
		return true;
	else
		return false;
}


string ReportHead()
{
	/*string result;
	char dateStr [9];
	char timeStr [9];
	_strdate_s(dateStr);
	_strtime_s(timeStr);
	result.append(dateStr);
	result.append(" ");
	result.append(timeStr);*/

	SYSTEMTIME st;
	char head[30];
	GetSystemTime(&st);
	sprintf_s(head, "[%02d.%02d.%04d %02d:%02d:%02d.%03d]: ", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	return head;
}

void ReportError(int ErrNr, char* ErrTxt)
{
	char ErrRes[256];
	WideCharToMultiByte(CP_ACP, NULL, _com_error(ErrNr).ErrorMessage(), -1, ErrRes, 255, NULL, NULL);
	*msg_out << ReportHead() << "(" << ErrTxt << "): " << ErrRes << endl;
}

void FinishNewMessage(module_iter WorkModuleIter)
{
	if (WorkModuleIter->LastMessagePos == 0xFFFFFFFF)
	{
		return;
	}
	else if ((WorkModuleIter->LastMessagePos + MSG_HEAD_SIZE) > WorkModuleIter->WriteBuffer.size())
	{
		*msg_out << ReportHead() << "Error finishing message for module '" << (WorkModuleIter->ModuleName) << "'!" << endl;
		WorkModuleIter->LastMessagePos = 0xFFFFFFFF;
	}
	else
	{
		unsigned int MsgSize = WorkModuleIter->WriteBuffer.size() - WorkModuleIter->LastMessagePos - MSG_HEAD_SIZE;
		WorkModuleIter->WriteBuffer.replace(WorkModuleIter->LastMessagePos + MSG_LENGTH_POS, 4, (char *)&MsgSize, 4);
		WorkModuleIter->WriteBuffer += MSG_END;
		WorkModuleIter->LastMessagePos = 0xFFFFFFFF;
	}
}

void PrepareNewMessage(module_iter WorkModuleIter, unsigned char MsgType)
{
	if (WorkModuleIter->LastMessagePos < 0xFFFFFFFF)
	{
		if (unsigned char(WorkModuleIter->WriteBuffer[WorkModuleIter->LastMessagePos + MSG_TYPE_POS]) == MsgType)
		{
			return;
		}
		else
		{
			FinishNewMessage(WorkModuleIter);
		}
	}

	/*	*msg_out << ReportHead() << "Preparing ";
		switch (MsgType)
		{
			case MSGTYPE_INIT: *msg_out << "INIT"; break;
			case MSGTYPE_MAPPING: *msg_out << "MAPPING"; break;
			case MSGTYPE_POINTS: *msg_out << "POINTS"; break;
			case MSGTYPE_GETPOINTS: *msg_out << "GET POINTS"; break;
			case MSGTYPE_ASK: *msg_out << "ASK"; break;
			case MSGTYPE_ANSWER: *msg_out << "ANSWER"; break;
		}
		*msg_out << " message for module '" << WorkModuleIter->ModuleName << "'." << endl;*/

	WorkModuleIter->LastMessagePos = WorkModuleIter->WriteBuffer.size();
	WorkModuleIter->WriteBuffer += MSG_BEGIN;
	WorkModuleIter->WriteBuffer.append("\x00\x00\x00\x00", 4);
	WorkModuleIter->WriteBuffer += char(MsgType);
}

unsigned int AddConsumerProducer(module_iter WorkModuleIter, int PointIndex, bool IsConsumer)
{
	list<module_iter>::iterator TempModuleIter;
	module_iter ModuleListIter;
	bool IsAlreadyProduced, IsAlreadyConsumed;

	for (TempModuleIter = PointsVector[PointIndex].Consumer.begin(); TempModuleIter != PointsVector[PointIndex].Consumer.end(); ++TempModuleIter)
	{
		if (*TempModuleIter == WorkModuleIter) break;
	}
	IsAlreadyConsumed = (TempModuleIter != PointsVector[PointIndex].Consumer.end());

	for (TempModuleIter = PointsVector[PointIndex].Producer.begin(); TempModuleIter != PointsVector[PointIndex].Producer.end(); ++TempModuleIter)
	{
		if (*TempModuleIter == WorkModuleIter) break;
	}
	IsAlreadyProduced = (TempModuleIter != PointsVector[PointIndex].Producer.end());

	if (IsConsumer)
	{
		if (IsAlreadyConsumed)
		{
			//*msg_out << ReportHead() << "Module '" << WorkModuleIter->ModuleName << "' wants to be consumer of point '" << PointName << "', which is already consumed by this module." << endl;
			PointIndex = 0xFFFFFFFF;
		}
		else
		{
			PointsVector[PointIndex].Consumer.push_back(WorkModuleIter);
			WorkModuleIter->Consuming.push_back(PointIndex);
			if (IsAlreadyProduced)
			{
				//*msg_out << ReportHead() << "Module '" << WorkModuleIter->ModuleName << "' is producer and now also consumer of point '" << PointName << "'." << endl;
				PointIndex = 0xFFFFFFFF;
			}
		}
	}
	else
	{
		if (IsAlreadyProduced)
		{
			//*msg_out << ReportHead() << "Module '" << WorkModuleIter->ModuleName << "' wants to be producer of point '" << PointName << "', which is already produced by this module." << endl; 
			PointIndex = 0xFFFFFFFF;
		}
		else
		{
			PointsVector[PointIndex].Producer.push_back(WorkModuleIter);
			WorkModuleIter->Producing.push_back(PointIndex);
			if (IsAlreadyConsumed)
			{
				//*msg_out << ReportHead() << "Module '" << WorkModuleIter->ModuleName << "' is consumer and now also producer of point '" << PointName << "'." << endl;
				PointIndex = 0xFFFFFFFF;
			}
			if ((PointsVector[PointIndex].LastChangedBy != ModuleList.end()) && (PointsVector[PointIndex].LastChangedBy->SockHandle == INVALID_SOCKET) && (PointsVector[PointIndex].LastChangedBy->ModuleName == WorkModuleIter->ModuleName))
			{
				PointsVector[PointIndex].LastChangedBy = WorkModuleIter;
			}
		}
	}
	return PointIndex;
}

unsigned int AddConsumerProducer(module_iter WorkModuleIter, string PointName, bool IsConsumer)
{
	list<module_iter>::iterator TempModuleIter;
	module_iter ModuleListIter;
	map<string, unsigned int>::iterator PointsMapIterator = PointsMap.find(PointName);
	unsigned int PointIndex;
	unsigned int TempSize;

	if (PointsMapIterator == PointsMap.end())
	{
		data_point NewPoint;
		NewPoint.PointName = PointName;
		NewPoint.PointQuality = 0;
		NewPoint.PointType = POINTTYPE_NONE;
		NewPoint.Forced = false;
		NewPoint.LastChangedBy = ModuleList.end();
		PointsVector.push_back(NewPoint);
		PointIndex = PointsVector.size() - 1;
		PointsMapIterator = PointsMap.insert(pair<string, int>(PointName, PointIndex)).first;
		for (ModuleListIter = ModuleList.begin(); ModuleListIter != ModuleList.end(); ++ModuleListIter)
			if ((ModuleListIter != WorkModuleIter) && ((ModuleListIter->ConsumingAll) || (ModuleListIter->ProducingAll)))
			{
				PrepareNewMessage(ModuleListIter, MSGTYPE_MAPPING);
				TempSize = PointName.size();
				ModuleListIter->WriteBuffer.append((char *)&TempSize, 1);
				ModuleListIter->WriteBuffer += PointName;
				ModuleListIter->WriteBuffer.append((char *)&PointIndex, 4);
				if (ModuleListIter->ConsumingAll)
				{
					PointsVector[PointIndex].Consumer.push_back(ModuleListIter);
					ModuleListIter->Consuming.push_back(PointIndex);
				}
				if (ModuleListIter->ProducingAll)
				{
					PointsVector[PointIndex].Producer.push_back(ModuleListIter);
					ModuleListIter->Producing.push_back(PointIndex);
				}
			}
	}
	else
	{
		PointIndex = PointsMapIterator->second;
	}
	return AddConsumerProducer(WorkModuleIter, PointIndex, IsConsumer);
}

void SendPoint(module_iter WorkModuleIter, unsigned int PointIndex, bool ForcedSend)
{
	if ((PointsVector[0].PointValue[0] == 0) && (PointIndex != 0) && (ForcedSend == false))
	{
		// jsme pasivni, nedelame nic
	}
	else if (PointIndex < PointsVector.size())
	{
		PrepareNewMessage(WorkModuleIter, MSGTYPE_POINTS);
		WorkModuleIter->WriteBuffer.append((char *)&PointIndex, 4);
		WorkModuleIter->WriteBuffer += char(PointsVector[PointIndex].PointQuality);
		WorkModuleIter->WriteBuffer += char(PointsVector[PointIndex].PointType);
		if (TypeMap[PointsVector[PointIndex].PointType] == 0xFFFFFFFF)
		{
			unsigned int TempLong = PointsVector[PointIndex].PointValue.size();
			WorkModuleIter->WriteBuffer.append((char *)&TempLong, 4);
		}

		WorkModuleIter->WriteBuffer += PointsVector[PointIndex].PointValue;
	}
	else
	{
		*msg_out << ReportHead() << "Point index '" << PointIndex << "' sent by module '" << WorkModuleIter->ModuleName << "' is out of range, no send possible." << endl;
	}
}

void UpdatePointValue(unsigned int PointIndex, unsigned char NewPointType, unsigned char NewPointQuality, string NewPointValue, module_iter WorkModuleIter)
{
	if (PointIndex < PointsVector.size())
	{
		uint64_t now = getNow();
		PointsVector[PointIndex].Changed = now;

		if (PointIndex == 0)
		{
			module_iter TempModuleIter;

			if (NewPointValue[0] != PointsVector[0].PointValue[0])
			{
				for (TempModuleIter = ModuleList.begin(); TempModuleIter != ModuleList.end(); ++TempModuleIter)
				{
					TempModuleIter->StateBeforeChange = TempModuleIter->State;
					if (NewPointValue[0] == 0)
						TempModuleIter->State &= ~STATE_MODULE_ACTIVE;
					else
						TempModuleIter->State |= STATE_MODULE_ACTIVE;
					TempModuleIter->StateChangedTime = now;
				}
			}
		}

		if (WorkModuleIter != ModuleList.end())
		{
			PointsVector[PointIndex].LastChangedBy = WorkModuleIter;
		}
		if ((PointIndex == 0) && (NewPointValue[0] != 0) && (PointsVector[0].PointValue[0] == 0))
		{
			list<unsigned int>::iterator PointIter;
			module_iter TempModuleIter;
			PointsVector[0].PointType = NewPointType;
			PointsVector[0].PointQuality = NewPointQuality;
			PointsVector[0].PointValue = NewPointValue;

			for (TempModuleIter = ModuleList.begin(); TempModuleIter != ModuleList.end(); ++TempModuleIter)
			{
				for (PointIter = TempModuleIter->Consuming.begin(); PointIter != TempModuleIter->Consuming.end(); ++PointIter)
				{
					SendPoint(TempModuleIter, *PointIter, false);
				}
			}
		}
		else if ((NewPointType != PointsVector[PointIndex].PointType) ||
			(NewPointQuality != PointsVector[PointIndex].PointQuality) ||
			(NewPointValue != PointsVector[PointIndex].PointValue))
		{
			list<module_iter>::iterator TempModuleIter;
			PointsVector[PointIndex].PointType = NewPointType;
			PointsVector[PointIndex].PointQuality = NewPointQuality;
			PointsVector[PointIndex].PointValue = NewPointValue;
			for (TempModuleIter = PointsVector[PointIndex].Consumer.begin(); TempModuleIter != PointsVector[PointIndex].Consumer.end(); ++TempModuleIter)
			{
				if (*TempModuleIter != WorkModuleIter)
					SendPoint(*TempModuleIter, PointIndex, false);
			}
		}
	}
	else
	{
		*msg_out << ReportHead() << "Point index '" << PointIndex << "' sent by module '" << WorkModuleIter->ModuleName << "' is out of range, no update possible." << endl;
	}
}

void RemoveModule(module_iter WorkModuleIter)
{
	list<unsigned int>::iterator PointIter;
	closesocket(WorkModuleIter->SockHandle);
	WorkModuleIter->SockHandle = INVALID_SOCKET;
	WorkModuleIter->WarnTime = 0xFFFFFFFFFFFFFFFF;
	WorkModuleIter->EndTime = GlobalRunTime + module_discard;

	WorkModuleIter->StateBeforeChange = WorkModuleIter->State;
	WorkModuleIter->State &= ~STATE_MODULE_CONNECTED;
	WorkModuleIter->StateChangedTime = getNow();

	for (PointIter = WorkModuleIter->Consuming.begin(); PointIter != WorkModuleIter->Consuming.end(); ++PointIter)
	{
		PointsVector[*PointIter].Consumer.remove(WorkModuleIter);
	}
}

void DiscardModule(module_iter* WorkModuleIter)
{
	list<unsigned int>::iterator PointIter;

	for (PointIter = (*WorkModuleIter)->Producing.begin(); PointIter != (*WorkModuleIter)->Producing.end(); ++PointIter)
	{
		PointsVector[*PointIter].Producer.remove(*WorkModuleIter);
		if (PointsVector[*PointIter].LastChangedBy == (*WorkModuleIter))
		{
			PointsVector[*PointIter].LastChangedBy = ModuleList.end();
			if ((*WorkModuleIter)->ModuleName[0] != '!')
			{
				UpdatePointValue(*PointIter, PointsVector[*PointIter].PointType, 0, PointsVector[*PointIter].PointValue, ModuleList.end());
			}
			//			*msg_out << ReportHead() << "Quality of point '" << PointsVector[*PointIter].PointName << "' dropped to 0 due to disconnected module." << endl;
		}
	}

	*WorkModuleIter = ModuleList.erase(*WorkModuleIter);
}

void RemoveAdminModule(admin_iter* WorkAdminIter)
{
	closesocket((*WorkAdminIter)->SockHandle);
	*WorkAdminIter = AdminList.erase(*WorkAdminIter);

	if (AdminList.empty())
	{
		map<unsigned int, unsigned int>::iterator ForcedMapIter;
		for (ForcedMapIter = ForcedMap.begin(); ForcedMapIter != ForcedMap.end(); ++ForcedMapIter)
		{
			PointsVector[ForcedMapIter->first].Forced = false;
		}
		ForcedMap.clear();
	}
}

bool ProcessGetPointMessage(module_iter WorkModuleIter, unsigned int MsgSize)
{
	unsigned int WantedIndex, TempPos;
	TempPos = MSG_HEAD_SIZE;

	while (TempPos < (MsgSize + MSG_HEAD_SIZE))
	{
		if ((TempPos + 4) > (MsgSize + MSG_HEAD_SIZE)) return 0;
		memcpy(&WantedIndex, WorkModuleIter->ReadBuffer.data() + TempPos, 4);
		TempPos += 4;

		if (WantedIndex == 0xFFFFFFFF)
		{
			list<unsigned int>::iterator PointIter;
			for (PointIter = WorkModuleIter->Producing.begin(); PointIter != WorkModuleIter->Producing.end(); ++PointIter)
			{
				SendPoint(WorkModuleIter, *PointIter, true);
			}
		}
		else
		{
			SendPoint(WorkModuleIter, WantedIndex, true);
		}
	}
	return 1;
}

bool ProcessPointMessage(module_iter WorkModuleIter, unsigned int MsgSize)
{
	map<unsigned char, unsigned int>::iterator TypeMapIterator;
	unsigned char SentPointQuality, SentPointType;
	unsigned int SentPointIndex, TempPos, SentPointSize;
	string SentPointValue;
	TempPos = MSG_HEAD_SIZE;

	while (TempPos < (MsgSize + MSG_HEAD_SIZE))
	{
		if ((TempPos + 4) > (MsgSize + MSG_HEAD_SIZE)) return 0;
		memcpy(&SentPointIndex, WorkModuleIter->ReadBuffer.data() + TempPos, 4);
		TempPos += 4;

		if ((TempPos + 1) > (MsgSize + MSG_HEAD_SIZE)) return 0;
		memcpy(&SentPointQuality, WorkModuleIter->ReadBuffer.data() + TempPos, 1);
		++TempPos;

		if ((TempPos + 1) > (MsgSize + MSG_HEAD_SIZE)) return 0;
		memcpy(&SentPointType, WorkModuleIter->ReadBuffer.data() + TempPos, 1);
		++TempPos;

		TypeMapIterator = TypeMap.find(SentPointType);
		if (TypeMapIterator == TypeMap.end())
		{
			*msg_out << ReportHead() << "Unknown point type: " << int(SentPointType) << endl;
			return 0;
		}
		else if (TypeMapIterator->second == 0xFFFFFFFF)
		{
			if ((TempPos + 4) > (MsgSize + MSG_HEAD_SIZE)) return 0;
			memcpy(&SentPointSize, WorkModuleIter->ReadBuffer.data() + TempPos, 4);
			TempPos += 4;
		}
		else
		{
			SentPointSize = TypeMapIterator->second;
		}

		if ((TempPos + SentPointSize) > (MsgSize + MSG_HEAD_SIZE)) return 0;
		SentPointValue = WorkModuleIter->ReadBuffer.substr(TempPos, SentPointSize);
		TempPos += SentPointSize;

		if (SentPointIndex > PointsVector.size())
		{
			*msg_out << ReportHead() << "Point index " << SentPointIndex << "' sent by module '" << WorkModuleIter->ModuleName << " is out of range, no update possible." << endl;
		}
		else if (PointsVector[SentPointIndex].Forced)
		{
			*msg_out << ReportHead() << "Point with index " << SentPointIndex << "' sent by module '" << WorkModuleIter->ModuleName << " is forced, no update possible." << endl;
		}
		else
		{
			list<module_iter>::iterator TempModuleIter;
			for (TempModuleIter = PointsVector[SentPointIndex].Producer.begin(); TempModuleIter != PointsVector[SentPointIndex].Producer.end(); ++TempModuleIter)
			{
				if (*TempModuleIter == WorkModuleIter) break;
			}
			if (TempModuleIter == PointsVector[SentPointIndex].Producer.end())
			{
				*msg_out << ReportHead() << "Point with index " << SentPointIndex << " is tried changed by module '" << WorkModuleIter->ModuleName << "', which is not producer of this point." << endl;
			}
			else
			{
				UpdatePointValue(SentPointIndex, SentPointType, SentPointQuality, SentPointValue, WorkModuleIter);
			}
		}
	}
	return 1;
}

bool ProcessInitMessage(module_iter WorkModuleIter, unsigned int MsgSize)
{
	unsigned char TempChar;
	unsigned int TempLong, TempPos;
	string TempString;
	unsigned int TempIndex;

	TempPos = MSG_HEAD_SIZE;

	if ((TempPos + 1) > (MsgSize + MSG_HEAD_SIZE)) return 0;
	memcpy(&TempChar, WorkModuleIter->ReadBuffer.data() + TempPos, 1);
	++TempPos;

	if ((TempPos + TempChar) > (MsgSize + MSG_HEAD_SIZE)) return 0;
	WorkModuleIter->ModuleName = WorkModuleIter->ReadBuffer.substr(TempPos, TempChar);
	TempPos += TempChar;

	if ((TempPos + 4) > (MsgSize + MSG_HEAD_SIZE)) return 0;
	memcpy(&TempLong, WorkModuleIter->ReadBuffer.data() + TempPos, 4);
	TempPos += 4;

	if (TempLong == 0xFFFFFFFF)
	{
		if (!WorkModuleIter->ProducingAll)
		{
			unsigned int TempSize;
			WorkModuleIter->ProducingAll = true;
			PrepareNewMessage(WorkModuleIter, MSGTYPE_MAPPING);
			for (TempIndex = 0; TempIndex < PointsVector.size(); TempIndex++)
			{
				if (AddConsumerProducer(WorkModuleIter, TempIndex, 0) != 0xFFFFFFFF)
				{
					TempSize = PointsVector[TempIndex].PointName.size();
					WorkModuleIter->WriteBuffer.append((char *)&TempSize, 1);
					WorkModuleIter->WriteBuffer += PointsVector[TempIndex].PointName;
					WorkModuleIter->WriteBuffer.append((char *)&TempIndex, 4);
				}
			}
		}
	}
	else
		while (TempLong-- > 0)
		{
			if ((TempPos + 1) > (MsgSize + MSG_HEAD_SIZE)) return 0;
			memcpy(&TempChar, WorkModuleIter->ReadBuffer.data() + TempPos, 1);
			++TempPos;

			if (TempChar > 0)
			{
				if ((TempPos + TempChar) > (MsgSize + MSG_HEAD_SIZE)) return 0;
				TempIndex = AddConsumerProducer(WorkModuleIter, WorkModuleIter->ReadBuffer.substr(TempPos, TempChar), 0);
				if (TempIndex != 0xFFFFFFFF)
				{
					PrepareNewMessage(WorkModuleIter, MSGTYPE_MAPPING);
					WorkModuleIter->WriteBuffer += char(TempChar);
					WorkModuleIter->WriteBuffer += WorkModuleIter->ReadBuffer.substr(TempPos, TempChar);
					WorkModuleIter->WriteBuffer.append((char *)&TempIndex, 4);
				}
				TempPos += TempChar;
			}
		}
	if ((TempPos + 4) > (MsgSize + MSG_HEAD_SIZE)) return 0;
	memcpy(&TempLong, WorkModuleIter->ReadBuffer.data() + TempPos, 4);
	TempPos += 4;

	if (TempLong == 0xFFFFFFFF)
	{
		if (!WorkModuleIter->ConsumingAll)
		{
			unsigned int TempSize;
			WorkModuleIter->ConsumingAll = true;
			PrepareNewMessage(WorkModuleIter, MSGTYPE_MAPPING);
			for (TempIndex = 0; TempIndex < PointsVector.size(); TempIndex++)
			{
				if (AddConsumerProducer(WorkModuleIter, TempIndex, 1) != 0xFFFFFFFF)
				{
					TempSize = PointsVector[TempIndex].PointName.size();
					WorkModuleIter->WriteBuffer.append((char *)&TempSize, 1);
					WorkModuleIter->WriteBuffer += PointsVector[TempIndex].PointName;
					WorkModuleIter->WriteBuffer.append((char *)&TempIndex, 4);
				}
			}
		}
	}
	else
		while (TempLong-- > 0)
		{
			if ((TempPos + 1) > (MsgSize + MSG_HEAD_SIZE)) return 0;
			memcpy(&TempChar, WorkModuleIter->ReadBuffer.data() + TempPos, 1);
			++TempPos;

			if (TempChar > 0)
			{
				if ((TempPos + TempChar) > (MsgSize + MSG_HEAD_SIZE)) return 0;
				TempIndex = AddConsumerProducer(WorkModuleIter, WorkModuleIter->ReadBuffer.substr(TempPos, TempChar), 1);
				if (TempIndex != 0xFFFFFFFF)
				{
					PrepareNewMessage(WorkModuleIter, MSGTYPE_MAPPING);
					WorkModuleIter->WriteBuffer += char(TempChar);
					WorkModuleIter->WriteBuffer += WorkModuleIter->ReadBuffer.substr(TempPos, TempChar);
					WorkModuleIter->WriteBuffer.append((char *)&TempIndex, 4);
				}
				TempPos += TempChar;
			}
		}
	return 1;
}

void ProcessMessage(module_iter WorkModuleIter)
{
	unsigned int TempPos;
	int Done = 0;
	unsigned int MsgSize;
	unsigned char MsgType;
	list<unsigned int>::iterator TempIntIter;

	while (((WorkModuleIter->ReadBuffer.size()) >= (MSG_HEAD_SIZE + MSG_FOOT_SIZE)) && (!Done))
	{
		TempPos = WorkModuleIter->ReadBuffer.find(MSG_BEGIN);
		if (TempPos == string::npos)
		{
			Done = 1;
		}
		else
		{
			WorkModuleIter->ReadBuffer.erase(0, TempPos);
			memcpy(&MsgSize, WorkModuleIter->ReadBuffer.data() + MSG_LENGTH_POS, 4);
			if ((WorkModuleIter->ReadBuffer.size()) < (MsgSize + MSG_HEAD_SIZE + MSG_FOOT_SIZE))
			{
				Done = 1;
			}
			else
			{
				if (WorkModuleIter->ReadBuffer.substr(MSG_HEAD_SIZE + MsgSize + MSG_FOOT_SIZE - strlen(MSG_END), strlen(MSG_END)) != MSG_END)
				{
					WorkModuleIter->ReadBuffer.erase(0, strlen(MSG_BEGIN));
					*msg_out << ReportHead() << "Bad message received from module '" << WorkModuleIter->ModuleName << "'." << endl;
				}
				else
				{
					memcpy(&MsgType, WorkModuleIter->ReadBuffer.data() + MSG_TYPE_POS, 1);
					switch (MsgType)
					{
					case MSGTYPE_INIT:
						if (!ProcessInitMessage(WorkModuleIter, MsgSize))
						{
							*msg_out << ReportHead() << "INIT message from module '" << WorkModuleIter->ModuleName << "' is not consistent." << endl;
						}
						else
						{
							*msg_out << ReportHead() << "Received INIT message from module '" << WorkModuleIter->ModuleName << "'." << endl;
						}
						if (!WorkModuleIter->ConsumingAll)
							for (TempIntIter = WorkModuleIter->Consuming.begin(); TempIntIter != WorkModuleIter->Consuming.end(); ++TempIntIter)
							{
								SendPoint(WorkModuleIter, *TempIntIter, false);
							}
						break;
					case MSGTYPE_POINTS:
						//*msg_out << ReportHead() << "Received POINTS message from module '" << WorkModuleIter->ModuleName << "'." << endl;
						if (!ProcessPointMessage(WorkModuleIter, MsgSize))
						{
							*msg_out << ReportHead() << "POINTS MESSAGE from module '" << WorkModuleIter->ModuleName << "' is not consistent." << endl;
						}
						break;
					case MSGTYPE_GETPOINTS:
						//*msg_out << ReportHead() << "Received GET POINTS message from module '" << WorkModuleIter->ModuleName << "'." << endl;
						if (!ProcessGetPointMessage(WorkModuleIter, MsgSize))
						{
							*msg_out << ReportHead() << "GET POINTS message from module '" << WorkModuleIter->ModuleName << "' is not consistent." << endl;
						}
						break;
					case MSGTYPE_ANSWER:
						//*msg_out << ReportHead() << "Received ANSWER message from module '" << WorkModuleIter->ModuleName << "'." << endl;
						break;
					default:
						*msg_out << ReportHead() << "Unknown message type " << int(MsgType) << " from module '" << WorkModuleIter->ModuleName << "'." << endl;
					}
					WorkModuleIter->ReadBuffer.erase(0, MSG_HEAD_SIZE + MsgSize + MSG_FOOT_SIZE);
				}
			}
		}
	}
}

void ProcessWrite(module_iter WorkModuleIter)
{
	FinishNewMessage(WorkModuleIter);
	int nwrite = send(WorkModuleIter->SockHandle, WorkModuleIter->WriteBuffer.data(), int(WorkModuleIter->WriteBuffer.size()), 0);
	if (nwrite < 0)
	{
		ReportError(WSAGetLastError(), "send");
	}
	else
	{
		WorkModuleIter->WriteBuffer.erase(0, (unsigned int)nwrite);
	}
}

void ProcessAdminWrite(admin_iter WorkAdminIter)
{
	int nwrite = send(WorkAdminIter->SockHandle, WorkAdminIter->WriteBuffer.c_str(), int(WorkAdminIter->WriteBuffer.size()), 0);
	if (nwrite < 0)
	{
		ReportError(WSAGetLastError(), "send");
	}
	else
	{
		WorkAdminIter->WriteBuffer.erase(0, (unsigned int)nwrite);
	}
}

void ProcessRead(module_iter* WorkModuleIter)
{
	int nread;
	u_long status;
	ioctlsocket((*WorkModuleIter)->SockHandle, FIONREAD, &status);
	if (status == 0)
	{
		*msg_out << ReportHead() << "Module '" << (*WorkModuleIter)->ModuleName << "' disconnected." << endl;
		RemoveModule(*WorkModuleIter);
	}
	else
	{
		char* TempBuf = new char[status];
		if (TempBuf == NULL)
		{
			ReportError(int(GetLastError()), "new");
		}
		else
		{
			nread = recv((*WorkModuleIter)->SockHandle, TempBuf, int(status), 0);
			if (nread < 0)
			{
				ReportError(WSAGetLastError(), "recv");
				RemoveModule(*WorkModuleIter);
			}
			else
			{
				(*WorkModuleIter)->ReadBuffer.append(TempBuf, (unsigned int)nread);
				ProcessMessage(*WorkModuleIter);
			}
			delete[] TempBuf;
		}
		(*WorkModuleIter)->WarnTime = GlobalRunTime + module_warn;
		(*WorkModuleIter)->EndTime = 0xFFFFFFFFFFFFFFFF;
	}
}

bool MatchesRegEx(string Pattern, string ComparingText)
{
	regex text(Pattern);
	return regex_match(ComparingText, text);
}

bool MatchesWildCard(string WildCard, string ComparingText)
{
	unsigned int TextPos = 0;
	unsigned int WildPos = 0;
	unsigned int TextAft = 0;
	unsigned int WildAft = 0;
	bool Asterisk = false;
	while ((TextPos < ComparingText.size()) && (WildPos < WildCard.size()))
	{
		if (WildCard[WildPos] == '*')
		{
			Asterisk = true;
		}
		else
		{
			/*if (TextPos >= ComparingText.size())
			{
				return false;
			}
			else*/ if (WildCard[WildPos] == '?')
			{
				TextPos++;
			}
			else
			{
				if (Asterisk)
				{
					if ((TextAft = ComparingText.find_first_of(WildCard[WildPos], TextPos)) == ComparingText.npos)
					{
						return false;
					}
					WildAft = WildPos;
					TextPos = TextAft + 1;
					Asterisk = false;
				}
				else if (ComparingText[TextPos] == WildCard[WildPos])
				{
					TextPos++;
				}
				else if (TextAft)
				{
					TextAft++;
					TextPos = TextAft;
					WildPos = WildAft;
					Asterisk = true;
					continue;
				}
				else
				{
					return false;
				}
			}
		}
		WildPos++;
	}
	return (((WildCard[WildCard.size() - 1] == '*') && (WildPos >= WildCard.size() - 1)) || ((TextPos == ComparingText.size()) && (WildPos == WildCard.size())));
}

template <class T>
bool StringToValue(const string s, const unsigned int index)
{
	T t;
	std::istringstream iss(s);
	if (((iss >> boolalpha >> t).fail()) ||
		((PointsVector[index].PointType == POINTTYPE_UINT8) && ((unsigned short)t > 255)) ||
		((PointsVector[index].PointType == POINTTYPE_INT8) && (((signed short)t > 127) || ((signed short)t < -128))))
	{
		return false;
	}
	else
	{
		string NewPointValue;
		NewPointValue.assign((char *)&t, TypeMap[PointsVector[index].PointType]);
		UpdatePointValue(index, PointsVector[index].PointType, PointsVector[index].PointQuality, NewPointValue, ModuleList.end());
		return true;
	}
}

string ProcessCommand(string Command, admin_iter &WorkAdminIter)
{
	vector<string> ParsedCmd;
	stringstream Ans;

	unsigned int TempBegin = 0;
	unsigned int TempEnd = 0;

	while (TempEnd < Command.size())
	{
		TempBegin = Command.find_first_not_of(' ', TempEnd);
		if ((TempBegin < Command.size()) && (Command[TempBegin] == '"'))
		{
			TempEnd = TempBegin + 1;
			do
			{
				TempEnd = Command.find_first_of("\"\\", TempEnd);
				if (Command[TempEnd] == '\\') TempEnd += 2;
			} while ((TempEnd < Command.size()) && (Command[TempEnd] != '"'));
			if (TempEnd >= Command.size())
			{
				Ans << "ERROR Bad string format." << endl << ends;
				return Ans.str();
			}
			TempEnd++;
		}
		else
		{
			TempEnd = Command.find_first_of(' ', TempBegin);
		}
		if ((TempEnd > TempBegin) && (TempBegin != Command.npos))
		{
			ParsedCmd.push_back(Command.substr(TempBegin, TempEnd - TempBegin));
			if (ParsedCmd[ParsedCmd.size() - 1][0] == '"')
			{
				unsigned int TempReplace = 0;
				do
				{
					TempReplace = ParsedCmd[ParsedCmd.size() - 1].find_first_of('\\', TempReplace);
					if (TempReplace < ParsedCmd[ParsedCmd.size() - 1].size() - 1)
					{
						switch (ParsedCmd[ParsedCmd.size() - 1][TempReplace + 1])
						{
						case '\\':
							ParsedCmd[ParsedCmd.size() - 1].erase(TempReplace, 1);
							break;
						case '"':
							ParsedCmd[ParsedCmd.size() - 1].erase(TempReplace, 1);
							break;
						case 'n':
							ParsedCmd[ParsedCmd.size() - 1].erase(TempReplace, 2);
							ParsedCmd[ParsedCmd.size() - 1].insert(TempReplace, "\n");
							break;
						case 'r':
							ParsedCmd[ParsedCmd.size() - 1].erase(TempReplace, 2);
							ParsedCmd[ParsedCmd.size() - 1].insert(TempReplace, "\r");
							break;
						case '0':
							ParsedCmd[ParsedCmd.size() - 1].erase(TempReplace, 2);
							ParsedCmd[ParsedCmd.size() - 1].insert(TempReplace, 1, '\0');
							break;
						default:
							Ans << "ERROR Bad string format with backslash." << endl << ends;
							return Ans.str();
						}
					}
				} while (TempReplace++ < ParsedCmd[ParsedCmd.size() - 1].size());
				ParsedCmd[ParsedCmd.size() - 1].erase(0, 1);
				ParsedCmd[ParsedCmd.size() - 1].erase(ParsedCmd[ParsedCmd.size() - 1].size() - 1, 1);
			}
		}
	}

	if (ParsedCmd.size() < 1) return "";

	if (ParsedCmd[0] == "help")
	{
		Ans << "This is help of this program(v." << VERSION << "). This help is describing so wide problematic areas, that everything possible question about anything in the whole universe is now clear. If not, please contact application developer. Thank you." << endl;
	}
	else if (ParsedCmd[0] == "supported_version")
	{
		if (ParsedCmd.size() != 2)
		{
			Ans << "No version specified behind command" << endl;
		}
		else
		{
			(*WorkAdminIter).SupportedVersion = ParsedCmd[1];

			Ans << "Ok" << endl;
		}
	}
	else if (ParsedCmd[0] == "router")
	{
		Ans << endl << "RTR|" << VERSION << "|" << socket_port << "|" << StartTime << endl;
	}
	else if (ParsedCmd[0] == "tagsinfo")
	{
		Ans << endl << "CNT|" << PointsVector.size() << endl;
	}
	else if (ParsedCmd[0] == "state")
	{
		int redundant = 0;
		string fillAdrr = "";

		if (!ModuleList.empty())
		{
			int germ = 0, fill = 0;

			for (auto m : ModuleList)
			{
				string s = m.ModuleName;
				for (auto &c : s)
					c = tolower(c);

				const string GERM_NAME = "germinator";
				const string FILL_NAME = "fillator";

				if (germ == 0 && s.length() >= GERM_NAME.length() && s.substr(0, GERM_NAME.length()) == GERM_NAME)
					germ = 1;

				if (fill == 0 && s.length() >= FILL_NAME.length() && s.substr(0, FILL_NAME.length()) == FILL_NAME)
				{
					fill = 1;
					fillAdrr = inet_ntoa(m.SockInfo.sin_addr);
				}

				if (fill && germ)
				{
					redundant = 1;
					break;
				}
			}
		}

		Ans << endl << "STA|" << (PointsVector[0].PointValue[0] ? "1" : "0") << "|" << (redundant ? "1" : "0") << "|" << fillAdrr << endl;
	}
	else if (ParsedCmd[0] == "list")
	{
		if (ParsedCmd.size() < 2) ParsedCmd.push_back("*");
		bool ByName = ((ParsedCmd.size() > 2) && (ParsedCmd[2] == "-n"));
		bool ByRegex = ((ParsedCmd.size() > 2) && (ParsedCmd[2] == "-rgx"));
		bool ByModule = ((ParsedCmd.size() > 3) && (ParsedCmd[2] == "-m")) ||
			((ParsedCmd.size() > 4) && (ParsedCmd[3] == "-m"));
		string ByModuleName = !ByModule ? "" : ((ParsedCmd[2] == "-m") ? ParsedCmd[3] : ParsedCmd[4]);
		list<module_iter>::iterator TempModuleIter;
		map_iter TempMapIter;
		long long TempSigned = 0;
		unsigned long long TempUnsigned = 0;
		float TempFloat = 0;
		double TempDouble = 0;
		unsigned int index;
		unsigned int TempSeq;

		Ans << endl << "PNT|";
		for (TempMapIter = PointsMap.begin(), TempSeq = 0; TempSeq < PointsVector.size(); ++TempSeq, ++TempMapIter)
		{
			index = (ByName ? TempMapIter->second : TempSeq);

			if (!ByRegex && MatchesWildCard(ParsedCmd[1], PointsVector[index].PointName)
				|| ByRegex && MatchesRegEx(ParsedCmd[1], PointsVector[index].PointName))
			{
				if (ByName)
				{
					bool isOut = true;
					for (auto i : PointsVector[index].Consumer)
					{
						if (i->ModuleName == ByModuleName)
						{
							isOut = false;
							break;
						}
					}

					if (isOut)
						for (auto i : PointsVector[index].Producer)
						{
							if (i->ModuleName == ByModuleName)
							{
								isOut = false;
								break;
							}
						}

					if (isOut)
						continue;
				}

				Ans << index << "|" << PointsVector[index].PointName << "|" << int(PointsVector[index].PointQuality) << "|" << int(PointsVector[index].PointType) << "|" << (PointsVector[index].Forced ? "f" : "n") << endl;
				switch (PointsVector[index].PointType)
				{
				case POINTTYPE_NONE:
					Ans << endl;
					break;
				case POINTTYPE_BOOL:
					if (PointsVector[index].PointValue[0])
						Ans << "true" << endl;
					else
						Ans << "false" << endl;
					break;
				case POINTTYPE_INT8:
				case POINTTYPE_INT16:
				case POINTTYPE_INT32:
				case POINTTYPE_INT64:
					TempSigned = 0;
					if ((signed char)(PointsVector[index].PointValue[PointsVector[index].PointValue.size() - 1]) < 0)
					{
						TempSigned = ~TempSigned;
					}
					memcpy(&TempSigned, PointsVector[index].PointValue.data(), PointsVector[index].PointValue.size());
					Ans << TempSigned << endl;
					break;
				case POINTTYPE_UINT8:
				case POINTTYPE_UINT16:
				case POINTTYPE_UINT32:
				case POINTTYPE_UINT64:
					TempUnsigned = 0;
					memcpy(&TempUnsigned, PointsVector[index].PointValue.data(), PointsVector[index].PointValue.size());
					Ans << TempUnsigned << endl;
					break;
				case POINTTYPE_FLOAT:
					TempFloat = 0;
					memcpy(&TempFloat, PointsVector[index].PointValue.data(), PointsVector[index].PointValue.size());
					Ans << TempFloat << endl;
					break;
				case POINTTYPE_DOUBLE:
					TempDouble = 0;
					memcpy(&TempDouble, PointsVector[index].PointValue.data(), PointsVector[index].PointValue.size());
					Ans << TempDouble << endl;
					break;
				case POINTTYPE_STREAM:
					Ans << "0x" << setbase(16) << setfill('0') << uppercase;
					for (unsigned int i = 0; i < PointsVector[index].PointValue.size(); i++)
					{
						Ans << setw(2) << ((unsigned short)(PointsVector[index].PointValue[i]) & 0x00FF);
					}
					Ans << setbase(10) << nouppercase << endl;
					break;
				case POINTTYPE_STRING:
				case POINTTYPE_XML:
				case POINTTYPE_BASE64:
				{
					string OutputString = PointsVector[index].PointValue;
					unsigned int TempPos = 0;
					string SearchString = "\\\"\r\n";
					SearchString.append(1, '\0');
					while ((TempPos = OutputString.find_first_of(SearchString, TempPos)) < OutputString.size())
					{
						switch (OutputString[TempPos])
						{
						case '\\':
						case '"':
							OutputString.insert(TempPos, "\\");
							TempPos += 2;
							break;
						case '\r':
							OutputString.insert(TempPos, "\\r");
							TempPos += 2;
							OutputString.erase(TempPos, 1);
							break;
						case '\n':
							OutputString.insert(TempPos, "\\n");
							TempPos += 2;
							OutputString.erase(TempPos, 1);
							break;
						case '\0':
							OutputString.insert(TempPos, "\\0");
							TempPos += 2;
							OutputString.erase(TempPos, 1);
							break;
						}
					}
					Ans << '"' << OutputString << '"' << endl;
				}
				break;
				}

				Ans << "<";
				for (TempModuleIter = PointsVector[index].Producer.begin(); TempModuleIter != PointsVector[index].Producer.end(); ++TempModuleIter)
				{
					if (TempModuleIter != PointsVector[index].Producer.begin()) Ans << "|";
					if (*TempModuleIter == PointsVector[index].LastChangedBy) Ans << "*";
					Ans << (*TempModuleIter)->ModuleName;
				}
				Ans << ">" << endl;

				Ans << "<";
				for (TempModuleIter = PointsVector[index].Consumer.begin(); TempModuleIter != PointsVector[index].Consumer.end(); ++TempModuleIter)
				{
					if (TempModuleIter != PointsVector[index].Consumer.begin()) Ans << "|";
					Ans << (*TempModuleIter)->ModuleName;
				}
				Ans << ">" << endl;
			}
		}
	}
	else if (ParsedCmd[0] == "set")
	{
		if (ParsedCmd.size() != 4)
		{
			Ans << "ERROR Incorrect number of parameters." << endl;
		}
		else
		{
			char * error;
			unsigned int index;
			index = strtoul(ParsedCmd[1].c_str(), &error, 10);
			if (error[0] != 0)
			{
				Ans << "ERROR Bad point index." << endl;
			}
			else if (index >= PointsVector.size())
			{
				Ans << "ERROR Point index out of range." << endl;
			}
			else
			{
				if (ParsedCmd[2] == "type")
				{
					unsigned int NewPointType = strtoul(ParsedCmd[3].c_str(), &error, 10);
					if (error[0] != 0)
					{
						Ans << "ERROR Bad type" << endl;
					}
					else
					{
						string NewPointValue;
						switch (NewPointType)
						{
						case POINTTYPE_NONE:
						case POINTTYPE_STRING:
						case POINTTYPE_STREAM:
						case POINTTYPE_XML:
						case POINTTYPE_BASE64:
							break;
						case POINTTYPE_BOOL:
						case POINTTYPE_INT8:
						case POINTTYPE_UINT8:
							NewPointValue.assign("\000", 1);
							break;
						case POINTTYPE_INT16:
						case POINTTYPE_UINT16:
							NewPointValue.assign("\000\000", 2);
							break;
						case POINTTYPE_INT32:
						case POINTTYPE_UINT32:
						case POINTTYPE_FLOAT:
							NewPointValue.assign("\000\000\000\000", 4);
							break;
						case POINTTYPE_INT64:
						case POINTTYPE_UINT64:
						case POINTTYPE_DOUBLE:
							NewPointValue.assign("\000\000\000\000\000\000\000\000", 8);
							break;
						default:
							Ans << "ERROR Bad point type." << endl << ends;
							return Ans.str();
						}
						UpdatePointValue(index, NewPointType, PointsVector[index].PointQuality, NewPointValue, ModuleList.end());
						Ans << "ACK set" << endl;
					}
				}
				else if (ParsedCmd[2] == "value")
				{
					switch (PointsVector[index].PointType)
					{
					case POINTTYPE_NONE:
						Ans << "ERROR Assign of type NONE not implemented." << endl;
						break;
					case POINTTYPE_BOOL:
						if (StringToValue<bool>(ParsedCmd[3], index)) Ans << "ACK set" << endl; else Ans << "ERROR Bad BOOL value." << endl;
						break;
					case POINTTYPE_INT8:
						if (StringToValue<signed short>(ParsedCmd[3], index)) Ans << "ACK set" << endl; else Ans << "ERROR Bad INT8 value." << endl;
						break;
					case POINTTYPE_INT16:
						if (StringToValue<signed short>(ParsedCmd[3], index)) Ans << "ACK set" << endl; else Ans << "ERROR Bad INT16 value." << endl;
						break;
					case POINTTYPE_INT32:
						if (StringToValue<signed long>(ParsedCmd[3], index)) Ans << "ACK set" << endl; else Ans << "ERROR Bad INT32 value." << endl;
						break;
					case POINTTYPE_INT64:
						if (StringToValue<signed long long>(ParsedCmd[3], index)) Ans << "ACK set" << endl; else Ans << "ERROR Bad INT64 value." << endl;
						break;
					case POINTTYPE_UINT8:
						if (StringToValue<unsigned short>(ParsedCmd[3], index)) Ans << "ACK set" << endl; else Ans << "ERROR Bad UINT8 value." << endl;
						break;
					case POINTTYPE_UINT16:
						if (StringToValue<unsigned short>(ParsedCmd[3], index)) Ans << "ACK set" << endl; else Ans << "ERROR Bad UINT16 value." << endl;
						break;
					case POINTTYPE_UINT32:
						if (StringToValue<unsigned long>(ParsedCmd[3], index)) Ans << "ACK set" << endl; else Ans << "ERROR Bad UINT32 value." << endl;
						break;
					case POINTTYPE_UINT64:
						if (StringToValue<unsigned long long>(ParsedCmd[3], index)) Ans << "ACK set" << endl; else Ans << "ERROR Bad UINT64 value." << endl;
						break;
					case POINTTYPE_FLOAT:
						if (StringToValue<float>(ParsedCmd[3], index)) Ans << "ACK set" << endl; else Ans << "ERROR Bad FLOAT value." << endl;
						break;
					case POINTTYPE_DOUBLE:
						if (StringToValue<double>(ParsedCmd[3], index)) Ans << "ACK set" << endl; else Ans << "ERROR Bad DOUBLE value." << endl;
						break;
					case POINTTYPE_STREAM:
						if ((ParsedCmd[3].size() < 2) || (((ParsedCmd[3].substr(0, 2) != "0x") && (ParsedCmd[3].substr(0, 2) != "0X"))))
						{
							Ans << "ERROR Bad STREAM value." << endl;
						}
						else
						{
							unsigned int TempNum;
							string NewPointValue;
							unsigned int TempPos = 2;
							while (TempPos < ParsedCmd[3].size())
							{
								stringstream TempPointValue;
								TempPointValue << hex << ParsedCmd[3].substr(TempPos, 2);
								if ((TempPointValue >> TempNum).fail())
								{
									Ans << "ERROR Bad STREAM value." << endl << ends;
									return Ans.str();
								}
								else
								{
									NewPointValue.append((char *)&TempNum, 1);
									TempPos += 2;
								}
							}
							UpdatePointValue(index, PointsVector[index].PointType, PointsVector[index].PointQuality, NewPointValue, ModuleList.end());
							Ans << "ACK set" << endl;
						}
						break;
					case POINTTYPE_STRING:
					case POINTTYPE_XML:
						UpdatePointValue(index, PointsVector[index].PointType, PointsVector[index].PointQuality, ParsedCmd[3], ModuleList.end());
						Ans << "ACK set" << endl;
						break;
					case POINTTYPE_BASE64:
						for (unsigned int i = 0; i < ParsedCmd[3].size(); i++)
						{
							if (!((ParsedCmd[3][i] >= 'A' && ParsedCmd[3][i] <= 'Z') ||
								(ParsedCmd[3][i] >= 'a' && ParsedCmd[3][i] <= 'z') ||
								(ParsedCmd[3][i] >= '0' && ParsedCmd[3][i] <= '9') ||
								(ParsedCmd[3][i] == '/' || ParsedCmd[3][i] == '+' || ParsedCmd[3][i] == '=')))
							{
								Ans << "ERROR Bad BASE64 value." << endl << ends;
								return Ans.str();
							}
						}
						UpdatePointValue(index, PointsVector[index].PointType, PointsVector[index].PointQuality, ParsedCmd[3], ModuleList.end());
						Ans << "ACK set" << endl;
						break;
					}
				}
				else if (ParsedCmd[2] == "quality")
				{
					unsigned int NewPointQuality = strtoul(ParsedCmd[3].c_str(), &error, 10);
					if (error[0] != 0)
					{
						Ans << "ERROR Bad quality." << endl;
					}
					else if (NewPointQuality > 255)
					{
						Ans << "ERROR Quality out of range." << endl;
					}
					else
					{
						UpdatePointValue(index, PointsVector[index].PointType, (unsigned char)(NewPointQuality), PointsVector[index].PointValue, ModuleList.end());
						Ans << "ACK set" << endl;
					}
				}
				else if (ParsedCmd[2] == "name")
				{
					map<string, unsigned int>::iterator PointsMapIterator = PointsMap.find(ParsedCmd[3]);
					if (PointsMapIterator == PointsMap.end())
					{
						PointsMap.erase(PointsVector[index].PointName);
						PointsVector[index].PointName = ParsedCmd[3];
						PointsMap.insert(pair<string, int>(ParsedCmd[3], index));
						Ans << "ACK set" << endl;
					}
					else
					{
						Ans << "ERROR Point name " << ParsedCmd[3] << " already exists." << endl;
					}

				}
				else
				{
					Ans << "ERROR Unknown parameter (" << ParsedCmd[2] << ") for set command." << endl;
				}
			}
		}
	}
	else if (ParsedCmd[0] == "modules")
	{
		if (ParsedCmd.size() < 2) ParsedCmd.push_back("*");
		Ans << endl << "MOD|";
		module_iter TempModuleIter;
		for (TempModuleIter = ModuleList.begin(); TempModuleIter != ModuleList.end(); ++TempModuleIter)
		{
			if ((MatchesWildCard(ParsedCmd[1], TempModuleIter->ModuleName)) && (TempModuleIter->SockHandle != INVALID_SOCKET))
			{
				if (supportAdminThisVersion(WorkAdminIter, "1.17"))
					Ans << TempModuleIter->ModuleName << "|" << TempModuleIter->State <<
					"|" << TempModuleIter->StateChangedTime << "|" << TempModuleIter->StateBeforeChange <<
					"|" << TempModuleIter->StartTime <<
					"|" << inet_ntoa(TempModuleIter->SockInfo.sin_addr) << endl;
				else
					Ans << TempModuleIter->ModuleName << "|" << int(PointsVector[0].PointValue[0]) << endl;
			}
		}
	}
	else if (ParsedCmd[0] == "sleep")
	{
		string s;
		s.assign("\000", 1);
		UpdatePointValue(0, POINTTYPE_BOOL, 255, s, ModuleList.end());
		Ans << "ACK sleep" << endl;
	}
	else if (ParsedCmd[0] == "wake")
	{
		string s;
		s.assign("\001", 1);
		UpdatePointValue(0, POINTTYPE_BOOL, 255, s, ModuleList.end());
		Ans << "ACK wake" << endl;
	}
	else if (ParsedCmd[0] == "disconnect")
	{
		int i = 0;
		if (ParsedCmd.size() < 2) ParsedCmd.push_back("*");
		module_iter TempModuleIter, NextModuleIter;
		for (TempModuleIter = ModuleList.begin(); TempModuleIter != ModuleList.end(); ++TempModuleIter)
		{
			if (MatchesWildCard(ParsedCmd[1], TempModuleIter->ModuleName))
			{
				i++;
				NextModuleIter = TempModuleIter;
				NextModuleIter++;
				RemoveModule(TempModuleIter);
				TempModuleIter = NextModuleIter;
				if (TempModuleIter == ModuleList.end()) break;
			}
		}
		Ans << "ACK disconnect" << endl;
	}
	else if (ParsedCmd[0] == "force")
	{
		if (ParsedCmd.size() < 2)
		{
			Ans << "ERROR Not enough parameters" << endl;
		}
		else
		{
			char * error;
			unsigned int index;
			index = strtoul(ParsedCmd[1].c_str(), &error, 10);
			if (error[0] != 0)
			{
				Ans << "ERROR Bad point index." << endl;
			}
			else if (index >= PointsVector.size())
			{
				Ans << "ERROR Point index out of range." << endl;
			}
			else
			{
				if (!PointsVector[index].Forced)
				{
					PointsVector[index].Forced = true;
					ForcedMap[index] = index;
				}
				Ans << "ACK force" << endl;
			}
		}
	}
	else if (ParsedCmd[0] == "unforce")
	{
		if (ParsedCmd.size() < 2)
		{
			Ans << "ERROR Not enough parameters" << endl;
		}
		else if (ParsedCmd[1] == "all")
		{
			map<unsigned int, unsigned int>::iterator ForcedMapIter;
			for (ForcedMapIter = ForcedMap.begin(); ForcedMapIter != ForcedMap.end(); ++ForcedMapIter)
			{
				PointsVector[ForcedMapIter->first].Forced = false;
			}
			ForcedMap.clear();
			Ans << "ACK unforce all" << endl;
		}
		else
		{
			char * error;
			unsigned int index;
			index = strtoul(ParsedCmd[1].c_str(), &error, 10);
			if (error[0] != 0)
			{
				Ans << "ERROR Bad point index." << endl;
			}
			else if (index >= PointsVector.size())
			{
				Ans << "ERROR Point index out of range." << endl;
			}
			else
			{
				if (PointsVector[index].Forced)
				{
					PointsVector[index].Forced = false;
					ForcedMap.erase(index);
				}
				Ans << "ACK unforce" << endl;
			}
		}
	}
	else if (ParsedCmd[0] == "live")
	{
	}
	else
	{
		Ans << "ERROR Unknown command." << endl;
	}
	Ans << ends;
	return Ans.str();
}

void ProcessAdminRead(admin_iter* WorkAdminIter)
{
	u_long status;
	int nread;
	ioctlsocket((*WorkAdminIter)->SockHandle, FIONREAD, &status);
	if (status == 0)
	{
		RemoveAdminModule(WorkAdminIter);
	}
	else
	{
		char* TempBuf = new char[status];
		(*WorkAdminIter)->WarnTime = GlobalRunTime + admin_warn;
		(*WorkAdminIter)->EndTime = 0xFFFFFFFFFFFFFFFF;
		if (TempBuf == NULL)
		{
			ReportError(int(GetLastError()), "new");
		}
		else
		{
			nread = recv((*WorkAdminIter)->SockHandle, TempBuf, int(status), 0);
			if (nread < 0)
			{
				ReportError(WSAGetLastError(), "recv");
				RemoveAdminModule(WorkAdminIter);
			}
			else
			{
				size_t EnterPos;
				(*WorkAdminIter)->ReadBuffer.append(TempBuf, (unsigned int)nread);
				while ((EnterPos = (*WorkAdminIter)->ReadBuffer.find_first_of("\r\n")) != (*WorkAdminIter)->ReadBuffer.npos)
				{
					if (EnterPos > 0)
					{
						string Query = (*WorkAdminIter)->ReadBuffer.substr(0, EnterPos);
						if (Query == "exit")
						{
							RemoveAdminModule(WorkAdminIter);
							break;
						}
						else
						{
							(*WorkAdminIter)->WriteBuffer.append(ProcessCommand(Query, *WorkAdminIter));
						}
					}
					(*WorkAdminIter)->ReadBuffer.erase(0, EnterPos + 1);
				}
			}
			delete[] TempBuf;
		}
	}
}

SOCKET CreateListenSocket(unsigned short Port)
{
	SOCKET WorkSocket;
	WorkSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (WorkSocket == INVALID_SOCKET)
	{
		ReportError(WSAGetLastError(), "socket");
		WSACleanup();
		return INVALID_SOCKET;
	}

	sockaddr_in ServerAddress;	// pøiøazení adresy k socketu
	ServerAddress.sin_family = AF_INET;
	ServerAddress.sin_addr.s_addr = ADDR_ANY; //inet_addr(INADDR_ANY)
	ServerAddress.sin_port = htons(Port);
	if (bind(WorkSocket, (sockaddr*)&ServerAddress, sizeof(ServerAddress)) == SOCKET_ERROR)
	{
		ReportError(WSAGetLastError(), "bind");
		closesocket(WorkSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}

	if (listen(WorkSocket, max_module_count) == SOCKET_ERROR)
	{
		ReportError(WSAGetLastError(), "listen");
		closesocket(WorkSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}
	return WorkSocket;
}

void ControlHandler(DWORD request)
{
	switch (request)
	{
	case SERVICE_CONTROL_STOP:
		*msg_out << ReportHead() << "Service stopped." << endl;

		ServiceStatus.dwWin32ExitCode = 0;
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(hStatus, &ServiceStatus);
		return;

	case SERVICE_CONTROL_SHUTDOWN:
		*msg_out << ReportHead() << "Service stopped." << endl;

		ServiceStatus.dwWin32ExitCode = 0;
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(hStatus, &ServiceStatus);
		return;

	default:
		break;
	}

	// Report current status
	SetServiceStatus(hStatus, &ServiceStatus);

	return;
}

int ServiceMain(int argc, char** argv)
{
	if (argc == 1)
	{
		ServiceStatus.dwServiceType = SERVICE_WIN32;
		ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
		ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
		ServiceStatus.dwWin32ExitCode = 0;
		ServiceStatus.dwServiceSpecificExitCode = 0;
		ServiceStatus.dwCheckPoint = 0;
		ServiceStatus.dwWaitHint = 0;

		hStatus = RegisterServiceCtrlHandler(LPCWSTR("DK_Router"), (LPHANDLER_FUNCTION)ControlHandler);
		if (hStatus == (SERVICE_STATUS_HANDLE)0)
		{
			// Registering Control Handler failed
			*msg_out << ReportHead() << "Registering service control handler failed. Without parameters must be run as service, otherwise add any parameter to command line." << endl;
			return 1;
		}
	}

	Init();
	{
		wchar_t szPath[MAX_PATH] = { 0 };
		GetModuleFileName(NULL, szPath, MAX_PATH);
		wstring IniPath = szPath;
		IniPath.replace(IniPath.size() - 3, 3, L"ini");

		socket_port = GetPrivateProfileInt(L"Basic", L"socket_port", DEFAULT_SOCKET_PORT, IniPath.c_str());
		admin_port = GetPrivateProfileInt(L"Basic", L"admin_port", DEFAULT_ADMIN_PORT, IniPath.c_str());
		max_module_count = GetPrivateProfileInt(L"Basic", L"max_module_count", DEFAULT_MAX_MODULE_COUNT, IniPath.c_str());
		module_warn = GetPrivateProfileInt(L"Basic", L"module_warn", DEFAULT_MODULE_WARN, IniPath.c_str());
		module_end = GetPrivateProfileInt(L"Basic", L"module_end", DEFAULT_MODULE_END, IniPath.c_str());
		module_discard = GetPrivateProfileInt(L"Basic", L"module_discard", DEFAULT_MODULE_DISCARD, IniPath.c_str());
		admin_warn = GetPrivateProfileInt(L"Basic", L"admin_warn", DEFAULT_ADMIN_WARN, IniPath.c_str());
		admin_end = GetPrivateProfileInt(L"Basic", L"admin_end", DEFAULT_ADMIN_END, IniPath.c_str());
		if (GetPrivateProfileInt(L"Basic", L"activity", DEFAULT_ACTIVITY, IniPath.c_str()) == 0)
		{
			PointsVector[0].PointValue.assign("\000", 1);
		}
		else
		{
			PointsVector[0].PointValue.assign("\001", 1);
		}
	}

	WSADATA wsaData;	// inicializace Winsock
	if (WSAStartup(0xFFFF, &wsaData) != NO_ERROR)
	{
		ReportError(WSAGetLastError(), "WSAStartup");
		if (argc == 1)
		{
			ServiceStatus.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(hStatus, &ServiceStatus);
		}
		return 1;
	}

	SOCKET ListenSocket = CreateListenSocket(socket_port);
	if (ListenSocket == INVALID_SOCKET)
	{
		if (argc == 1)
		{
			ServiceStatus.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(hStatus, &ServiceStatus);
		}
		return 1;
	}
	SOCKET AdminSocket = CreateListenSocket(admin_port);

	*msg_out << ReportHead() << "Router (version " << VERSION << ") ready." << endl;

	if (argc == 1)
	{
		ServiceStatus.dwCurrentState = SERVICE_RUNNING;
		SetServiceStatus(hStatus, &ServiceStatus);
	}

	fd_set fdRead, fdWrite;
	int fdCount, TempSize;
	module_iter ModuleListIter;
	admin_iter AdminListIter;
	data_module TempModule;
	admin_module TempAdmin;
	unsigned long long NextTime;
	timeval timeout;

	while (1)
		//	while ((argc > 1) || (ServiceStatus.dwCurrentState == SERVICE_RUNNING))
	{
		NextTime = 0xFFFFFFFFFFFFFFFF;
		//NextTime = GlobalRunTime + CHECK_TIME;
		FD_ZERO(&fdRead);
		FD_SET(ListenSocket, &fdRead);
		FD_SET(AdminSocket, &fdRead);
		FD_ZERO(&fdWrite);
		for (ModuleListIter = ModuleList.begin(); ModuleListIter != ModuleList.end(); )
		{
			//			FinishNewMessage(ModuleListIter);
			if (GlobalRunTime >= ModuleListIter->WarnTime)
			{
				ModuleListIter->WarnTime = 0xFFFFFFFFFFFFFFFF;
				ModuleListIter->EndTime = GlobalRunTime + module_end;
				PrepareNewMessage(ModuleListIter, MSGTYPE_ASK);
				//				FinishNewMessage(ModuleListIter);
			}
			if (GlobalRunTime >= ModuleListIter->EndTime)
			{
				if (ModuleListIter->SockHandle != INVALID_SOCKET)
				{
					*msg_out << ReportHead() << "Module '" << ModuleListIter->ModuleName << "' timed out." << endl;
					RemoveModule(ModuleListIter);
				}
				else
				{
					*msg_out << ReportHead() << "Module '" << ModuleListIter->ModuleName << "' discarded." << endl;
					DiscardModule(&ModuleListIter);
					continue;
				}
			}

			if (NextTime > ModuleListIter->WarnTime) NextTime = ModuleListIter->WarnTime;
			if (NextTime > ModuleListIter->EndTime) NextTime = ModuleListIter->EndTime;

			if (ModuleListIter->SockHandle != INVALID_SOCKET)
			{
				FD_SET(ModuleListIter->SockHandle, &fdRead);
				if (!ModuleListIter->WriteBuffer.empty())
				{
					FD_SET(ModuleListIter->SockHandle, &fdWrite);
				}
			}
			++ModuleListIter;
		}
		for (AdminListIter = AdminList.begin(); AdminListIter != AdminList.end(); )
		{
			if (GlobalRunTime >= AdminListIter->WarnTime)
			{
				AdminListIter->WarnTime = 0xFFFFFFFFFFFFFFFF;
				AdminListIter->EndTime = GlobalRunTime + admin_end;
				AdminListIter->WriteBuffer.append("live\n");
				AdminListIter->WriteBuffer.append(1, '\0');
			}
			if (GlobalRunTime >= AdminListIter->EndTime)
			{
				RemoveAdminModule(&AdminListIter);
				break;
			}

			if (NextTime > AdminListIter->WarnTime) NextTime = AdminListIter->WarnTime;
			if (NextTime > AdminListIter->EndTime) NextTime = AdminListIter->EndTime;

			FD_SET(AdminListIter->SockHandle, &fdRead);
			if (!AdminListIter->WriteBuffer.empty())
			{
				FD_SET(AdminListIter->SockHandle, &fdWrite);
			}
			++AdminListIter;
		}
		if ((NextTime = NextTime - GlobalRunTime) < 1000)
		{
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
		}
		else
		{
			timeout.tv_sec = int((NextTime / 1000) & 0x000000007FFFFFFF);
			timeout.tv_usec = int(NextTime % 1000);
		}
		fdCount = select(AdminList.size() + ModuleList.size() + 2, &fdRead, &fdWrite, NULL, &timeout);
		GlobalRunTime += DWORD(GetTickCount() - DWORD(GlobalRunTime));
		//		*msg_out << ReportHead() << NextTime << endl;
		if (fdCount == SOCKET_ERROR)
		{
			ReportError(WSAGetLastError(), "select");
		}
		else if (fdCount > 0)
		{
			for (ModuleListIter = ModuleList.begin();
				(ModuleListIter != ModuleList.end()) && (fdCount > 0);
				++ModuleListIter)
				if (FD_ISSET(ModuleListIter->SockHandle, &fdRead))
				{
					fdCount--;
					ProcessRead(&ModuleListIter);
					if (ModuleListIter == ModuleList.end())
					{
						break;
					}
				}

			for (ModuleListIter = ModuleList.begin();
				(ModuleListIter != ModuleList.end()) && (fdCount > 0);
				++ModuleListIter)
				if (FD_ISSET(ModuleListIter->SockHandle, &fdWrite))
				{
					fdCount--;
					ProcessWrite(ModuleListIter);
				}

			for (AdminListIter = AdminList.begin();
				(AdminListIter != AdminList.end()) && (fdCount > 0);
				++AdminListIter)
				if (FD_ISSET(AdminListIter->SockHandle, &fdRead))
				{
					fdCount--;
					ProcessAdminRead(&AdminListIter);
					if (AdminListIter == AdminList.end())
					{
						break;
					}
				}

			for (AdminListIter = AdminList.begin();
				(AdminListIter != AdminList.end()) && (fdCount > 0);
				++AdminListIter)
				if (FD_ISSET(AdminListIter->SockHandle, &fdWrite))
				{
					fdCount--;
					ProcessAdminWrite(AdminListIter);
				}

			if ((fdCount > 0) && (FD_ISSET(ListenSocket, &fdRead)))
			{
				TempSize = sizeof(TempModule.SockInfo);
				TempModule.SockHandle = accept(ListenSocket, (sockaddr*)&TempModule.SockInfo, &TempSize);
				if (TempModule.SockHandle == INVALID_SOCKET)
				{
					ReportError(WSAGetLastError(), "accept");
				}
				else
				{
					TempModule.LastMessagePos = 0xFFFFFFFF;
					TempModule.ModuleName = "?";
					TempModule.ConsumingAll = false;
					TempModule.ProducingAll = false;
					TempModule.WarnTime = GlobalRunTime + module_warn;
					TempModule.EndTime = 0xFFFFFFFFFFFFFFFF;
					TempModule.StartTime = getNow();
					TempModule.StateChangedTime = TempModule.StartTime;
					TempModule.State = STATE_MODULE_CONNECTED;
					if(PointsVector[0].PointValue[0] == 1)
						TempModule.State |= STATE_MODULE_ACTIVE;
					TempModule.StateBeforeChange = 0;
					ModuleList.push_back(TempModule);
					*msg_out << ReportHead() << "New module connected." << endl;
				}
				fdCount--;
			}

			if ((fdCount > 0) && (FD_ISSET(AdminSocket, &fdRead)))
			{
				TempSize = sizeof(TempAdmin.SockInfo);
				TempAdmin.SockHandle = accept(AdminSocket, (sockaddr*)&TempAdmin.SockInfo, &TempSize);
				if (TempAdmin.SockHandle == INVALID_SOCKET)
				{
					ReportError(WSAGetLastError(), "accept");
				}
				else
				{
					TempAdmin.WarnTime = GlobalRunTime + admin_warn;
					TempAdmin.EndTime = 0xFFFFFFFFFFFFFFFF;
					AdminList.push_back(TempAdmin);
				}
				fdCount--;
			}
		}
	}
	return 0;
}

/*bool CheckKey(wchar_t szPath[MAX_PATH])
{
	string strAppName;
	string strKeyName;

	wstring basicwstring(szPath);
	string basicstring(basicwstring.begin(), basicwstring.end());

	strAppName = basicstring;
	strAppName = strAppName.substr(strAppName.rfind("\\") + 1);

	strKeyName = basicstring;
	strKeyName = strKeyName.replace(strKeyName.size() - 3, 3, "key");

	*msg_out << ReportHead() << strAppName << " " << strKeyName << endl;

//	cout << "Key file name = " << strKeyName << endl << endl;

	std::ifstream ifs(strKeyName);
	std::string content( (std::istreambuf_iterator<char>(ifs) ),
						(std::istreambuf_iterator<char>()    ) );
	ifs.close();

	return CheckKey(content, strAppName);
}*/

int main(int argc, char** argv)
{
	StartTime = getNow();

	if (argc > 1)
	{
		/*if (CheckKey() == false)
		{
			*msg_out << ReportHead() << "Router is not licensed for this computer. Please obtain registration code." << endl;
			return 1;
		}*/

		return ServiceMain(argc, argv);
	}
	else
	{
		wchar_t szPath[MAX_PATH] = { 0 };
		GetModuleFileName(NULL, szPath, MAX_PATH);
		wstring LogPath = szPath;
		LogPath.replace(LogPath.size() - 3, 3, L"log");
		msg_out = new ofstream(LogPath, ios_base::app);

		/*if (CheckKey(szPath) == false)
		{
			*msg_out << ReportHead() << "Router is not licensed for this computer. Please obtain registration code." << endl;
			return 1;
		}*/

		SERVICE_TABLE_ENTRY ServiceTable[2];
		ServiceTable[0].lpServiceName = LPWSTR("DK_Router");
		ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;
		ServiceTable[1].lpServiceName = NULL;
		ServiceTable[1].lpServiceProc = NULL;
		// Start the control dispatcher thread for our service
		StartServiceCtrlDispatcher(ServiceTable);
		return 0;
	}
}