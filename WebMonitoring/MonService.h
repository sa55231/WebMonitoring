#pragma once
#include "crow_all.h"
#include "ServiceBase.h" 
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <condition_variable>
#include <ppl.h>
#include "sqlite3.h"

struct site {
    int id = -1;
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
    site AddSite(const std::string& name, const std::string& url) const;
    bool UpdateSite(int id,const std::string& name, const std::string& url) const;
    void DeleteSite(int id) const;
    site GetSite(int id) const;
    void ScheduleSites();
    void ScheduleSite(const site& s);
private:
    sqlite3 *db;    
    std::mutex mtx;
    std::unordered_set<crow::websocket::connection*> connections;
    concurrency::cancellation_token_source cts;
    std::vector<concurrency::task<void>> tasks;
    std::mutex task_timeout_mutex;
    std::condition_variable task_timeout_condition;
    crow::SimpleApp app;
    std::string indexHtmlString;
    std::thread thr;
    BOOL m_fStopping;
};

