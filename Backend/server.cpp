// server.cpp
// Runs Contexto locally: a native HTTP server around the game API in api.cpp.
// (On GitHub Pages the same api.cpp runs in the browser as WebAssembly instead - see wasm_api.cpp.)
//
// Build:  g++ -O2 -std=c++14 server.cpp api.cpp contexto.cpp http_server.cpp -o contexto_server -lws2_32   (Windows)
//         g++ -O2 -std=c++14 server.cpp api.cpp contexto.cpp http_server.cpp -o contexto_server            (Linux/macOS)
// Run:    contexto_server [--port 8080] [--host 0.0.0.0] [--data data/vectors.bin] [--static ..] [--no-browser]
#include "api.h"
#include "http_server.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

using namespace std;

static string STATIC_DIR;

static HttpResponse not_found(){
    HttpResponse r; r.status = 404; r.body = "{\"error\":\"not found\"}"; return r;
}

// ------------------------------------------------------------------ static files
// Only web files are served, and never hidden paths like /.git, so the
// C++ sources next to index.html are not exposed.
static HttpResponse serve_static(const string& path){
    if(STATIC_DIR.empty() || path.find("..") != string::npos || path.find("/.") != string::npos)
        return not_found();
    string rel = path == "/" ? "/index.html" : path;
    size_t dot = rel.rfind('.');
    string ext = dot == string::npos ? "" : rel.substr(dot + 1);
    HttpResponse r;
    if(ext == "html")      r.content_type = "text/html; charset=utf-8";
    else if(ext == "css")  r.content_type = "text/css; charset=utf-8";
    else if(ext == "js")   r.content_type = "application/javascript; charset=utf-8";
    else if(ext == "svg")  r.content_type = "image/svg+xml";
    else if(ext == "png")  r.content_type = "image/png";
    else if(ext == "ico")  r.content_type = "image/x-icon";
    else return not_found();
    ifstream f(STATIC_DIR + rel, ios::binary);
    if(!f) return not_found();
    r.body.assign((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    return r;
}

static HttpResponse route(const HttpRequest& req){
    if(req.path.compare(0, 5, "/api/") == 0) return api_route(req);
    if(req.method == "GET") return serve_static(req.path);
    return not_found();
}

static bool file_exists(const string& p){ ifstream f(p); return (bool)f; }
// Opens the website in the default browser.
static void open_browser(const string& url){
#ifdef _WIN32
    string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
    string cmd = "open \"" + url + "\" >/dev/null 2>&1 &";
#else
    string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
#endif
    if(system(cmd.c_str()) != 0) cout << "Open " << url << " in your browser\n" << flush;
}

// ------------------------------------------------------------------ main
int main(int argc, char** argv){
    std::ios_base::sync_with_stdio(false);

    string host = "127.0.0.1", data_path = "data/vectors.bin";
    int port = 8080;
    bool browser = true;
    if(const char* env_port = getenv("PORT")){ // hosting providers set PORT
        port = atoi(env_port);
        host = "0.0.0.0";
        browser = false;
    }
    for(int i = 1; i < argc; i++){
        string k = argv[i];
        if(k == "--no-browser"){ browser = false; continue; }
        if(i + 1 >= argc){ cerr << "missing value for " << k << "\n"; return 1; }
        string v = argv[++i];
        if(k == "--port") port = atoi(v.c_str());
        else if(k == "--host") host = v;
        else if(k == "--data") data_path = v;
        else if(k == "--static") STATIC_DIR = v;
        else { cerr << "unknown option " << k << "\n"; return 1; }
    }
    // By default serve the website (index.html, style.css, script.js) from the project folder
    if(STATIC_DIR.empty()){
        if(file_exists("../index.html")) STATIC_DIR = "..";
        else if(file_exists("index.html")) STATIC_DIR = ".";
    }

    cout << "Loading the data ...\n" << flush;
    if(!api_init(data_path)){
        cout << "No binary cache found make sure u have the binary file and try again (" << data_path << ")\n";
        cout << "Press Enter to close..." << flush;
        cin.get();
        return 1;
    }
    cout << "Loaded " << api_word_count() << " words\n" << flush;

    ostringstream url;
    url << "http://localhost:" << port << "/";
    bool ok = run_http_server(host, port, route, [&](){
        cout << "Contexto is running at " << url.str() << "  (close this window to stop it)\n" << flush;
        if(browser) open_browser(url.str());
    });
    if(!ok){
        // most likely the server is already running in another window
        if(browser) open_browser(url.str());
        cout << "Press Enter to close..." << flush;
        cin.get();
        return 1;
    }
    return 0;
}
