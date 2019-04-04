//----------------------------------------------------------------------------
//   The confidential and proprietary information contained in this file may
//   only be used by a person authorised under and to the extent permitted
//   by a subsisting licensing agreement from ARM Limited or its affiliates.
//
//          (C) COPYRIGHT 2018 ARM Limited or its affiliates
//              ALL RIGHTS RESERVED
//
//   This entire notice must be reproduced on all copies of this file
//   and copies of this file may only be made by a person if such person is
//   permitted to do so under the terms of a subsisting license agreement
//   from ARM Limited or its affiliates.
//----------------------------------------------------------------------------

#include <version.h>

#include <ATL/ATLTypes.h>
#include <ATL/ATLLogger.h>
#include <ATL/ATLConfig.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include <event2/keyvalq_struct.h>

#define LOG_CONTEXT "HTTPEngine"

#include <HTTPServer/EventEngine.h>

#include <fstream>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define __file_stat _stat
#define __stat_t _stat
#define __fileno _fileno
FILE* __fopen(const char *filename, const char *mode) {
    FILE* stream;
    errno_t err = fopen_s(&stream, filename, mode);
    return (err == 0 ? stream : NULL);
}
#else // _WIN32
#define __file_stat lstat
#define __stat_t stat
#define __fileno fileno
#define __fopen fopen
#include <signal.h>
#endif

namespace act {

    static const struct table_entry {
        const char *extension;
        const char *content_type;
    } content_type_table[] = {
        { "js",     "text/javascript"},
        { "css",    "text/css" },
        { "xml",    "application/xml" },
        { "json",   "application/json" },
        { "html",   "text/html" },
        { "png",    "image/png" },
        { "xhtml",  "application/xhtml+xml" },
        { "gif",    "image/gif" },
        { "c",      "text/x-c" },
        { "h",      "text/x-h" },
        { "jpg",    "image/jpeg" },
        { "jpeg",   "image/jpeg" },
        { "txt",    "text/plain" },
        { "xsl",    "application/xml" },
        { "woff",   "application/font-woff" },
        { NULL,   NULL }
    };

    static CEventEngine *httpServer = NULL;

    const UInt8 CEventEngine::GET_ANY     = 0x00;
    const UInt8 CEventEngine::GET_ROOT    = 0x01;
    const UInt8 CEventEngine::GET_VER     = 0x02;
    const UInt8 CEventEngine::ACC_MGR     = 0x03;
    const UInt8 CEventEngine::GET_ICO     = 0x04;
    const UInt8 CEventEngine::UPLD_XML    = 0x05;
    const UInt8 CEventEngine::UPLD_API    = 0x06;
    const UInt8 CEventEngine::GET_LOAD    = 0x07;
    const UInt8 CEventEngine::GET_RDIR    = 0x08;
    const UInt8 CEventEngine::GET_INFO    = 0x09;

    const UInt8 CEventEngine::HW_READ     = 0x80;
    const UInt8 CEventEngine::HW_POLL     = 0x81;
    const UInt8 CEventEngine::HW_WRITE    = 0x82;
    const UInt8 CEventEngine::FW_GET      = 0x83;
    const UInt8 CEventEngine::FW_SET      = 0x84;
    const UInt8 CEventEngine::CA_LOAD     = 0x85;
    const UInt8 CEventEngine::CA_SAVE     = 0x86;
    const UInt8 CEventEngine::FW_POLL     = 0x87;
    const UInt8 CEventEngine::FI_DNLOAD   = 0x88;
    const UInt8 CEventEngine::FI_UPLOAD   = 0x89;

    static void eventCallback(evutil_socket_t, short, void *) {
    }

    CEventEngine::~CEventEngine() {
        Terminate();
    }

    const std::string CEventEngine::GetObjectStaticName() {
        return LOG_CONTEXT;
    }

    const std::string CEventEngine::GetName() {
        return LOG_CONTEXT;
    }

    /* Try to guess a good content-type for 'path' */
    const char *CEventEngine::guessContentType(const char *path) {
        const char *last_period, *extension;
        const struct table_entry *ent;
        last_period = strrchr(path, '.');
        if (!last_period || strchr(last_period, '/'))
            goto not_found; /* no extension */
        extension = last_period + 1;
        for (ent = &content_type_table[0]; ent->extension; ++ent) {
            if (!evutil_ascii_strcasecmp(ent->extension, extension))
                return ent->content_type;
        }
    not_found:
        return "application/unknown";
    }

