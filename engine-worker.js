// Runs the C++ engine (wasm/contexto.js) on a background thread so the page never freezes
// while it loads the word vectors or computes a ranking. script.js talks to it with messages.
try {
    importScripts("wasm/contexto.js");
} catch (e) {
    postMessage({ type: "failed", error: "could not load wasm/contexto.js: " + e.message });
    throw e;
}

const ready = createContexto().then(M => {
    if (!M.ccall("contexto_init", "number", ["string"], ["/vectors.bin"])) throw new Error("vectors.bin failed to load");
    return M;
});
ready.then(() => postMessage({ type: "ready" }),
           err => postMessage({ type: "failed", error: String(err && err.message || err) }));

onmessage = async e => {
    const { id, path, game, word } = e.data;
    try {
        const M = await ready;
        postMessage({ id, body: M.ccall("contexto_call", "string", ["string", "string", "string"], [path, game, word]) });
    } catch (err) {
        postMessage({ id, error: String(err && err.message || err) });
    }
};
