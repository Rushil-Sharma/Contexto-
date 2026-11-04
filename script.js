// Contexto frontend. All game logic (ranking, hints, scoring) runs in the C++ backend;
// this file only animates, calls the backend and draws the answers.
//
// The C++ backend runs in one of two ways:
//  1. WebAssembly (wasm/contexto.js, built from Backend/ by build_wasm.sh): the C++ runs right
//     here in the browser. This is what GitHub Pages uses - no server needed.
//  2. Fallback: the native C++ server (Backend/contexto_server.exe) over HTTP.
const API = location.protocol.startsWith("http") ? "" : "http://localhost:8080";

// Start loading the C++ engine immediately; the intro animation hides the load time.
const engineReady = (async () => {
    if (typeof createContexto !== "function") return null;      // wasm build not present
    const M = await createContexto();
    if (!M.ccall("contexto_init", "number", ["string"], ["/vectors.bin"])) throw new Error("vectors.bin failed to load");
    return M;
})().catch(err => { console.error(err); return null; });
const $ = id => document.getElementById(id);
const reduced = matchMedia("(prefers-reduced-motion: reduce)").matches;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const TOTAL_WORDS = 7906;

// word2vec embedding of "context" (shown in the intro)
const CONTEXT_VEC = [0.130859,0.065918,-0.083984,0.086914,-0.161133,0.095215,0.098633,0.075684,0.298828,-0.040527,-0.016602,-0.154297,-0.018311,0.143555,-0.217773,0.079590,-0.163086,0.135742,0.088867,-0.208008,0.043701,-0.078613,0.024658,0.074707,-0.042236,-0.080566,-0.137695,0.055664,0.024536,0.326172,-0.341797,-0.059326,-0.219727,0.382812,-0.089355,-0.071289,-0.013062,0.055420,0.046875,0.001450,0.106445,0.235352,-0.035645,0.277344,0.052979,-0.060547,-0.287109,-0.228516,0.103516,-0.145508,-0.057129,-0.167969,-0.246094,-0.255859,0.101074,0.040039,0.118652,-0.112305,0.078613,-0.088867,-0.302734,0.000305,0.172852,-0.006165,-0.103027,0.176758,-0.227539,0.064453,-0.047363,-0.120605,-0.154297,-0.064941,0.156250,-0.212891,0.114258,-0.067383,0.122070,0.265625,0.298828,0.106934,0.072266,0.168945,0.068848,-0.009216,0.064453,-0.067383,0.129883,0.196289,-0.085449,0.125977,0.225586,-0.045410,0.041992,-0.166992,-0.148438,0.029541,0.020142,-0.063477,0.077637,-0.131836,-0.332031,0.045898,-0.138672,-0.019043,-0.211914,-0.190430,-0.269531,0.012695,-0.154297,-0.085449,-0.219727,0.153320,0.006042,0.097656,0.247070,-0.009705,0.097168,0.019653,0.386719,0.306641,-0.234375,-0.134766,-0.106934,0.207031,-0.221680,0.182617,-0.187500,0.110840,-0.140625,-0.134766,-0.244141,-0.164062,-0.167969,0.041992,0.167969,0.048096,0.021362,-0.091797,0.009644,-0.005035,0.140625,0.041992,0.127930,0.036621,0.033936,-0.165039,-0.178711,0.154297,-0.117188,0.141602,0.151367,0.170898,-0.173828,-0.083496,0.047852,-0.026978,-0.234375,-0.008118,-0.080566,-0.143555,-0.063477,0.107422,0.292969,-0.083496,0.034668,0.189453,-0.170898,-0.073242,-0.070312,0.253906,-0.005554,0.033936,-0.034668,-0.144531,0.213867,0.007080,0.088867,-0.189453,0.060303,0.099121,0.008362,0.026245,-0.054932,-0.093262,0.121582,-0.056885,0.048096,0.216797,-0.061035,0.209961,-0.008118,-0.122559,0.038330,-0.096191,-0.138672,-0.032715,-0.113281,0.087891,0.068848,-0.009521,-0.014526,0.108398,-0.194336,-0.041016,-0.105469,-0.168945,0.105469,-0.086426,-0.043213,-0.021484,-0.059326,0.015320,-0.133789,0.123047,-0.330078,-0.248047,0.050049,0.133789,0.104980,-0.044922,-0.071289,0.320312,-0.192383,0.057129,-0.050781,-0.175781,0.166992,0.083008,-0.077637,0.186523,-0.070312,-0.062988,-0.076172,-0.021118,0.306641,-0.100098,0.148438,0.306641,0.300781,-0.065430,-0.363281,0.188477,0.182617,-0.015320,0.185547,0.074219,-0.359375,0.261719,-0.080078,0.127930,0.072754,0.149414,-0.345703,-0.010986,0.107422,-0.115723,0.197266,0.021973,0.215820,-0.227539,-0.020630,-0.040039,-0.069824,-0.071289,0.111328,0.363281,0.114258,-0.269531,-0.308594,-0.215820,-0.177734,0.052246,0.375000,0.155273,0.029785,-0.018066,0.008118,0.081055,-0.215820,0.117188,-0.026611,-0.003662,0.018188,-0.041016,-0.263672,-0.060791,0.076172,-0.042969,-0.051514,-0.006592,-0.204102,-0.091797,0.034912,-0.037842,-0.053467,-0.027100,-0.094238,0.022583,-0.134766,-0.060547];

