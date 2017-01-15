#include "stdafx.h"

#include "MonService.h"
#include "resource.h"
#include "picojson.h"
#include "utils.h"

#include <shlobj.h>    // for SHGetFolderPath

CMonService::CMonService(PWSTR pszServiceName,
    BOOL fCanStop, BOOL fCanShutdown, BOOL fCanPauseContinue) : CServiceBase(pszServiceName, fCanStop, fCanShutdown, fCanPauseContinue),db(nullptr)
{
    m_fStopping = FALSE;
    indexHtmlString = GetHtmlResource(IDR_HTML1);
    TCHAR szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA,NULL, 0, szPath)))
    {
        // Set lpstrInitialDir to the path that SHGetFolderPath obtains.         
        std::wstring path(szPath);
        path.append(L"\\webmon");
        CreateDirectory(path.c_str(),NULL);
        path.append(L"\\webmon.db");
        if (sqlite3_open(ConvertToString(path.c_str()).c_str(), &db))
        {
            const char* err = sqlite3_errmsg(db);
            sqlite3_close(db);
            auto wmsg = ConvertToWString(err);
            auto fullMsg = L"Cannot open database: " + wmsg;
            WriteEventLogEntry(fullMsg.c_str(),EVENTLOG_ERROR_TYPE);
            throw fullMsg;
        }
        else
        {
            auto fullMsg = L"Using database: " + path;
            WriteEventLogEntry(fullMsg.c_str(), EVENTLOG_INFORMATION_TYPE);
            sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS URLS(NAME TEXT PRIMARY KEY,URL TEXT)",NULL,NULL,NULL);
        }
        //szPath;
    }
}


CMonService::~CMonService()
{
    OnStop();
    if(db)
    {
        sqlite3_close(db);
    }    
}

void CMonService::OnStart(DWORD dwArgc, PWSTR * pszArgv)
{
    WriteEventLogEntry(L"CMonService in OnStart",
        EVENTLOG_INFORMATION_TYPE);

    // Queue the main service function for execution in a worker thread. 
    //CThreadPool::QueueUserWorkItem(&CMonService::ServiceWorkerThread, this);
    thr = std::thread([this]() {
        ServiceWorkerThread();
    });    
}

void CMonService::OnStop()
{
    WriteEventLogEntry(L"CMonService in OnStop",
        EVENTLOG_INFORMATION_TYPE);

    app.stop();
    thr.join();
}

std::vector<site> CMonService::GetAllSites() const
{
    std::vector<site> sites;
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,"SELECT NAME,URL FROM URLS ORDER BY NAME",-1,&stmt,NULL);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        site s;
        s.name = (const char*)sqlite3_column_text(stmt, 0);
        s.url = (const char*)sqlite3_column_text(stmt, 1);
        sites.push_back(s);
    }
    sqlite3_finalize(stmt);
    return sites;
}

void CMonService::AddSite(const std::string & name, const std::string & url) const
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "insert into URLS (NAME, URL) values (?1, ?2);", -1, &stmt, NULL);       

    sqlite3_bind_text(stmt, 1, name.c_str(), name.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, url.c_str(), url.length(), SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("ERROR inserting data: %s\n", sqlite3_errmsg(db));
        
    }

    sqlite3_finalize(stmt);
}

void CMonService::DeleteSite(const std::string & name) const
{
}

void CMonService::ServiceWorkerThread(void)
{
    // Log a service stop message to the Application log. 
    WriteEventLogEntry(L"CMonService1 in ServiceWorkerThread",
        EVENTLOG_INFORMATION_TYPE);

    CROW_ROUTE(app, "/")
        .methods("GET"_method)
        ([this]() {
        return indexHtmlString;
    });
    CROW_ROUTE(app, "/sites")
        .methods("GET"_method)
        ([this]() {
        auto sites = GetAllSites();
        crow::response resp;
        resp.code = 200;
        resp.set_header("Content-Type", "application/json;charset=utf8");
        picojson::array jarray;
        for (const auto& s : sites)
        {
            picojson::object obj;
            obj["name"] = picojson::value(s.name);
            obj["url"] = picojson::value(s.url);
            jarray.push_back(picojson::value(obj));
        }
        resp.body = picojson::value(jarray).serialize();
        return resp;
    });

    CROW_ROUTE(app, "/sites")
        .methods("POST"_method)
        ([this](const crow::request& req) {
        picojson::value val;
        picojson::parse(val,req.body);
        crow::response resp;
        if (val.get("name").is<std::string>() && val.get("url").is<std::string>())
        {
            AddSite(val.get("name").to_str(), val.get("url").to_str());
            resp.code = 201;
            resp.add_header("Location", "/sites/" + val.get("name").to_str());
        }
        else
        {
            resp.code = 400;
        }        
        return resp;
    });

    CROW_ROUTE(app, "/sites/<string>")
        .methods("DELETE"_method)
        ([this](const crow::request& req, const std::string& name) {
        
        DeleteSite(name);
        crow::response resp;
        resp.code = 200;
        return resp;
    });

    app.bindaddr("0.0.0.0");
    app.port(18080).multithreaded().run();

    WriteEventLogEntry(L"CMonService stopping after app.stop() was called",
        EVENTLOG_INFORMATION_TYPE);
    m_fStopping = TRUE;
   
}
