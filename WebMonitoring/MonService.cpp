#include "stdafx.h"

#include "MonService.h"
#include "resource.h"
#include "picojson.h"
#include "utils.h"
#include "WinHttpClient.h"
#include <ppltasks.h>
#include <shlobj.h>    // for SHGetFolderPath

using namespace std;

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
            sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS URLS(ID INTEGER PRIMARY KEY AUTOINCREMENT,NAME TEXT,URL TEXT)",NULL,NULL,NULL);
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
    cts.cancel();
    task_timeout_condition.notify_all();
    app.stop();
    thr.join();
    auto joinTask = concurrency::when_all(begin(tasks),end(tasks));
    joinTask.wait();
}

std::vector<site> CMonService::GetAllSites() const
{
    std::vector<site> sites;
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,"SELECT ID,NAME,URL FROM URLS ORDER BY ID",-1,&stmt,NULL);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        site s;
        s.id = sqlite3_column_int(stmt, 0);
        s.name = (const char*)sqlite3_column_text(stmt, 1);
        s.url = (const char*)sqlite3_column_text(stmt, 2);
        sites.push_back(s);
    }
    sqlite3_finalize(stmt);
    return sites;
}

site CMonService::AddSite(const std::string & name, const std::string & url) const
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "insert into URLS (NAME, URL) values (?1, ?2);", -1, &stmt, NULL);       

    sqlite3_bind_text(stmt, 1, name.c_str(), name.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, url.c_str(), url.length(), SQLITE_TRANSIENT);
    site s;
    s.name = name;
    s.url = url;
    s.id = -1;
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) 
    {
        printf("ERROR inserting data: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return s;    
    }
    sqlite3_finalize(stmt);
    sqlite3_stmt *stmt1;
    sqlite3_prepare_v2(db, "SELECT last_insert_rowid()", -1, &stmt1, NULL);
    rc = sqlite3_step(stmt1);
    if (rc != SQLITE_ROW) 
    {
        printf("ERROR inserting data: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt1);
        return s;
    }
    else 
    {
        s.id = sqlite3_column_int(stmt1, 0);
    }
    sqlite3_finalize(stmt1);
    return s;
}

bool CMonService::UpdateSite(int id, const std::string & name, const std::string & url) const
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE URLS SET NAME=?1 , URL=?2 WHERE ID=?3;", -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, name.c_str(), name.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, url.c_str(), url.length(), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, id);
    site s;
    s.name = name;
    s.url = url;
    s.id = -1;
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        printf("ERROR updating data: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

void CMonService::DeleteSite(int id) const
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "DELETE FROM URLS WHERE ID = ?1;", -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);
}

site CMonService::GetSite(int id) const
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT ID,NAME,URL FROM URLS WHERE ID=?1", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        site s;
        s.id = sqlite3_column_int(stmt, 0);
        s.name = (const char*)sqlite3_column_text(stmt, 1);
        s.url = (const char*)sqlite3_column_text(stmt, 2);
        sqlite3_finalize(stmt);
        return s;
    }
    sqlite3_finalize(stmt);
    return site();
}

void CMonService::ScheduleSites()
{
    auto sites = GetAllSites();    
    std::for_each(begin(sites), end(sites), [this](const site& s) {
        ScheduleSite(s);
    });
}

void CMonService::ScheduleSite(const site & s)
{
    auto token = cts.get_token();
    auto task = concurrency::create_task([this, s]() {

        auto sleep_duration = chrono::seconds(60);
        cout << "Checking " << s.name << " at url: " << s.url << endl;
        WinHttpClient client(ConvertToWString(s.url.c_str()));
        client.SetUserAgent(L"Web Monitoring Service/0.1");
        client.SetRequireValidSslCertificates(false);

        while (!cts.get_token().is_canceled())
        {
            auto start = std::chrono::system_clock::now();
            picojson::object resp;
            if (client.SendHttpRequest(L"GET", false))
            {
                auto status = client.GetResponseStatusCode();
                auto end = std::chrono::system_clock::now();
                auto duration = end - start;
                std::chrono::milliseconds md = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
                resp["status"] = picojson::value((int64_t)std::stoi(status));
                resp["duration"] = picojson::value(md.count());
                wcout << status << endl;
            }
            else
            {
                resp["status"] = picojson::value((int64_t)500);
            }
            resp["site_id"] = picojson::value((int64_t)s.id);
            resp["name"] = picojson::value(s.name);
            {
                std::lock_guard<std::mutex> _(mtx);
                for (auto& con : connections)
                {
                    con->send_text(picojson::value(resp).serialize());
                }
            }
            std::unique_lock<std::mutex> lock(task_timeout_mutex);
            task_timeout_condition.wait_for(lock, sleep_duration);
            //concurrency::wait(sleep_duration.count() * 1000);
            //std::this_thread::sleep_for(sleep_duration);
        }

    }, token);
    
    tasks.emplace_back(task);
}