// ---------------------------------------------------------------- pages
function showPage(id) {
    for (const p of document.querySelectorAll(".page")) p.hidden = p.id !== id;
    window.scrollTo(0, 0);
}

// ---------------------------------------------------------------- wave transition
// Like a slide transition: the browser snapshots the current page, update() swaps in
// the new page / theme, and the NEW page is revealed through a wavy hole that grows
// out of (x, y). The old page dims underneath it.
// Uses the View Transitions API; browsers without it just swap instantly.
let waving = false;
function wave(x, y, update) {
    if (reduced || !document.startViewTransition) { update(); return Promise.resolve(); }
    waving = true;
    const vt = document.startViewTransition(update);
    const W = innerWidth, H = innerHeight;
    const maxR = Math.hypot(Math.max(x, W - x), Math.max(y, H - y)) + 60;

    // keyframes: a wavy polygon (same point count in every frame so it interpolates)
    const FRAMES = 40, POINTS = 160, frames = [];
    for (let f = 0; f <= FRAMES; f++) {
        const p = f / FRAMES;
        // gentle ease-out (sine): fast start, steady finish, no long crawl at the end
        const r = Math.sin(p * Math.PI / 2) * maxR;
        const amp = (8 + 0.05 * maxR) * Math.sin(p * Math.PI);  // wobble scales with screen, peaks mid-way
        const phase = p * 14;
        const pts = [];
        for (let i = 0; i < POINTS; i++) {
            const a = (i / POINTS) * Math.PI * 2;
            const rr = f === FRAMES ? maxR * 1.5
                     : Math.max(0, r + amp * Math.sin(7 * a + phase) + amp * 0.5 * Math.sin(12 * a - phase * 1.4));
            pts.push((x + rr * Math.cos(a)).toFixed(1) + "px " + (y + rr * Math.sin(a)).toFixed(1) + "px");
        }
        frames.push({ clipPath: "polygon(" + pts.join(",") + ")" });
    }

    vt.ready.then(() => {
        const opts = { duration: 850, easing: "linear", fill: "both" };
        document.documentElement.animate(frames, { ...opts, pseudoElement: "::view-transition-new(root)" });
        document.documentElement.animate(
            [{ filter: "brightness(1)" }, { filter: "brightness(0.5)" }],
            { ...opts, easing: "ease-in", pseudoElement: "::view-transition-old(root)" });
    }).catch(() => {});
    return vt.finished.catch(() => {}).then(() => { waving = false; });
}
function centerOf(el) {
    const b = el.getBoundingClientRect();
    return [b.left + b.width / 2, b.top + b.height / 2];
}

// ---------------------------------------------------------------- theme
let theme = "dark";
try { if (localStorage.getItem("contexto-theme") === "light") theme = "light"; } catch (e) {}
function applyTheme(t) {
    theme = t;
    document.documentElement.dataset.theme = t;
    for (const s of document.querySelectorAll(".theme-switch")) s.setAttribute("aria-checked", t === "light");
    try { localStorage.setItem("contexto-theme", t); } catch (e) {}
}
applyTheme(theme);
for (const s of document.querySelectorAll(".theme-switch")) {
    s.addEventListener("click", () => {
        if (waving) return;
        const next = theme === "dark" ? "light" : "dark";
        const [x, y] = centerOf(s);
        wave(x, y, () => applyTheme(next));
    });
}

