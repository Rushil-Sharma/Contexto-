# Contexto backend (C++)

The game logic from `C++/main.cpp`, usable in two ways:

1. **On the website (GitHub Pages):** the C++ is compiled to WebAssembly (`../wasm/contexto.js`) and runs inside the visitor's browser. No server is needed.
2. **Locally:** as a native HTTP server (`contexto_server.exe`).

Both use the same `api.cpp` and `contexto.cpp`, so they return the same JSON.

| File | What it is |
|---|---|
| `contexto.h / .cpp` | Your game: `cosine_similarity`, `load_text`, `load_binary`, the ranking (sims → order → rank_of), and the guess, HINT, HELP, EXIT and RUSHIL branches. The logic is the same as `main.cpp`, but results are returned instead of printed with `cout`. |
| `api.h / .cpp` | Holds the running games and turns each result into JSON. Handles every `/api/*` path (`api_route`). |
| `wasm_api.cpp` | WebAssembly entry points `contexto_init` / `contexto_call`, which go through `api_route` |
| `build_wasm.sh` | Builds `../wasm/contexto.js` with Emscripten (the compiled C++ plus the embedded `vectors.bin`, in one file) |
| `http_server.h / .cpp` | A small single-threaded HTTP/1.1 server (Winsock on Windows, BSD sockets on Linux/macOS) |
| `server.cpp` | Native server `main()`: serves the website files plus `/api/*` |
| `data/vectors.bin` | Copy of `C++/data/vectors.bin` |

## Deploying (GitHub Pages)

`.github/workflows/pages.yml` runs `build_wasm.sh` on every push to `main` and publishes `index.html`, `style.css`, `script.js`, `engine-worker.js` and `wasm/contexto.js` (the engine runs in a Web Worker, so it never blocks the page). One-time setup: in the repo, go to **Settings → Pages → Source** and choose **GitHub Actions**.

To build the WebAssembly file locally (so that double-clicking `index.html` works without the server), install [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) and run `bash Backend/build_wasm.sh`.

## Build & run

```
# Windows (MinGW)
g++ -O2 -std=c++14 server.cpp api.cpp contexto.cpp http_server.cpp -o contexto_server -lws2_32
.\contexto_server.exe

# Linux / macOS
g++ -O2 -std=c++14 server.cpp api.cpp contexto.cpp http_server.cpp -o contexto_server
./contexto_server
```

Double-click `contexto_server.exe` (or run it from `Backend/`): it serves `../index.html`, `style.css` and `script.js` and opens http://localhost:8080 in your browser. Keep the window open while playing. Opening `index.html` directly also works, as long as the server is running.

Options: `--port 8080` (or the `PORT` env var, which also binds to 0.0.0.0 for hosting), `--host 127.0.0.1`, `--data data/vectors.bin`, `--static <folder>` (default: the folder containing `index.html`), `--no-browser`. Only .html/.css/.js/.png/.svg/.ico files are served; hidden paths like `.git` never are.

## API

Every endpoint returns JSON. Parameters can be sent as a query string, as a form body (`game=..&word=..`) or as a JSON body (`{"game":"..","word":".."}`). CORS is open (`*`).

Every game response includes the current state: `tries`, `hints`, `best_rank` (null until the first valid guess), `finished` and `won`.

| Endpoint | Original equivalent | Response |
|---|---|---|
| `POST /api/new` | starting the program (picks the secret) | `{game, words, ...state}`. Keep the `game` id. |
| `POST /api/guess` `game, word` | typing a word | `status`: `ok` (+`rank`), `won` (+`rank`, `message`), `not_found`, or `game_over` (409) |
| `POST /api/hint` `game` | `HINT` | `kind`: `letter_count` (`letters`), `first_letter` / `last_letter` (`letter`), `word_at_rank` (`rank`, `word`), or `no_more`. Always includes `message`. |
| `POST /api/giveup` `game` | `EXIT` | `{secret, top:[{rank, word, you_were_here}] (ranks 2–21)}` |
| `GET /api/help` | `HELP` | `{lines:[...]}` |
| `GET /api/state?game=` | none | state only |
| `GET /api/health` | none | `{status, words, games}` |

`/api/guess` also accepts the typed commands `HELP`, `HINT`, `EXIT` and `RUSHIL`, exactly like the terminal prompt. `RUSHIL` returns `{type:"cheat", secret}`. Each response has a `type` field (`guess`, `hint`, `help`, `exit` or `cheat`) so the frontend can tell them apart.

Notes:
- Guesses are case-sensitive and only the first word is used, the same as `cin >> guess`. The frontend should lowercase input before sending it.
- Repeating a guess counts as a try, as in the original.
- One change from `C++/main.cpp`: a hint that reveals a word (hint 4 and later) also counts as a guess. It adds a try and can lower `best_rank`, so later hints get closer and stop once you are inside the top 50.
- Games idle for 24h are deleted. At most 20,000 games are kept at once.
