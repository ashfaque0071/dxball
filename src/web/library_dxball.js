/* Emscripten --js-library for the DX Ball web build.
 *
 * Exposes one native function, dxball_syncfs(), to C. It is declared in
 * src/storage.c and called by storageSync() after every save-file write or
 * delete, which is what pushes the IDBFS mount at /saves into the browser's
 * IndexedDB.
 *
 * This lives in a JS library rather than an inline EM_ASM block so that the
 * web build can stay on -std=c99 (EM_ASM requires a -std=gnu* mode) and so
 * that no eval()-based emscripten_run_script() call is needed.
 */

addToLibrary({
    dxball_syncfs__deps: ["$FS"],
    dxball_syncfs: function () {
        try {
            FS.syncfs(false, function (err) {
                if (err) {
                    console.error("dxball: saving to IndexedDB failed:", err);
                }
            });
        } catch (e) {
            console.error("dxball: FS.syncfs() threw:", e);
        }
    }
});