// ---------------------------------------------------------------- 1. intro (< 10 s)
async function intro() {
    if (reduced) return goHome();
    let done = false;
    const stop = new AbortController();               // removes the skip listeners when the intro ends
    const finish = () => { if (!done) { done = true; stop.abort(); goHome(); } };
    addEventListener("keydown", finish, { signal: stop.signal });
    $("intro").addEventListener("pointerdown", finish, { signal: stop.signal });

    const word = $("intro-word"), vec = $("intro-vec"), text = $("intro-text");
    const step = async ms => { await sleep(ms); return done; };

    if (await step(400)) return;
    for (const ch of "Contexto") { word.textContent += ch; if (await step(120)) return; }   // type
    if (await step(600)) return;
    while (word.textContent) { word.textContent = word.textContent.slice(0, -1); if (await step(40)) return; } // erase
    if (await step(200)) return;
    for (const ch of "context") { word.textContent += ch; if (await step(60)) return; }     // retype
    if (await step(250)) return;

    text.classList.add("embedding");                                                          // shrink + stream the vector
    vec.textContent = "=[";
    const t0 = performance.now(), dur = 2400;
    await new Promise(res => {
        (function tick(now) {
            if (done) return res();
            const n = Math.min(CONTEXT_VEC.length, Math.ceil(((now - t0) / dur) * CONTEXT_VEC.length));
            vec.textContent = "=[" + CONTEXT_VEC.slice(0, n).map(v => v.toFixed(6)).join(", ") + (n === CONTEXT_VEC.length ? "]" : "");
            n < CONTEXT_VEC.length ? requestAnimationFrame(tick) : res();
        })(t0);
    });
    if (await step(1000)) return;
    $("intro").classList.add("fade-out");
    if (await step(400)) return;
    finish();
}
function goHome() {
    showPage("home");
    $("play").focus();
}

// ---------------------------------------------------------------- 2. home -> game
$("play").addEventListener("click", () => {
    if (waving) return;
    const [x, y] = centerOf($("play"));
    const fresh = !game || finished;
    // keep the transition free of heavy work: only swap the page during it,
    // and start the C++ game once the wave has finished
    wave(x, y, () => {
        showPage("game");
        if (fresh) resetBoard();
    }).then(async () => {
        if (fresh) await newGame();
        $("inp").focus();
    });
});
$("home-link").addEventListener("click", () => goHome());

// ---------------------------------------------------------------- 3. game
let game = null, guesses = [], hints = [], pattern = null, finished = false, latest = null;

async function api(path, data) {
    const engine = await engineReady;
    if (engine) {   // C++ compiled to WebAssembly: same api_route() as the server, same JSON
        const d = data || {};
        return JSON.parse(engine.ccall("contexto_call", "string", ["string", "string", "string"], [path, d.game || "", d.word || ""]));
    }
    const res = await fetch(API + path, {
        method: data ? "POST" : "GET",
        headers: { "Content-Type": "application/json" },
        body: data ? JSON.stringify(data) : undefined
    });
    return res.json();
}

function say(text, kind) {
    const m = $("msg");
    m.textContent = text || "";
    m.className = "msg" + (kind ? " " + kind : "");
}

function setControls() {
    $("inp").disabled = finished;
    $("hint").disabled = finished;
    $("giveup").disabled = finished;
}

function resetBoard() {
    game = null; guesses = []; hints = []; pattern = null; finished = false; latest = null;
    $("tries").textContent = "0";
    $("result").hidden = true;
    say("");
    render();
    setControls();
}

async function newGame() {
    resetBoard();
    const slow = setTimeout(() => say("Loading the word vectors..."), 300);
    try {
        game = (await api("/api/new", {})).game;
        say("");
    } catch (e) {
        say("Can't load the game engine. Build wasm/contexto.js or start Backend/contexto_server.exe.", "error");
    }
    clearTimeout(slow);
}

// closeness on a log scale: rank 7906 -> 0%, rank 1 -> 100%
const closeness = rank => rank <= 1 ? 100 : Math.max(2, (1 - Math.log(rank) / Math.log(TOTAL_WORDS)) * 100);

