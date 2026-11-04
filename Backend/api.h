// api.h - the Contexto game API (see api.cpp). No sockets in here.
#ifndef API_H
#define API_H

#include <string>
#include "http_server.h"   // only for the HttpRequest / HttpResponse structs

bool api_init(const std::string& data_path);       // loads vectors.bin (load_binary)
int  api_word_count();
HttpResponse api_route(const HttpRequest& req);    // handles every /api/... path

#endif
