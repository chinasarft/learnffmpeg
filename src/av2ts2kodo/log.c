#include "log.h"

int nLogLevel = LOG_LEVEL_INFO;
LogFunc logFunc = NULL;

void SetLogLevelToTrace()
{
        nLogLevel = LOG_LEVEL_TRACE;
}

void SetLogLevelToDebug()
{
        nLogLevel = LOG_LEVEL_DEBUG;
}

void SetLogLevelToInfo()
{
        nLogLevel = LOG_LEVEL_INFO;
}

void SetLogLevelToWarn()
{
        nLogLevel = LOG_LEVEL_WARN;
}

void SetLogLevelToError()
{
        nLogLevel = LOG_LEVEL_ERROR;
}

void SetLogCallback(LogFunc f)
{
	logFunc = f;
}


void Log(int nLevel, char * pFmt, ...)
{
	if (nLevel >= nLogLevel){
	        va_list ap;
                va_start(ap, pFmt);
                if (logFunc == NULL) {
                        vprintf(pFmt, ap);
		} else {
                        char logStr[257] = {0};
                        vsnprintf(logStr, sizeof(logStr), pFmt, ap);
                        logFunc(logStr);
		}
                va_end(ap);
	}
}
