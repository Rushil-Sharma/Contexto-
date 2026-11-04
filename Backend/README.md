# Contexto backend (C++)

The game logic from `C++/main.cpp` running as an HTTP server. There are no external libraries: it uses only the standard library and sockets.

| File | What it is |
|---|---|
| `contexto.h / .cpp` | Your game: `cosine_similarity`, `load_text`, `load_binary`, the ranking (sims → order → rank_of), and the guess, HINT, HELP, EXIT and RUSHIL branches. The logic is the same as `main.cpp`, but results are returned instead of printed with `cout`. |
| `http_server.h / .cpp` | A small single-threaded HTTP/1.1 server (Winsock on Windows, BSD sockets on Linux/macOS) |
| `server.cpp` | `main()`: loads `data/vectors.bin` once, keeps one `Game` per player, and handles the `/api/*` routes |
| `data/vectors.bin` | Copy of `C++/data/vectors.bin` |

## Build & run

```
# Windows (MinGW)
g++ -O2 -std=c++14 server.cpp contexto.cpp http_server.cpp -o contexto_server -lws2_32
.\contexto_server.exe

# Linux / macOS
g++ -O2 -std=c++14 server.cpp contexto.cpp http_server.cpp -o contexto_server
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
