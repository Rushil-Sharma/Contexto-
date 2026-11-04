// wasm_api.cpp
// Entry points for the WebAssembly build (used on GitHub Pages / when index.html is opened directly).
// The browser calls these instead of making HTTP requests; they go through exactly the same
// api_route() -> Game -> contexto.cpp code as the native server, and return the same JSON.
//
// Build (from Backend/, with Emscripten):  see build_wasm.sh
#include "api.h"

#include <string>

#ifdef __EMSCRIPTEN__
    #include <emscripten/emscripten.h>
#else
    #define EMSCRIPTEN_KEEPALIVE
#endif

using namespace std;

extern "C" {

// Loads the word vectors. vectors.bin is embedded into the build at /vectors.bin.
// Returns the number of words, or 0 on failure.
EMSCRIPTEN_KEEPALIVE int contexto_init(const char* data_path){
    return api_init(data_path) ? api_word_count() : 0;
}

// path: "/api/new", "/api/guess", "/api/hint", "/api/giveup", "/api/help", "/api/state"
// Returns the JSON body (valid until the next call).
EMSCRIPTEN_KEEPALIVE const char* contexto_call(const char* path, const char* game, const char* word){
    static string out;
    HttpRequest req;
    req.path = path;
    req.method = (req.path == "/api/help" || req.path == "/api/state" || req.path == "/api/health") ? "GET" : "POST";
    if(game && *game) req.params["game"] = game;
    if(word && *word) req.params["word"] = word;
    out = api_route(req).body;
    return out.c_str();
}

}
