// http_server.cpp  (see http_server.h)
#include "http_server.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET sock_t;
    #define CLOSE_SOCKET closesocket
    static bool sock_ok(sock_t s){ return s != INVALID_SOCKET; }
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <signal.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <unistd.h>
    typedef int sock_t;
    #define CLOSE_SOCKET close
    static bool sock_ok(sock_t s){ return s >= 0; }
#endif

using namespace std;

static const size_t MAX_HEADER_BYTES = 16 * 1024;
static const size_t MAX_BODY_BYTES   = 16 * 1024;

static int hex_val(char c){
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

string url_decode(const string& s){
    string out;
    out.reserve(s.size());
    for(size_t i = 0; i < s.size(); i++){
        if(s[i] == '+') out += ' ';
        else if(s[i] == '%' && i + 2 < s.size() && hex_val(s[i+1]) >= 0 && hex_val(s[i+2]) >= 0){
            out += (char)(hex_val(s[i+1]) * 16 + hex_val(s[i+2]));
            i += 2;
        }else out += s[i];
    }
    return out;
}

// a=1&b=2  ->  params
static void parse_urlencoded(const string& s, map<string,string>& params){
    size_t start = 0;
    while(start <= s.size()){
        size_t amp = s.find('&', start);
        if(amp == string::npos) amp = s.size();
        string pair = s.substr(start, amp - start);
        if(!pair.empty()){
            size_t eq = pair.find('=');
            if(eq == string::npos) params[url_decode(pair)] = "";
            else params[url_decode(pair.substr(0, eq))] = url_decode(pair.substr(eq + 1));
        }
        start = amp + 1;
    }
}

// Very small parser for a FLAT json object: {"game":"abc","word":"cat"}
// Values may be strings, numbers, true/false/null. Nested objects are ignored.
static void parse_flat_json(const string& s, map<string,string>& params){
    size_t i = 0, n = s.size();
    auto skip_ws = [&](){ while(i < n && isspace((unsigned char)s[i])) i++; };
    auto read_string = [&](string& out) -> bool{
        if(i >= n || s[i] != '"') return false;
        i++;
        while(i < n && s[i] != '"'){
            if(s[i] == '\\' && i + 1 < n){
                i++;
                char c = s[i];
                if(c == 'n') out += '\n';
                else if(c == 't') out += '\t';
                else if(c == 'r') out += '\r';
                else if(c == 'u' && i + 4 < n){ // keep it simple: only ASCII \u00XX
                    int v = 0;
                    for(int k = 1; k <= 4; k++){ int h = hex_val(s[i+k]); v = v * 16 + (h < 0 ? 0 : h); }
                    if(v < 128) out += (char)v;
                    i += 4;
                }
                else out += c;
            }else out += s[i];
            i++;
        }
        if(i >= n) return false;
        i++; // closing quote
        return true;
    };
    skip_ws();
    if(i >= n || s[i] != '{') return;
    i++;
    while(i < n){
        skip_ws();
        if(i < n && s[i] == '}') return;
        string key, val;
        if(!read_string(key)) return;
        skip_ws();
        if(i >= n || s[i] != ':') return;
        i++;
        skip_ws();
        if(i < n && s[i] == '"'){
            if(!read_string(val)) return;
        }else{
            while(i < n && s[i] != ',' && s[i] != '}' && !isspace((unsigned char)s[i])) val += s[i++];
        }
        params[key] = val;
        skip_ws();
        if(i < n && s[i] == ',') i++;
    }
}

static const char* status_text(int code){
    switch(code){
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default:  return "Unknown";
    }
}

static void send_all(sock_t c, const string& data){
    size_t sent = 0;
    while(sent < data.size()){
        int r = send(c, data.data() + sent, (int)(data.size() - sent), 0);
        if(r <= 0) return;
        sent += (size_t)r;
    }
}

static void send_response(sock_t c, const HttpResponse& res){
    ostringstream out;
    out << "HTTP/1.1 " << res.status << " " << status_text(res.status) << "\r\n";
    out << "Content-Type: " << res.content_type << "\r\n";
    out << "Content-Length: " << res.body.size() << "\r\n";
    // CORS: lets a frontend served from a different port/domain call the API
    out << "Access-Control-Allow-Origin: *\r\n";
    out << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    out << "Access-Control-Allow-Headers: Content-Type\r\n";
    out << "Cache-Control: no-store\r\n";
    out << "Connection: close\r\n\r\n";
    out << res.body;
    send_all(c, out.str());
}

static HttpResponse error_response(int code, const string& msg){
    HttpResponse r;
    r.status = code;
    r.body = "{\"error\":\"" + msg + "\"}";
    return r;
}

static void set_timeout(sock_t c, int seconds){
#ifdef _WIN32
    DWORD ms = seconds * 1000;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ms, sizeof ms);
    setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, (const char*)&ms, sizeof ms);
