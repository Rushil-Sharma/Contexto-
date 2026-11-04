// server.cpp
// Contexto backend: loads data/vectors.bin once, then serves the game over HTTP.
// All game logic lives in contexto.cpp (the C++/main.cpp logic, without cin/cout).
//
// Build:  g++ -O2 -std=c++14 server.cpp contexto.cpp http_server.cpp -o contexto_server -lws2_32   (Windows)
//         g++ -O2 -std=c++14 server.cpp contexto.cpp http_server.cpp -o contexto_server            (Linux/macOS)
// Run:    contexto_server [--port 8080] [--host 0.0.0.0] [--data data/vectors.bin] [--static ../Frontend]
#include "contexto.h"
#include "http_server.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>

using namespace std;

// ------------------------------------------------------------------ config
static const size_t MAX_GAMES      = 20000;        // oldest games are dropped past this
static const time_t GAME_TTL_SECS  = 24 * 60 * 60; // games idle for a day are dropped

// ------------------------------------------------------------------ state
static Dictionary DICT;
static unique_ptr<RankingCache> RANKINGS;

struct Session{
    unique_ptr<Game> game;
    time_t last_used;
};
static unordered_map<string, Session> GAMES;
static mt19937_64 ID_GEN;
static unsigned GAME_COUNTER = 0;
static string STATIC_DIR;