    CATLError CEventEngine::addFileToSend( struct evbuffer *evb, std::string full_path ) {
        CATLError result;
        struct __stat_t st;
        __file_stat(full_path.c_str(), &st);
        if(S_IFREG == (st.st_mode & S_IFMT)) { // if it is regular file
            FILE * f_stream = __fopen(full_path.c_str(), "rb");
            if ( f_stream != NULL ) {
                int fds = __fileno(f_stream);
                fseek( f_stream, 0L, SEEK_END );
                UInt32 sz = ftell( f_stream );
                fseek( f_stream, 0L, SEEK_SET );
                evbuffer_add_file( evb, fds, 0, sz );
            } else {
                result = EATLErrorFatal;
            }
        } else {
            result = EATLErrorFatal;
        }
        return result;
    }

    void CEventEngine::checkResult(const CATLError& res, struct evhttp_request *req) {
        if (res == EATLErrorOk) {
            evhttp_send_reply(req, 200, "", NULL);
        } else if (res == EATLErrorNotChanged) {
            evhttp_send_reply(req, 202, "Not changed", NULL);
        } else if (res == EATLErrorFatal) { // stop server
            evhttp_send_error(req, 500, "Fatal server error");
            httpServer->Terminate();
        } else {
            evhttp_send_error(req, 500, "Server error");
        }
    }

    CATLError CEventEngine::getPayload(evhttp_request *req, std::string& payload) {
        evbuffer * buf = evhttp_request_get_input_buffer(req);
        basesize length = evbuffer_get_length(buf);
        payload.resize(length);
        if (evbuffer_remove(buf, (char*)payload.data(), payload.size()) != (int)length) {
            evhttp_send_error(req, 507, "Payload is too big");
            return EATLErrorNoEnoughBuffer;
        } else {
            return EATLErrorOk;
        }
    }

    CATLError CEventEngine::getPayload(struct evhttp_request *req, std::vector<UInt8>& payload) {
        evbuffer * buf = evhttp_request_get_input_buffer(req);
        basesize length = evbuffer_get_length(buf);
        payload.resize(length);
        if (evbuffer_remove(buf, payload.data(), payload.size()) != (int)length) {
            evhttp_send_error(req, 507, "Payload is too big");
            return EATLErrorNoEnoughBuffer;
        } else {
            return EATLErrorOk;
        }
    }

