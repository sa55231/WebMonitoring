#pragma once
#include "crow_all.h"
#include "ServiceBase.h" 
#include <string>
#include <thread>
#include <vector>
#include "sqlite3.h"

struct site {
    std::string name;
    std::string url;
};

class CMonService : public CServiceBase
{
public:
    CMonService(PWSTR pszServiceName,
        BOOL fCanStop = TRUE,
        BOOL fCanShutdown = TRUE,
        BOOL fCanPauseContinue = FALSE);
    virtual ~CMonService();
    void ServiceWorkerThread(void);
protected:
    virtual void OnStart(DWORD dwArgc, PWSTR *pszArgv);
    virtual void OnStop();

private:    
    std::vector<site> GetAllSites() const;
    void AddSite(const std::string& name, const std::string& url) const;
    void DeleteSite(const std::string& name) const;
private:
    sqlite3 *db;    
    crow::SimpleApp app;
    std::string indexHtmlString;
    std::thread thr;
    BOOL m_fStopping;
};

