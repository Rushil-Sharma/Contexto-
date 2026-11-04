// Small glue code: all game logic runs in the C++ backend (Backend/).
// Opened through the C++ server -> same origin. Opened as a file -> talk to localhost:8080.
const API = location.protocol.startsWith("http") ? "" : "http://localhost:8080";
const $ = id => document.getElementById(id);
let game = null, guesses = [];

async function call(path, data) {
    const res = await fetch(API + path, {
        method: data ? "POST" : "GET",
        headers: { "Content-Type": "application/json" },
        body: data ? JSON.stringify(data) : undefined
    });
    return res.json();
}

function show(text) { $("msg").textContent = text; }

function render(latest) {
    $("guesses").innerHTML = "";
    guesses.slice().sort((a, b) => a.rank - b.rank).forEach(g => {
        const li = document.createElement("li");
        li.innerHTML = "<span></span><span>" + g.rank + "</span>";
        li.firstChild.textContent = g.word;
        if (g.word === latest) li.className = "latest";
        $("guesses").appendChild(li);
    });
}

async function newGame() {
    try {
        game = (await call("/api/new", {})).game;
        guesses = []; render(); $("tries").textContent = 0; show("");
    } catch (e) {
        show("Can't reach the C++ server. Start Backend/contexto_server.exe first.");
    }
}

$("form").onsubmit = async e => {
    e.preventDefault();
    const word = $("inp").value.trim().toLowerCase();
    $("inp").value = "";
    if (!word || !game) return;
    const r = await call("/api/guess", { game, word });
    if (r.status === "not_found") return show("Word not found, try again!");
    if (r.status === "game_over") return show("The game is over. Start a new one!");
    if (!guesses.some(g => g.word === word)) guesses.push({ word, rank: r.rank });
    $("tries").textContent = r.tries;
    show(r.status === "won" ? r.message : "");
    render(word);
};

$("hint").onclick = async () => {
    if (!game) return;
    const r = await call("/api/hint", { game });
    show(r.message || r.error);
};

$("giveup").onclick = async () => {
    if (!game) return;
    const r = await call("/api/giveup", { game });
    show("The word was: " + r.secret + ". Closest: " + r.top.slice(0, 5).map(t => t.word).join(", "));
};

$("new").onclick = newGame;
newGame();