    void CEventEngine::baseEventHandler(struct evhttp_request *req, void *arg) {

        if (httpServer == NULL || httpServer->cm == NULL) {
            evhttp_send_error(req, 500, "HTTP server not initialized");
            return;
        }

        evhttp_cmd_type cmdtype = evhttp_request_get_command(req);
        switch (cmdtype) {
            case EVHTTP_REQ_GET:
            case EVHTTP_REQ_POST:
            case EVHTTP_REQ_DELETE: break;
            default: evhttp_send_error(req, 405, "Unsupported HTTP method"); return;
        }

        std::string uri = evhttp_request_get_uri(req);
        struct evbuffer *evb = evhttp_request_get_output_buffer(req);

        SysLog()->LogDebug(LOG_CONTEXT, "http request %d %s", cmdtype, uri.c_str());

        UInt8 route = (*((UInt8*)arg)); // type of request

        if (cmdtype == EVHTTP_REQ_GET) {

            evhttp_request_get_evhttp_uri(req);
            UInt16 status = 200;

            switch(route) {
            case CEventEngine::GET_RDIR: // redirect to /
                status = 301;
                evhttp_add_header(evhttp_request_get_output_headers(req), "Location", "/");
            case CEventEngine::GET_ROOT:
                if (httpServer->cm->isXMLLoaded()) {
                    if (addFileToSend( evb, httpServer->documentRoot + FILE_SEPARATOR + "ApicalControl.xml").IsOk()) {
                        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/xml");
                        evhttp_send_reply(req, status, "", evb);
                    } else {
                        SysLog()->LogWarning(LOG_CONTEXT, "requested file `ApicalControl.xml` was not found" );
                        evhttp_send_error(req, 404, "Document ApicalControl.xml not found");
                    }
                } else {
                    if (addFileToSend( evb, httpServer->documentRoot + FILE_SEPARATOR + "load.html").IsOk()) {
                        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/html");
                        evhttp_send_reply(req, status, "", evb);
                    } else {
                        SysLog()->LogWarning(LOG_CONTEXT, "requested file `load.html` was not found" );
                        evhttp_send_error(req, 404, "Document load.html not found");
                    }
                }
                return;
            case CEventEngine::GET_LOAD: // override reload xml
                if (addFileToSend( evb, httpServer->documentRoot + FILE_SEPARATOR + "load.html").IsOk()) {
                    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/html");
                    evhttp_send_reply(req, 200, "", evb);
                } else {
                    SysLog()->LogWarning(LOG_CONTEXT, "requested file `load.html` was not found" );
                    evhttp_send_error(req, 404, "Document load.html not found");
                }
                return;
            case CEventEngine::GET_VER:
                evbuffer_add_printf(evb, "{\"version\":\"%s\"}", __MODULE_VERSION);
                evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
                evhttp_send_reply(req, 200, "", evb);
                return;
            case CEventEngine::GET_INFO:
                evbuffer_add_printf(evb, "%s", "{");
                evbuffer_add_printf(evb, "\"%s\":\"%s\",", "version", __MODULE_VERSION " of " __SOURCE_DATE_TIME);
                evbuffer_add_printf(evb, "\"%s\":\"%s\",", "built", __DATE__ " at " __TIME__);
                evbuffer_add_printf(evb, "\"%s\":\"%s\",", "platform", "");
                evbuffer_add_printf(evb, "\"%s\":\"%s\",", "user command", SysConfig()->GetUserCommandLine().c_str());
                evbuffer_add_printf(evb, "\"%s\":\"%s\",", "full command", SysConfig()->GetFullCommandLine().c_str());
                evbuffer_add_printf(evb, "\"%s\":\"%s\",", "hw access type", httpServer->cm->hwAccessType().c_str());
                evbuffer_add_printf(evb, "\"%s\":\"%s\"",  "fw access type", httpServer->cm->fwAccessType().c_str());
                evbuffer_add_printf(evb, "%s", "}");
                evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
                evhttp_send_reply(req, 200, "", evb);
                return;
            case CEventEngine::ACC_MGR:
                evbuffer_add_printf(evb, "{\"configured\":%s}", httpServer->cm->isAcessManagerConfigured() ? "true" : "false");
                evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
                evhttp_send_reply(req, 200, "", evb);
                return;
            case CEventEngine::GET_ICO:
                if ( addFileToSend( evb, httpServer->documentRoot + FILE_SEPARATOR + "img" + FILE_SEPARATOR + "acs.png").IsOk() ) {
                    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "image/png");
                    evhttp_send_reply(req, 200, "", evb);
                } else {
                    SysLog()->LogDebug(LOG_CONTEXT, "requested file `img/acs.png` was not found" );
                    evhttp_send_error(req, 404, "Document img/acs.png not found");
                }
                return;
            case CEventEngine::GET_ANY: {
                struct evhttp_uri *decoded = evhttp_uri_parse( uri.c_str() );  /* We need to decode it, to see what path the user really wanted. */
                const char * path = evhttp_uri_get_path( decoded );
                const char *dec_path = evhttp_uridecode( path, 0, NULL);
                std::string decoded_path( dec_path );
                const char *type = guessContentType( dec_path );
                std::string full_path = httpServer->documentRoot + decoded_path;
                if ( addFileToSend( evb, full_path).IsOk() ) {
                    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", type);
                    evhttp_send_reply(req, 200, "", evb);
                } else {
                    SysLog()->LogDebug(LOG_CONTEXT, ("requested file `%s` was not found"), decoded_path.c_str() );
                    evhttp_send_error(req, 404, "Document not found");
                }
                return;
            }
            default:
                evhttp_send_error(req, 404, "Not found");
                return;
            }

        } else if (cmdtype == EVHTTP_REQ_POST) {

            std::string text;
            std::vector<UInt8> data;

            CATLError res;
            switch (route) {
            case CEventEngine::ACC_MGR:
                getPayload(req, text);
                res = httpServer->cm->configAccessManager(text);
                checkResult(res, req);
                return;
            case CEventEngine::UPLD_XML:
                getPayload(req, text);
                res = httpServer->cm->saveXml(text);
                checkResult(res, req);
                return;
            case CEventEngine::UPLD_API:
                getPayload(req, text);
                res = httpServer->cm->saveApi(text);
                checkResult(res, req);
                return;
            case HW_READ: case HW_POLL: case HW_WRITE: case FW_GET: case FW_SET: case FW_POLL: case CA_LOAD:  case CA_SAVE:
                if (httpServer->cm->isAcessManagerConfigured()) {
                    getPayload(req, text);
                    std::pair < std::string, UInt16 > reply = httpServer->cm->proxy(text, route);
                    if (reply.second == 200) {
                        evbuffer_add_printf(evb, "%s", reply.first.c_str() );
                        evhttp_add_header(evhttp_request_get_output_headers(req),  "Content-Type", "application/json");
                        evhttp_send_reply(req, 200, NULL, evb);
                    } else {
                        evhttp_send_error(req, reply.second, reply.first.c_str());
                    }
                    return;
                } else {
                    evhttp_send_error(req, 503, "Access Manager not configured");
                    return;
                }
            case FI_UPLOAD: case FI_DNLOAD: {
                getPayload(req, data);
                if (httpServer->cm->isAcessManagerConfigured()) {
                    evkeyvalq * headers = evhttp_request_get_input_headers(req);
                    evkeyval * cur = headers->tqh_first;
                    std::string section, command, key;
                    while (cur) {
                        key = std::string(cur->key);
                        if (key == "act-file-section") {
                            section.assign(cur->value);
                        } else if (key == "act-file-command") {
                            command.assign(cur->value);
                        }
                        cur = cur->next.tqe_next;
                    }
                    if (section.size() > 0 && command.size() > 0) {
                        UInt8 sec = stou8(section);
                        UInt8 cmd = stou8(command);
                        std::pair < std::string, UInt16 > reply;
                        if (route == FI_UPLOAD) { // upload file
                            reply = httpServer->cm->uploadFile(data, sec, cmd);
                        } else { // download file
                            reply = httpServer->cm->downloadFile(sec, cmd);
                        }
                        if (reply.second == 200) {
                            evbuffer_add_printf(evb, "%s", reply.first.c_str() );
                            evhttp_add_header(evhttp_request_get_output_headers(req),  "Content-Type", "application/json");
                            evhttp_send_reply(req, 200, "", evb);
                        } else {
                            evhttp_send_error(req, reply.second, reply.first.c_str());
                        }
                    } else {
                        evhttp_send_error(req, 400, "Bad section and command for file transfer");
                    }
                } else {
                    evhttp_send_error(req, 503, "Access Manager not configured");
                    return;
                }
                return;
            }
            default:
                evhttp_send_error(req, 404, "Unknown request");
                return;
            }
        } else if (cmdtype == EVHTTP_REQ_DELETE) {
            CATLError res;
            switch(route) {
            case CEventEngine::UPLD_API: // deletion of api file requested
                res = httpServer->cm->dropApi();
                checkResult(res, req);
                return;
            default:
                evhttp_send_error(req, 404, "Not found");
                return;
            }
        }
    }

    CATLError CEventEngine::Initialize(const flags32&) {
        if (state >= EHTTPEngineInitialized) {
            SysLog()->LogError(LOG_CONTEXT, "attempt to initialize http engine which is already initialized");
            return EATLErrorUnsupported;
        }
        CATLError result;
        state = EHTTPEngineNotInitialized;
#ifdef _WIN32
            WSADATA WSAData;
            WSAStartup(0x101, &WSAData);
            evthread_use_windows_threads();
#else
        if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
            return EATLErrorFatal;
        }
        if (0 == evthread_use_pthreads()) {
            SysLog()->LogDebug(LOG_CONTEXT, "Multithreading support enabled");
        }