#else
    struct timeval tv; tv.tv_sec = seconds; tv.tv_usec = 0;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
#endif
}

// Reads one request from the socket. Returns an error code (>0) or 0 on success.
static int read_request(sock_t c, HttpRequest& req){
    string data;
    char buf[4096];
    size_t header_end = string::npos;
    while(header_end == string::npos){
        int r = recv(c, buf, sizeof buf, 0);
        if(r <= 0) return 400;
        data.append(buf, r);
        header_end = data.find("\r\n\r\n");
        if(header_end == string::npos && data.size() > MAX_HEADER_BYTES) return 413;
    }

    // request line
    size_t line_end = data.find("\r\n");
    istringstream first(data.substr(0, line_end));
    string target, version;
    first >> req.method >> target >> version;
    if(req.method.empty() || target.empty()) return 400;

    // headers
    size_t pos = line_end + 2;
    while(pos < header_end){
        size_t e = data.find("\r\n", pos);
        string line = data.substr(pos, e - pos);
        size_t colon = line.find(':');
        if(colon != string::npos){
            string key = line.substr(0, colon);
            for(char& ch : key) ch = (char)tolower((unsigned char)ch);
            size_t vs = colon + 1;
            while(vs < line.size() && line[vs] == ' ') vs++;
            req.headers[key] = line.substr(vs);
        }
        pos = e + 2;
    }

    // body
    size_t content_length = 0;
    auto cl = req.headers.find("content-length");
    if(cl != req.headers.end()) content_length = (size_t)strtoul(cl->second.c_str(), NULL, 10);
    if(content_length > MAX_BODY_BYTES) return 413;
    req.body = data.substr(header_end + 4);
    while(req.body.size() < content_length){
        int r = recv(c, buf, sizeof buf, 0);
        if(r <= 0) return 400;
        req.body.append(buf, r);
    }
    if(req.body.size() > content_length) req.body.resize(content_length);

    // path + query string
    size_t q = target.find('?');
    req.path = url_decode(target.substr(0, q));
    if(q != string::npos) parse_urlencoded(target.substr(q + 1), req.params);

    // body params (override query params)
    string ctype = req.headers.count("content-type") ? req.headers["content-type"] : "";
    if(ctype.find("application/json") != string::npos) parse_flat_json(req.body, req.params);
    else if(ctype.find("application/x-www-form-urlencoded") != string::npos) parse_urlencoded(req.body, req.params);
    return 0;
}

bool run_http_server(const string& host, int port, HttpHandler handler, function<void()> on_ready){
#ifdef _WIN32
    WSADATA wsa;
    if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0){ cerr << "WSAStartup failed\n"; return false; }
#else
    signal(SIGPIPE, SIG_IGN); // a client closing early must not kill the server
#endif
    sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(!sock_ok(s)){ cerr << "socket() failed\n"; return false; }
#ifndef _WIN32
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
#endif
    sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    if(bind(s, (sockaddr*)&addr, sizeof addr) != 0){
        cerr << "bind() failed - is port " << port << " already in use?\n";
        CLOSE_SOCKET(s);
        return false;
    }
    if(listen(s, 64) != 0){ cerr << "listen() failed\n"; CLOSE_SOCKET(s); return false; }
    if(on_ready) on_ready();

    while(true){
        sock_t c = accept(s, NULL, NULL);
        if(!sock_ok(c)) continue;
        set_timeout(c, 5); // a slow client can't hold the (single) server forever

        HttpRequest req;
        int err = read_request(c, req);
        if(err){
            send_response(c, error_response(err, err == 413 ? "request too large" : "bad request"));
        }else if(req.method == "OPTIONS"){
            HttpResponse pre; pre.status = 204; pre.content_type = "text/plain";
            send_response(c, pre);
        }else{
            HttpResponse res;
            try{
                res = handler(req);
            }catch(const exception& e){
                res = error_response(500, "internal error");
                cerr << "handler error: " << e.what() << "\n";
            }
            send_response(c, res);
        }
        CLOSE_SOCKET(c);
    }
}
