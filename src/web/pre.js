/* Emscripten --pre-js for the DX Ball web build.
 *
 * The game reads its save files from the relative path "saves/" (SAVE_DIR in
 * src/config.h). Emscripten starts with the working directory at "/", so that
 * resolves to "/saves". MEMFS would satisfy every read and write but would be
 * thrown away on reload, so "/saves" is mounted as IDBFS instead, which is
 * backed by the browser's IndexedDB.
 *
 * IDBFS only reads from IndexedDB when FS.syncfs(true, ...) is called. That
 * has to finish before gameInit() reads settings and progress, so the load is
 * wrapped in a run dependency: Emscripten will not call main() until the
 * dependency is removed. Writes are flushed by storageSync() in
 * src/storage.c.
 */

var Module = typeof Module !== "undefined" ? Module : {};

Module["preRun"] = Module["preRun"] || [];
Module["preRun"].push(function () {
    var addDep = typeof addRunDependency !== "undefined"
        ? addRunDependency
        : Module["addRunDependency"];
    var removeDep = typeof removeRunDependency !== "undefined"
        ? removeRunDependency
        : Module["removeRunDependency"];

    var fs = typeof FS !== "undefined" ? FS : Module["FS"];
    var idbfs = typeof IDBFS !== "undefined" ? IDBFS : Module["IDBFS"];

    if (!fs || !idbfs) {
        console.error("dxball: FS/IDBFS unavailable; progress will not persist.");
        return;
    }

    try {
        fs.mkdir("/saves");
    } catch (e) {
        /* Already created (for example by a hot reload); nothing to do. */
    }

    try {
        fs.mount(idbfs, {}, "/saves");
    } catch (e) {
        console.error("dxball: could not mount IDBFS; progress will not persist.", e);
        return;
    }

    addDep("dxball-load-saves");
    fs.syncfs(true, function (err) {
        if (err) {
            /* A first visit has no IndexedDB store yet, and private-browsing
               modes can refuse it outright. Either way the game must still
               start, just with empty saves. */
            console.warn("dxball: could not read saved data from IndexedDB.", err);
        }
        removeDep("dxball-load-saves");
    });
});