function render() {
    // guesses (best rank first)
    const gl = $("guesses");
    gl.textContent = "";
    for (const g of guesses.slice().sort((a, b) => a.rank - b.rank)) {
        const li = document.createElement("li");
        li.className = "guess" + (g.hint ? " hinted" : "") + (g.word === latest ? " latest" : "");
        const line = document.createElement("div"); line.className = "guess-line";
        const w = document.createElement("span"); w.className = "guess-word"; w.textContent = g.word;
        if (g.hint) { const t = document.createElement("span"); t.className = "guess-tag"; t.textContent = "hint"; w.appendChild(t); }
        const r = document.createElement("span"); r.className = "guess-rank"; r.textContent = g.rank;
        line.append(w, r);
        const bar = document.createElement("div"); bar.className = "bar"; bar.setAttribute("aria-hidden", "true");
        const cover = document.createElement("div"); cover.className = "bar-cover"; cover.style.width = (100 - closeness(g.rank)) + "%";
        bar.appendChild(cover);
        li.append(line, bar);
        gl.appendChild(li);
    }
    $("guesses-empty").hidden = guesses.length > 0;

    // hints history
    const hl = $("hints");
    hl.textContent = "";
    for (const h of hints) {
        const li = document.createElement("li");
        const n = document.createElement("span"); n.className = "gutter"; n.textContent = String(h.n).padStart(2, "0");
        const t = document.createElement("span"); t.textContent = h.text; if (h.gold) t.className = "hint-gold";
        li.append(n, t);
        hl.appendChild(li);
    }
    $("hints-empty").hidden = hints.length > 0;

    // a______d pattern next to the guess count
    $("pattern-wrap").hidden = !pattern;
    if (pattern) {
        const chars = Array(pattern.len).fill("_");
        if (pattern.first) chars[0] = pattern.first;
        if (pattern.last) chars[pattern.len - 1] = pattern.last;
        $("pattern").textContent = chars.join("");
        $("pattern").setAttribute("aria-label", pattern.len + " letters: " + chars.map(c => c === "_" ? "blank" : c).join(" "));
    }
}

function addGuess(word, rank, hint) {
    const old = guesses.find(g => g.word === word);
    if (old) old.hint = old.hint || hint;
    else guesses.push({ word, rank, hint });
    latest = word;
    return !old;
}

function handleHint(r) {
    if (r.error) return say(r.error, "error");
    $("tries").textContent = r.tries;
    const gold = r.kind === "word_at_rank";
    if (r.kind === "letter_count") pattern = { len: r.letters };
    if (r.kind === "first_letter" && pattern) pattern.first = r.letter;
    if (r.kind === "last_letter" && pattern) pattern.last = r.letter;
    if (gold) addGuess(r.word, r.rank, true);
    hints.push({ n: r.number, text: r.message, gold });
    say(r.message);
    render();
}

function handleExit(r, won) {
    finished = true;
    setControls();
    const box = $("result");
    box.textContent = "";
    const h = document.createElement("h3");
    h.textContent = won ? "You found it: " + r.secret : "The word was: " + r.secret;
    const sub = document.createElement("p"); sub.className = "empty"; sub.textContent = "the 20 closest words";
    const ol = document.createElement("ol");
    for (const t of r.top) {
        const li = document.createElement("li");
        if (t.you_were_here) li.className = "here";
        const n = document.createElement("span"); n.className = "gutter"; n.textContent = t.rank;
        li.append(n, t.word + (t.you_were_here ? "  <- you were here" : ""));
        ol.appendChild(li);
    }
    box.append(h, sub, ol);
    box.hidden = false;
}

$("form").addEventListener("submit", async e => {
    e.preventDefault();
    const raw = $("inp").value.trim();
    $("inp").value = "";
    if (!raw || !game || finished) return;
    // typed commands work like the terminal version; everything else is lowercased
    const word = ["HELP", "HINT", "EXIT", "RUSHIL"].includes(raw) ? raw : raw.toLowerCase();
    let r;
    try { r = await api("/api/guess", { game, word }); }
    catch (err) { return say("Can't reach the C++ server.", "error"); }

    if (r.error) return say(r.error, "error");
    if (r.type === "help") return say(r.lines.join(" "));
    if (r.type === "cheat") return say("secret: " + r.secret);
    if (r.type === "hint") return handleHint(r);
    if (r.type === "exit") { say(""); return handleExit(r, false); }

    $("tries").textContent = r.tries;
    if (r.status === "not_found") return say("\"" + word + "\" is not in the word list.", "error");
    if (r.status === "game_over") return say("This game is over. Start a new one.", "error");
    const isNew = addGuess(word, r.rank, false);
    render();
    if (r.status === "won") {
        say(r.message, "win");
        const end = await api("/api/giveup", { game });   // fetch the top-20 list for the end screen
        handleExit(end, true);
    } else {
        say(isNew ? "" : "\"" + word + "\" was already guessed (rank " + r.rank + ").");
    }
});

$("hint").addEventListener("click", async () => {
    if (!game || finished) return;
    try { handleHint(await api("/api/hint", { game })); }
    catch (err) { say("Can't reach the C++ server.", "error"); }
    $("inp").focus();
});

$("giveup").addEventListener("click", async () => {
    if (!game || finished) return;
    try { const r = await api("/api/giveup", { game }); say(""); handleExit(r, false); }
    catch (err) { say("Can't reach the C++ server.", "error"); }
});

$("new").addEventListener("click", async () => { await newGame(); $("inp").focus(); });

intro();