#endif
        eventBase = event_base_new();
        if (eventBase == NULL) {
            SysLog()->LogError(LOG_CONTEXT, "could not create instance for http engine: exiting...");
            result = EATLErrorNoMemory;
        } else {
            eventHttp = evhttp_new(eventBase);
            if (eventHttp == NULL) {
                SysLog()->LogError(LOG_CONTEXT, "could not create evhttp: exiting..." );
                result = EATLErrorNoMemory;
            } else {
                UInt16 port = SysConfig()->GetValueAsU16("http-port");
                documentRoot = SysConfig()->GetValue("http-root");
                std::string ip_addr = SysConfig()->GetValue("http-address");

                evhttp_set_gencb(eventHttp,                    CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::GET_ANY);

                evhttp_set_cb(eventHttp, "/",                  CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::GET_ROOT);
                evhttp_set_cb(eventHttp, "/load.html",         CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::GET_RDIR);
                evhttp_set_cb(eventHttp, "/ApicalControl.xml", CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::GET_RDIR);
                evhttp_set_cb(eventHttp, "/reload",            CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::GET_LOAD);
                evhttp_set_cb(eventHttp, "/version",           CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::GET_VER);
                evhttp_set_cb(eventHttp, "/am",                CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::ACC_MGR);
                evhttp_set_cb(eventHttp, "/favicon.ico",       CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::GET_ICO);
                evhttp_set_cb(eventHttp, "/about",             CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::GET_INFO);

                evhttp_set_cb(eventHttp, "/xml",               CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::UPLD_XML);
                evhttp_set_cb(eventHttp, "/api",               CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::UPLD_API);
                evhttp_set_cb(eventHttp, "/hw/read",           CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::HW_READ);
                evhttp_set_cb(eventHttp, "/hw/poll",           CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::HW_POLL);
                evhttp_set_cb(eventHttp, "/hw/write",          CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::HW_WRITE);
                evhttp_set_cb(eventHttp, "/fw/get",            CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::FW_GET);
                evhttp_set_cb(eventHttp, "/fw/set",            CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::FW_SET);
                evhttp_set_cb(eventHttp, "/calibration/load",  CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::CA_LOAD);
                evhttp_set_cb(eventHttp, "/calibration/save",  CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::CA_SAVE);
                evhttp_set_cb(eventHttp, "/fw/poll",           CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::FW_POLL);
                evhttp_set_cb(eventHttp, "/file/download",     CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::FI_DNLOAD);
                evhttp_set_cb(eventHttp, "/file/upload",       CEventEngine::baseEventHandler, (UInt8*)&CEventEngine::FI_UPLOAD);

                struct evhttp_bound_socket *handle = evhttp_bind_socket_with_handle(eventHttp, ip_addr.c_str(), port);
                if (handle == NULL) {
                    if (ip_addr == "0.0.0.0") {
                        SysLog()->LogError(LOG_CONTEXT, "can't bind to port %u", port);
                    } else {
                        SysLog()->LogError(LOG_CONTEXT, "can't bind to address %s:%u", ip_addr.c_str(), port);
                    }
                    result = EATLErrorFatal;
                } else {
                    state = EHTTPEngineInitialized;
                }
            }
        }
        return result;
    }

    CATLError CEventEngine::Open() {
        CATLError result;
        if (eventBase != NULL) {
            httpServer = this;
            state = EHTTPEngineRunning;
            CATLError result;
            if (eventBase != NULL) {
                 state = EHTTPEngineRunning;
                 int res = event_base_dispatch(eventBase); // loop until server is stopped
                 if (res != 0) {
                     result = EATLErrorFatal;
                     SysLog()->LogError(LOG_CONTEXT, "failed to run http server");
                 } else {
                     SysLog()->LogDebug(LOG_CONTEXT, "the http engine is running");
                 }
            } else {
                SysLog()->LogError(LOG_CONTEXT, "failed to run thread: http engine instance is not initialized");
            }
            OnServerFinish.Invoke(this);
        } else {
            SysLog()->LogError(LOG_CONTEXT, "failed to start http engine");
            result = EATLErrorFatal;
        }
        return result;
    }

    CATLError CEventEngine::Close() {
        CATLError result;
        if (eventBase != NULL) {
            atl::Int32 res = event_base_loopbreak(eventBase);
            if (res == 0) {
                  // to guarantee loop break we have to generate a one new event.
                  // after we activate it the main loop will be terminated
                  struct event *ev1 = event_new(eventBase, -1, EV_ET, eventCallback, (char*)"Dummy event");
                  event_add(ev1, NULL);
                  event_active(ev1, 0, 0);
                  event_free(ev1);
                  state = EHTTPEngineStopped;
            } else {
                SysLog()->LogError(LOG_CONTEXT, "failed to stop http server loop");
            }
        } else {
            result = EATLErrorNotInitialized;
        }
        return result;
    }

    EHTTPEngineStates CEventEngine::GetState() {
        return state;
    }

    CATLError CEventEngine::Terminate() {
        CATLError result;
        if (eventBase != NULL) {
             Close();
             event_base_free(eventBase);
             eventBase = NULL;
             state = EHTTPEngineNotInitialized;
        }
        return result;
    }

    void CEventEngine::ServerFinish(void*) {
        SysLog()->LogInfo(LOG_CONTEXT, "http engine stops");
    }

}