// ------------------------------------------------------------------ json helpers
static string esc(const string& s){
    string o = "\"";
    for(unsigned char c : s){
        switch(c){
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if(c < 0x20){ char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    return o + "\"";
}
static string num(long long v){ ostringstream o; o << v; return o.str(); }
static string boolean(bool b){ return b ? "true" : "false"; }

static HttpResponse json(int status, const string& body){
    HttpResponse r; r.status = status; r.body = body; return r;
}
static HttpResponse error(int status, const string& msg){
    return json(status, "{\"error\":" + esc(msg) + "}");
}

// ------------------------------------------------------------------ game store
static string new_game_id(){
    // (built by hand: old MinGW's printf doesn't understand %llx)
    static const char* HEX = "0123456789abcdef";
    unsigned long long v = ID_GEN();
    string id(16, '0');
    for(int i = 15; i >= 0; i--){ id[i] = HEX[v & 15]; v >>= 4; }
    return id;
}

static void prune_games(){
    time_t now = time(NULL);
    for(auto it = GAMES.begin(); it != GAMES.end();){
        if(now - it->second.last_used > GAME_TTL_SECS) it = GAMES.erase(it); else ++it;
    }
    while(GAMES.size() >= MAX_GAMES){ // still full: drop the least recently used one
        auto oldest = GAMES.begin();
        for(auto it = GAMES.begin(); it != GAMES.end(); ++it)
            if(it->second.last_used < oldest->second.last_used) oldest = it;
        GAMES.erase(oldest);
    }
}

static Game* find_game(const HttpRequest& req){
    auto it = GAMES.find(req.param("game"));
    if(it == GAMES.end()) return NULL;
    it->second.last_used = time(NULL);
    return it->second.game.get();
}

// "cin >> guess" only ever reads one whitespace-separated token
static string first_token(const string& s){
    istringstream in(s);
    string t;
    in >> t;
    return t;
}

// ------------------------------------------------------------------ JSON for each result
static string state_json(const Game& g){
    return "\"tries\":" + num(g.get_tries()) +
           ",\"hints\":" + num(g.get_hints()) +
           ",\"best_rank\":" + (g.get_tries() == 0 ?string("null") : num(g.get_best_rank())) +
           ",\"finished\":" + boolean(g.finished()) +
           ",\"won\":" + boolean(g.won());
}

static HttpResponse help_response(){
    string lines = "[";
    vector<string> h = Game::help_text();
    for(size_t i = 0; i < h.size(); i++) lines += (i ? "," : "") + esc(h[i]);
    lines += "]";
    return json(200, "{\"type\":\"help\",\"lines\":" + lines + "}");
}

static HttpResponse hint_response(Game& g){
    if(g.finished()) return error(409, "game is over");
    HintResult h = g.hint();
    string body = "{\"type\":\"hint\",\"number\":" + num(h.number) + ",";
    switch(h.type){
        case HintType::LETTER_COUNT:
            body += "\"kind\":\"letter_count\",\"letters\":" + num(h.letters) +
                    ",\"message\":" + esc("The word contains " + num(h.letters) + " letters");
            break;
        case HintType::FIRST_LETTER:
            body += "\"kind\":\"first_letter\",\"letter\":" + esc(string(1, h.letter)) +
                    ",\"message\":" + esc("The word starts with the letter " + string(1, h.letter));
            break;
        case HintType::LAST_LETTER:
            body += "\"kind\":\"last_letter\",\"letter\":" + esc(string(1, h.letter)) +
                    ",\"message\":" + esc("The word ends with the letter " + string(1, h.letter));
            break;
        case HintType::WORD_AT_RANK:
            body += "\"kind\":\"word_at_rank\",\"rank\":" + num(h.rank) + ",\"word\":" + esc(h.word) +
                    ",\"message\":" + esc("The word at rank " + num(h.rank) + " is " + h.word);
            break;
        case HintType::NO_MORE:
            body += "\"kind\":\"no_more\",\"message\":" + esc("Alright thats enough hints for today no more hints now !");
            break;
    }
    return json(200, body + "," + state_json(g) + "}");
}

static HttpResponse exit_response(Game& g){
    ExitResult e = g.exit_game();
    string top = "[";
    for(size_t i = 0; i < e.top.size(); i++){
        if(i) top += ",";
        top += "{\"rank\":" + num(e.top[i].rank) + ",\"word\":" + esc(e.top[i].word) +
               ",\"you_were_here\":" + boolean(e.top[i].you_were_here) + "}";
    }
    top += "]";
    return json(200, "{\"type\":\"exit\",\"secret\":" + esc(e.secret) + ",\"top\":" + top + "," + state_json(g) + "}");
}

static HttpResponse guess_response(Game& g, const string& word){
    GuessResult r = g.guess(word);
    string head = "{\"type\":\"guess\",\"word\":" + esc(r.word) + ",";
    switch(r.status){
        case GuessStatus::NOT_FOUND:
            return json(200, head + "\"status\":\"not_found\",\"message\":\"Word Not found, Try again !\"," + state_json(g) + "}");
        case GuessStatus::GAME_OVER:
            return json(409, head + "\"status\":\"game_over\",\"message\":\"The game is already over.\"," + state_json(g) + "}");
        case GuessStatus::WON:
            return json(200, head + "\"status\":\"won\",\"rank\":" + num(r.rank) + ",\"message\":" +
                        esc("Congratulations! You found the word in " + num(r.tries) + " guesses!") + "," + state_json(g) + "}");
        default:
            return json(200, head + "\"status\":\"ok\",\"rank\":" + num(r.rank) + "," + state_json(g) + "}");
    }
}

// ------------------------------------------------------------------ static files (optional)
// Only web files are served, and never hidden paths like /.git, so the
// C++ sources / data next to index.html are not exposed.
static HttpResponse serve_static(const string& path){
    if(STATIC_DIR.empty() || path.find("..") != string::npos || path.find("/.") != string::npos)
        return error(404, "not found");
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
    else return error(404, "not found");
    ifstream f(STATIC_DIR + rel, ios::binary);
    if(!f) return error(404, "not found");
    r.body.assign((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    return r;
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

// ------------------------------------------------------------------ router
static HttpResponse route(const HttpRequest& req){
    const string& p = req.path;
    const bool get = req.method == "GET", post = req.method == "POST";

    if(p == "/api/health" && get)
        return json(200, "{\"status\":\"ok\",\"words\":" + num(DICT.count) + ",\"games\":" + num((long long)GAMES.size()) + "}");

    if(p == "/api/help" && get) return help_response();

    if(p == "/api/new" && post){
        prune_games();
        // same seeding as main.cpp (clock), plus a counter so two games
        // started in the same clock tick still get different secrets
        unsigned seed = chrono::high_resolution_clock::now().time_since_epoch().count();
        seed ^= (++GAME_COUNTER) * 2654435761u;
        string id = new_game_id();
        while(GAMES.count(id)) id = new_game_id();
        Session s;
        s.game.reset(new Game(DICT, *RANKINGS, seed));
        s.last_used = time(NULL);
        Game& g = *s.game;
        GAMES[id] = std::move(s);
        return json(200, "{\"game\":" + esc(id) + ",\"words\":" + num(DICT.count) + "," + state_json(g) + "}");
    }

    if(p.compare(0, 5, "/api/") == 0 && !get && !post) return error(405, "method not allowed");

    // everything below needs an existing game
    if(p == "/api/state" || p == "/api/guess" || p == "/api/hint" || p == "/api/giveup"){
        if(p != "/api/state" && !post) return error(405, "use POST");
        Game* g = find_game(req);
        if(!g) return error(404, "unknown game id - start a new game with POST /api/new");

        if(p == "/api/state")  return json(200, "{" + state_json(*g) + "}");
        if(p == "/api/hint")   return hint_response(*g);
        if(p == "/api/giveup") return exit_response(*g);

        // /api/guess : behaves exactly like typing into the original prompt
        string guess = first_token(req.param("word"));
        if(guess.empty()) return error(400, "missing 'word'");
        if(guess == "RUSHIL") return json(200, "{\"type\":\"cheat\",\"secret\":" + esc(g->cheat_secret()) + "}");
        if(guess == "HELP")   return help_response();
        if(guess == "EXIT")   return exit_response(*g);
        if(guess == "HINT")   return hint_response(*g);
        return guess_response(*g, guess);
    }

    if(p.compare(0, 5, "/api/") == 0) return error(404, "no such endpoint");
    if(get) return serve_static(p);
    return error(404, "not found");
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
    if(!load_binary(data_path, DICT.Vec, DICT.word_to_index, DICT.count)){
        cout << "No binary cache found make sure u have the binary file and try again (" << data_path << ")\n";
        cout << "Press Enter to close..." << flush;
        cin.get();
        return 1;
    }
    cout << "Loaded " << DICT.count << " words\n" << flush;
    RANKINGS.reset(new RankingCache(DICT));
    ID_GEN.seed((unsigned long long)chrono::high_resolution_clock::now().time_since_epoch().count());

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
