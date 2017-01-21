#pragma once
#pragma warning(push)
#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4800)
#include "crow_all.h"
#pragma warning(pop)


#include "ServiceBase.h" 
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <condition_variable>
#include <concurrent_unordered_map.h>
#include <concurrent_vector.h>
#include <ppl.h>
#include "sqlite3.h"

struct site {
    int id = -1;
    std::string name;
    std::string url;
};
struct response_data {    
    std::string name;
    std::string url;
    int64_t duration = -1;
    int64_t request_time = -1;
    int status;    
    int site_id;
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
    void OpenDatabase();
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
    void AddHistoricalData(const response_data& resp);
    response_data GetLatestHistoricalData(int site_id);
    std::vector<response_data> GetHistoricalData(int site_id);
    void DeleteOldHistoricalData(int site_id);
private:
    sqlite3 *db;    
    std::mutex mtx;
    std::unordered_set<crow::websocket::connection*> connections;        
    concurrency::concurrent_unordered_map<int,concurrency::cancellation_token_source> cancellations_source_tasks;
    concurrency::concurrent_vector<concurrency::task<void>> tasks;
    std::mutex task_timeout_mutex;
    std::condition_variable task_timeout_condition;
    crow::SimpleApp app;
    std::string indexHtmlString;
    std::string chartJsString;
    std::thread thr;
    BOOL m_fStopping;
};