void CMonService::ServiceWorkerThread(void)
{
    // Log a service stop message to the Application log. 
    WriteEventLogEntry(L"CMonService1 in ServiceWorkerThread",EVENTLOG_INFORMATION_TYPE);

    CROW_ROUTE(app, "/ws")
        .websocket()
        .onopen([&](crow::websocket::connection& conn) {
        CROW_LOG_INFO << "new websocket connection";
        std::lock_guard<std::mutex> _(mtx);
        connections.insert(&conn);
    }).onclose([&](crow::websocket::connection& conn, const std::string& reason) {
        CROW_LOG_INFO << "websocket connection closed: " << reason;
        std::lock_guard<std::mutex> _(mtx);
        connections.erase(&conn);
    });

    CROW_ROUTE(app, "/")
        .methods("GET"_method)
        ([this]() {
        return indexHtmlString;
    });
    CROW_ROUTE(app, "/sites")
        .methods("GET"_method, "POST"_method)
        ([this](const crow::request& req) {        
        crow::response resp;
        if (req.method == crow::HTTPMethod::Get)
        {
            auto sites = GetAllSites();
            resp.code = 200;
            resp.set_header("Content-Type", "application/json;charset=utf8");
            picojson::array jarray;
            for (const auto& s : sites)
            {
                picojson::object obj;
                obj["id"] = picojson::value((int64_t)s.id);
                obj["name"] = picojson::value(s.name);
                obj["url"] = picojson::value(s.url);
                jarray.push_back(picojson::value(obj));
            }
            resp.body = picojson::value(jarray).serialize();
        }
        else if (req.method == crow::HTTPMethod::Post)
        {
            picojson::value val;
            picojson::parse(val, req.body);
            
            if (val.get("name").is<std::string>() && val.get("url").is<std::string>())
            {
                site s = AddSite(val.get("name").to_str(), val.get("url").to_str());
                if(s.id >= 0)
                {
                    resp.code = 201;
                    resp.add_header("Location", "/sites/" + std::to_string(s.id));
                    ScheduleSite(s);
                }
                else
                {
                    resp.code = 400;
                }
            }
            else
            {
                resp.code = 400;
            }
        }
        return resp;
    });

    CROW_ROUTE(app, "/sites/<int>")
        .methods("DELETE"_method,"PUT"_method,"GET"_method)
        ([this](const crow::request& req, int id) {
        crow::response resp;
        if (req.method == crow::HTTPMethod::Put)
        {
            picojson::value val;
            picojson::parse(val, req.body);

            if (val.get("name").is<std::string>() && val.get("url").is<std::string>())
            {                
                if (UpdateSite(id, val.get("name").to_str(), val.get("url").to_str()))
                {
                    resp.code = 200;
                    site s;
                    s.id = id;
                    s.name = val.get("name").to_str();
                    s.url = val.get("url").to_str();
                    ScheduleSite(s);
                }
                else
                {
                    resp.code = 404;
                }
            }
            else
            {
                resp.code = 400;
            }
            
        }
        else if (req.method == crow::HTTPMethod::Delete)
        {
            DeleteSite(id);
            resp.code = 200;
        }        
        else if (req.method == crow::HTTPMethod::Get)
        {
            site s = GetSite(id);
            if (s.id >= 0)
            {
                picojson::object obj;
                obj["id"] = picojson::value((int64_t)s.id);
                obj["name"] = picojson::value(s.name);
                obj["url"] = picojson::value(s.url);
                resp.set_header("Content-Type", "application/json;charset=utf8");
                resp.code = 200 ;
                resp.body = picojson::value(obj).serialize();                
            }
            else
            {
                resp.code = 404;
            }

        }
        return resp;
    });

    ScheduleSites();

    app.bindaddr("0.0.0.0");
    app.port(18080).multithreaded().run();

    WriteEventLogEntry(L"CMonService stopping after app.stop() was called",
        EVENTLOG_INFORMATION_TYPE);
    m_fStopping = TRUE;
   
}
