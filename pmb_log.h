#pragma once

#include <fstream>

namespace pmb
{

enum log_type : char
{
	logError = '\x01',
	logWarning,
	logInf,
	logDebug,
	logNone,
	logWTrace
};


class log : public std::ofstream
{
	typedef std::vector<const char*> vstr;
	typedef std::map<DWORD, vstr> mthrid_vstr;

public:
	static log* instance(log_type logLevel);
	static log* instance(log_type logLevel, const char* filename, bool bColored = false, bool bLevelFunction = false);
	static log* instance(bool bprintDate = true);
	static log* beginFunction(log_type type, const char* functionName, bool bprintDate = true);
	static log* endFunction(log_type type, const char* functionName = NULL, bool bprintDate = true);
	static log* endFunction();

	static void destroy();

	static int _gen_filename(const tm& now, CStringA& ofn);

private:
	static log* _instance;

	static CCriticalSection _cs;

private:
	log();
	log(const char* filename, bool bColored, bool bLevelFunction);

	void check_filename(const tm& now);

public:
	~log();

	log& trace(log_type type, LPCSTR lpszFormat, ...);
	log& trace(log_type, int nTab, LPCSTR lpszFormat, ...);
	log& traceN(LPCSTR str, int n, LPCSTR str2);
//	log& trace(log_type type, const char* sFormat, ...);
	log& trace(log_type type, const std::stringstream& ss);
	log& trace(log_type type, const std::string& str);

	const std::string& filename() const;
	void prev_filename(CStringA& fn) const;
	bool colored() const;
	const char* functionName() const;
	log_type logLevel() const;

protected:
	void getThreadId();
	void printDate();
	void printType(log_type type);
	void _printType(log_type type);

protected:
	int _iFnDate;
	bool _bMillisec;
	bool _bColored;
	bool _bPrintDate;
	log_type _logLevel;

	bool _levelFunction;
	const char* _sfname;
	mthrid_vstr _functionName;

private:
	std::string _filename;
	std::string _oFilename;
	tm _tm;
	tm _lastcheck;
};




}