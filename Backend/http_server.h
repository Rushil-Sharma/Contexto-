// http_server.h
// A tiny dependency-free HTTP/1.1 server (Winsock on Windows, BSD sockets elsewhere).
// Single-threaded on purpose: MinGW GCC 6.3 has no std::thread / std::mutex, and every
// request here takes only a few milliseconds, so one-at-a-time is plenty for this game.
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <functional>
#include <map>
#include <string>

struct HttpRequest{
    std::string method;                         // GET, POST, OPTIONS ...
    std::string path;                           // "/api/guess" (no query string)
    std::map<std::string, std::string> params;  // query string + x-www-form-urlencoded body
    std::map<std::string, std::string> headers; // keys lower-cased
    std::string body;

    std::string param(const std::string& key, const std::string& def = "") const{
        auto it = params.find(key);
        return it == params.end() ? def : it->second;
    }
};

struct HttpResponse{
    int status = 200;
    std::string content_type = "application/json";
    std::string body;
};

typedef std::function<HttpResponse(const HttpRequest&)> HttpHandler;

// Blocks forever serving requests. Returns false if the socket could not be opened.
// `on_ready` (optional) is called once the server is listening.
bool run_http_server(const std::string& host, int port, HttpHandler handler, std::function<void()> on_ready = nullptr);

std::string url_decode(const std::string& s);

#endif
