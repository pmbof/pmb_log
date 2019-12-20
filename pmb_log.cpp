#include "stdafx.h"
#include "pmb_log.h"

#include <iostream>
#include <iomanip>
#include <sys/timeb.h>



namespace pmb
{


log* log::_instance = NULL;

CCriticalSection log::_cs;


log* log::instance(log_type logLevel)
{
	_cs.Lock();
	if (!_instance)
	{
		AfxTrace(L"Creating instance pmb::log to stdout\n");
		_instance = new log;
		_instance->_logLevel = logLevel;
	}
	_cs.Unlock();
	return _instance;
}

log* log::instance(log_type logLevel, const char* filename, bool bColored, bool bLevelFunction)
{
	_cs.Lock();
	if (!_instance)
	{
		tm now;
		time_t t = time(0);
		localtime_s(&now, &t);

		CStringA ofn(filename);

		int r = _gen_filename(now, ofn);

		AfxTrace(L"Creating instance pmb::log(\"%s\")\n", (LPCTSTR)CString(filename));
		_instance = new log(ofn, bColored, bLevelFunction);
		_instance->_logLevel = logLevel;

		if (r)
		{
			_instance->_tm = now;
			_instance->_iFnDate = r;
			_instance->_oFilename = filename;
		}
	}
	_cs.Unlock();
	return _instance;
}




int log::_gen_filename(const tm& now, CStringA& ofn)
{
	CStringA replace;
	replace.Format("%04d", 1900 + now.tm_year);
	int r = 0 < ofn.Replace("%YYYY%", replace) ? 0x01 : 0;
	replace.Format("%02d", now.tm_year % 100);
	r |= 0 < ofn.Replace("%YY%", replace) ? 0x01 : 0;
	replace.Format("%02d", 1 + now.tm_mon);
	r |= 0 < ofn.Replace("%MM%", replace) ? 0x02 : 0;
	replace.Format("%02d", now.tm_mday);
	r |= ofn.Replace("%DD%", replace) ? 0x04 : 0;
	replace.Format("%02d", now.tm_hour);
	r |= ofn.Replace("%HH%", replace) ? 0x08 : 0;
	return r;
}



log* log::instance(bool bprintDate)
{
	if (_instance && bprintDate)
	{
		_cs.Lock();
		_instance->_bPrintDate = true;
		_cs.Unlock();
	}
	return _instance;
}


log* log::beginFunction(log_type type, const char* functionName, bool bprintDate)
{
	if (_instance)
	{
		_cs.Lock();
		_instance->_bPrintDate = bprintDate;
		const DWORD thrId = GetThreadId(GetCurrentThread());
		if (_instance->_levelFunction)
			_instance->_functionName[thrId].push_back(functionName);
		else
			_instance->_sfname = functionName;
		if (type <= _instance->_logLevel)
		{
			if (_instance->_bPrintDate)
				_instance->printDate();
			_instance->getThreadId();
			_instance->printType(type);
			*_instance << "Begin function:   [" << thrId;
			if (_instance->_levelFunction)
				*_instance << '.' << _instance->_functionName[thrId].size();
			*_instance << "]\n";
		}
		_cs.Unlock();
	}
	return _instance;
}



log* log::endFunction(log_type type, const char* functionName, bool bprintDate)
{
	if (_instance && _instance->_levelFunction)
	{
		_cs.Lock();
		_instance->_bPrintDate = bprintDate;
		const DWORD thrId = GetThreadId(GetCurrentThread());
		mthrid_vstr::iterator ithr = _instance->_functionName.find(thrId);
		if (type <= _instance->_logLevel)
		{
			if (functionName && (ithr != _instance->_functionName.end() && (!ithr->second.size() || ithr->second.back() != functionName)))
				ithr->second.push_back(functionName);
			else if (functionName && ithr == _instance->_functionName.end())
			{
				_instance->_functionName[thrId].push_back(functionName);
				ithr = _instance->_functionName.find(thrId);
			}
			if (_instance->_bPrintDate)
				_instance->printDate();
			_instance->getThreadId();
			_instance->printType(type);
			if (ithr != _instance->_functionName.end())
				*_instance << "End function.  [" << thrId << '.' << ithr->second.size() << "]\n";
			else
				*_instance << "End function.  [-.0]\n";
		}
		if (ithr != _instance->_functionName.end() && ithr->second.size())
			ithr->second.pop_back();
		_cs.Unlock();
	}
	else if (_instance && _instance->_sfname)
	{
		_cs.Lock();
		_instance->_sfname = NULL;
		_cs.Unlock();
	}
	return _instance;
}



log* log::endFunction()
{
	if (_instance && _instance->_levelFunction)
	{
		_cs.Lock();
		mthrid_vstr::iterator ithr = _instance->_functionName.find(GetThreadId(GetCurrentThread()));
		if (ithr != _instance->_functionName.end() && ithr->second.size())
			ithr->second.pop_back();
		_cs.Unlock();
	}
	else if (_instance && _instance->_levelFunction)
	{
		_cs.Lock();
		_instance->_sfname = NULL;
		_cs.Unlock();
	}
	return _instance;
}


void log::destroy()
{
	if (_instance)
	{
		delete _instance;
		_instance = nullptr;
	}
}



log::log()
	: _bMillisec(true), _iFnDate(0)
{
	set_rdbuf(std::cout.rdbuf());
}


log::log(const char* filename, bool bColored, bool bLevelFunction)
	: std::ofstream(filename, std::ofstream::out | std::ofstream::app), _filename(filename), _bColored(bColored), _levelFunction(bLevelFunction), _bMillisec(true), _iFnDate(0)
{
}


log::~log()
{
	close();
}


void log::check_filename(const tm& now)
{
	if (_iFnDate && (_lastcheck.tm_min != now.tm_min || _lastcheck.tm_hour != now.tm_hour || _lastcheck.tm_yday != now.tm_yday || _lastcheck.tm_year != now.tm_year))
	{
		if (_iFnDate & 0x08 && now.tm_hour != _tm.tm_hour || _iFnDate & 0x04 && now.tm_mday != _tm.tm_mday || _iFnDate & 0x02 && now.tm_mon != _tm.tm_mon || _iFnDate & 0x01 && now.tm_year != _tm.tm_year)
		{
			CStringA ofn(_oFilename.c_str());
			_gen_filename(now, ofn);
			_instance->_tm = now;
			_instance->_filename = ofn;
			flush();
			close();
			open(ofn, std::ofstream::out | std::ofstream::app);
		}
		_lastcheck = now;
	}
}


const std::string& log::filename() const
{
	return _filename;
}

void log::prev_filename(CStringA& lfn) const
{
	tm now;
	time_t t = time(0);
	if (_iFnDate & 0x08)
		t -= 60 * 60;
	else if (_iFnDate & 0x04)
		t -= 24 * 60 * 60;
	localtime_s(&now, &t);
	if (!(_iFnDate & 0x08 || _iFnDate & 0x04))
	{
		if (_iFnDate & 0x02)
		{
			if (--now.tm_mon < 0);
			{
				now.tm_mon = 11;
				now.tm_year;
			}
		}
		else if (_iFnDate & 0x01)
			--now.tm_year;
	}
	
	lfn = _oFilename.c_str();
	_gen_filename(now, lfn);
}


const char* log::functionName() const
{
	if (!_levelFunction)
		return NULL;
	mthrid_vstr::const_iterator ithr = _functionName.find(GetThreadId(GetCurrentThread()));
	return ithr == _functionName.end() || !ithr->second.size() ? NULL : ithr->second.back();
}


bool log::colored() const
{
	return _bColored;
}


log_type log::logLevel() const
{
	return _logLevel;
}


void log::printDate()
{
	tm now;
	unsigned short millisec;
	if (_bMillisec)
	{
		_timeb stnow;
		_ftime_s(&stnow);
		localtime_s(&now, &stnow.time);
		millisec = stnow.millitm;
	}
	else
	{
		time_t t = time(0);
		localtime_s(&now, &t);
	}

	check_filename(now);

	if (_bColored)
		*this << char(0x1B) << "[0;38;5;33m";
	*this << std::setfill('0') << std::setw(2) << long((now.tm_year + 1900) % 100)
			<< std::setfill('0') << std::setw(2) << (now.tm_mon + 1)
			<< std::setfill('0') << std::setw(2) << now.tm_mday
		<< std::setfill('0') << std::setw(2) << now.tm_hour
			<< std::setfill('0') << std::setw(2) << now.tm_min
			<< std::setfill('0') << std::setw(2) << now.tm_sec;
	if (_bMillisec)
		*this << std::setfill('0') << std::setw(3) << millisec;
	if (_bColored)
		*this << char(0x1B) << "[m";
	*this << ":";
	_bPrintDate = false;
}


void log::_printType(log_type type)
{
	switch (type)
	{
	case logWTrace:
	case logNone:
		*this << ':';
		break;
	case logDebug:
		if (_bColored)
			*this << char(0x1B) << "[1;33m";
		*this << ":D:";
		if (_bColored)
			*this << char(0x1B) << "[m";
		break;
	case logInf:
		if (_bColored)
			*this << char(0x1B) << "[1;32m";
		*this << ":I:";
		if (_bColored)
			*this << char(0x1B) << "[m";
		break;
	case logWarning:
		if (_bColored)
			*this << char(0x1B) << "[1;31m";
		*this << ":W:";
		if (_bColored)
			*this << char(0x1B) << "[m";
		break;
	case logError:
		if (_bColored)
			*this << char(0x1B) << "[1;38;5;124m";
		*this << ":E:";
		if (_bColored)
			*this << char(0x1B) << "[m";
		break;
	default:
		break;
	}
}


void log::printType(log_type type)
{
	_printType(type);
	const char* fname;
	if (_levelFunction)
	{
		mthrid_vstr::const_iterator ithr = _instance->_functionName.find(GetThreadId(GetCurrentThread()));
		fname = ithr != _functionName.end() && ithr->second.size() ? ithr->second.back() : NULL;
	}
	else
		fname = _sfname;

	if (fname)
	{
		if (_bColored)
			*this << char(0x1B) << "[1;38;5;30m";
		*this << fname;
		if (_bColored)
			*this << char(0x1B) << "[m";
		*this << ": ";
		if (_bColored && (type == logWarning || type == logError))
			*this << char(0x1B) << (type == logWarning ? "[1;31m" : "[1;38;5;124m");
	}
}


void log::getThreadId()
{
	if (_bColored)
		*this << char(0x1B) << "[1;38;5;202m";
	*this << GetThreadId(GetCurrentThread());
	if (_bColored) 
		*this << char(0x1B) << "[m";
}


log& log::trace(log_type type, LPCSTR lpszFormat, ...)
{
	_cs.Lock();

	if (_logLevel < type)
	{
		_cs.Unlock();
		return *this;
	}

	if (_bPrintDate)
		printDate();
	getThreadId();
	printType(type);

	char buffer[1024 * 16];
	va_list argptr;
	va_start(argptr, lpszFormat);
	vsprintf_s(buffer, lpszFormat, argptr);
	va_end(argptr);
	*this << buffer;
	this->flush();

#ifdef DEBUG
	if (_logLevel == logWTrace)
	{
		if (100 < strlen(buffer))
			buffer[99] = '\0';
		AfxTrace(CString(buffer));
	}
#endif // DEBUG

	_cs.Unlock();
	return *this;
}




log& log::trace(log_type type, int nTabs, LPCSTR lpszFormat, ...)
{
	_cs.Lock();

	if (_logLevel < type)
	{
		_cs.Unlock();
		return *this;
	}

	const int col_size = 60;
	//         1         2         3         4         5         6         7         8
	//123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789
	//YYMMDDHHMISSMIL:ThreadID:T:functionname				-> _bMillisec == true 
	///               17         28
	//YYMMDDHHMISS:ThreadID:T:functionname					-> _bMillisec == false
	///            14         25

	const unsigned short tab_size = 0;
	unsigned short ndig = _bPrintDate ? _bMillisec ? 17 : 14 : 0;
	if (_bPrintDate)
		printDate();
	getThreadId();
	_printType(type);

	ndig += ceil(::log10f(GetThreadId(GetCurrentThread()))) + 3 + 2;

	std::string fname;
	if (_levelFunction)
	{
		mthrid_vstr::const_iterator ithr = _instance->_functionName.find(GetThreadId(GetCurrentThread()));
		fname = ithr != _functionName.end() && ithr->second.size() ? ithr->second.back() : NULL;
	}
	else
		fname = _sfname;
	if (col_size - ndig <= fname.size())
	{
		fname = fname.substr(0, col_size - ndig - 3);
		*this << fname << "...";
	}
	if (_bColored)
		*this << char(0x1B) << "[1;38;5;30m";
	*this << fname;
	if (_bColored)
		*this << char(0x1B) << "[m";
	*this << ": ";
	ndig += fname.size();
	for (unsigned short i = ndig; i < col_size + tab_size * nTabs; ++i)
		*this << ' ';
	if (_bColored && (type == logWarning || type == logError))
		*this << char(0x1B) << (type == logWarning ? "[1;31m" : "[1;38;5;124m");


	char buffer[1024 * 16];
	va_list argptr;
	va_start(argptr, lpszFormat);
	vsprintf_s(buffer, lpszFormat, argptr);
	va_end(argptr);
	*this << buffer;
	this->flush();

#ifdef DEBUG
	if (_logLevel == logWTrace)
	{
		if (100 < strlen(buffer))
			buffer[99] = '\0';
		AfxTrace(CString(buffer));
	}
#endif // DEBUG

	_cs.Unlock();
	return *this;
}



log& log::traceN(LPCSTR str, int n, LPCSTR str2)
{
	_cs.Lock();
	char* c = new char[n + 1];
	for (int i = 0; i < n; ++i)
		c[i] = '0' + i % 10;
	c[n] = '\0';
	*this << str << c << str2;
	delete[] c;
	_cs.Unlock();
	return *this;
}




log& log::trace(log_type type, const std::stringstream& ss)
{
	_cs.Lock();

	if (_logLevel < type)
	{
		_cs.Unlock();
		return *this;
	}

	if (_bPrintDate)
		printDate();
	printType(type);

	*this << ss.str();

#ifdef DEBUG
	if (_logLevel == logWTrace)
	{
		CString str(ss.str().c_str());
		str.Truncate(160);
		AfxTrace(str);
	}
#endif // DEBUG

	_cs.Unlock();
	return *this;
}


log& log::trace(log_type type, const std::string& str)
{
	_cs.Lock();

	if (_logLevel < type)
	{
		_cs.Unlock();
		return *this;
	}

	if (_bPrintDate)
		printDate();
	printType(type);

	*this << str;

#ifdef DEBUG
	if (_logLevel == logWTrace)
	{
		CString str(str.c_str());
		str.Truncate(160);
		AfxTrace(str);
	}
#endif // DEBUG

	_cs.Unlock();
	return *this;
}



}