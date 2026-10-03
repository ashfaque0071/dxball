# DX Ball — Wizarding World

A wizarding-themed brick-breaker written in C99 with
[raylib](https://www.raylib.com/). The game includes seven levels, multiple
brick and power-up types, adjustable difficulty, keyboard or mouse paddle
control, resumable games, level selection, music, sound effects, and local
high-score tracking.

## Project team

- Ashifa J. Rahman — Student ID: 2505106
- Joyeeta Mitra — Student ID: 2505100

- **Department:** Computer Science and Engineering (CSE)
- **University:** Bangladesh University of Engineering and Technology (BUET)
- **Supervisor:** Anwarul Bashir Shuaib

> [!IMPORTANT]
> Build and run the game from the repository root. The game loads assets and
> save data with relative paths. The supplied scripts switch to the correct
> directory automatically.

## Dependencies

| Platform | Required software | Notes |
| --- | --- | --- |
| All builds | A C99 compiler and raylib | The bundled Windows files target raylib 5.5. The macOS build also compiles with raylib 6.0. |
| macOS | Xcode Command Line Tools, Homebrew, and the Homebrew `raylib` formula | `run.sh` links the Cocoa, OpenGL, IOKit, and CoreVideo system frameworks. |
| Windows | 64-bit MinGW-w64 GCC | The required raylib 5.5 headers and x86-64 static library are included in the repository. Visual C++ is not supported by the provided script. |
| Web (WebAssembly) | The Emscripten SDK, plus `make`, `curl`, and `zip` | `build_web.sh` downloads and builds raylib 5.5 for the web itself; see [Web build](#web-build-webassembly). |

Beyond the platform dependencies listed above, no additional C libraries,
environment variables, asset downloads, or asset-generation steps are
required. All images, fonts, music, and sound effects used by the game are
included in `assets/`. The web build is the one exception: it needs the
Emscripten SDK and fetches the raylib source once, into a cache directory that
is not part of the repository.

## Quick start

### macOS

Install the required developer tools and raylib:

```sh
xcode-select --install
brew install raylib
```

From the repository root, build and start the game:

```sh
./run.sh
```

The script finds Clang, Homebrew, and raylib, compiles the project to
`dxball`, and then starts it.

If macOS reports that `run.sh` is not executable, fix its permission once and
try again:

```sh
chmod +x run.sh
./run.sh
```

### Windows

1. Install a **64-bit MinGW-w64 GCC** toolchain.
2. Add the toolchain's `bin` directory to `PATH`.
3. Open Command Prompt or PowerShell in the repository root.
4. Double-click `build.bat`, or run the build script from a terminal:

   **Command Prompt**

   ```bat
   build.bat
   ```

   **PowerShell**

   ```powershell
   .\build.bat
   ```

The script builds `dxball.exe` and starts the game. The raylib headers and
64-bit MinGW static library required by the Windows build are already included
in `raylib\include\` and `raylib\lib\`.

## Web build (WebAssembly)

The same C sources also compile to WebAssembly with
[Emscripten](https://emscripten.org/), producing a build that runs in a browser
and can be uploaded to itch.io. The browser version is the same raylib game, not
a rewrite: it shares every source file with the desktop builds.

### Prerequisites

- The **Emscripten SDK** (emsdk). The build has been verified with Emscripten
  6.0.10.
- `make`, `curl`, and `zip`, which macOS and most Linux distributions already
  provide. On macOS they come with the Xcode Command Line Tools
  (`xcode-select --install`).
- **Recommended:** `ffmpeg` and `pngquant`, used to re-encode the artwork for
  the web (see [Download size](#download-size)). Without them the build still
  works, but ships the full-size originals and the download grows from about
  10 MB to about 70 MB.

  ```sh
  brew install ffmpeg pngquant oxipng        # macOS
  sudo apt install ffmpeg pngquant oxipng    # Debian / Ubuntu
  ```

Nothing else is needed. `build_web.sh` downloads the pinned raylib 5.5 source,
verifies its SHA-256 checksum, and builds it for the web itself. raylib 5.5 is
the same release already vendored in `raylib/` for the Windows build.

### Installing and activating Emscripten

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk
./emsdk install latest
./emsdk activate latest
. ./emsdk_env.sh
```

The last line activates the toolchain for the current shell only, so it must be
repeated in each new terminal. `build_web.sh` also activates the SDK itself if
it finds one at `$EMSDK`, `~/emsdk`, `/usr/local/emsdk`, or `/opt/emsdk`, so in
practice running the build script from a fresh shell is usually enough. If no
SDK is found, the script stops with installation instructions rather than a
compiler error.

### Building

From the repository root:

```sh
./build_web.sh
```

The script accepts two options:

| Option | Effect |
| --- | --- |
| `--clean` | Deletes the cached raylib download and build, then rebuilds it from scratch. |
| `--no-zip` | Builds `dist/web/` but skips creating the itch.io archive. |

### Output

| Path | Contents |
| --- | --- |
| `dist/web/` | The playable release: `index.html`, `index.js`, `index.wasm`, `index.data`, `deferred.data`, `deferred.json`, the bundled licences, and `MANIFEST.txt`. |
| `dist/dxball-web-itch.zip` | The same files packaged with `index.html` at the archive root, ready to upload to itch.io. |
| `build/web-deps/` | The cached raylib source and its web build. Safe to delete; it is re-created on demand. |

`index.data` holds everything needed for the menus and first chamber. The six
later chamber backgrounds and intro screens are packed in `deferred.data` and
download in the background while the game is already usable. `deferred.json`
lists the files and verifies the archive's checksum. `MANIFEST.txt`
lists each packaged file with its SHA-256 checksum, so the contents of an upload
can be verified later.

`build/` and `dist/` are generated directories and are listed in `.gitignore`
along with the native executables and the personal `saves/` directory. None of
them belong in version control.

### Download size

`assets/` weighs about 71 MB, which is far too much for a browser game: a
player would stare at the loading bar for minutes before the menu appeared.
Shipping it as-is is the single biggest thing that would stop anyone playing.

`tools/optimize_web_assets.sh` therefore builds a re-encoded copy into
`build/web-assets/`, and that copy is what gets packaged:

| Source | Shipped as | Why |
| --- | --- | --- |
| PNG with no transparency | JPEG, quality ~88 | About 6× smaller, including every level background. |
| Nearly opaque level intro PNG | JPEG, quality ~88 | The seven intro screens have no fully transparent pixels and only slight translucency; JPEG cuts their combined size by several MB. |
| Other PNG that uses alpha | PNG, palette-quantised | About 4× smaller with alpha intact, for sprite atlases and overlays. |
| WAV | OGG Vorbis | About 15× smaller. Vorbis rather than MP3 because MP3's encoder padding would put an audible gap at the loop point of the music. |
| Fonts, licences | copied | Already small. |

The result is **about 10 MB instead of 71 MB**. The intro screen change reduces
the browser download by about a quarter compared with the earlier 15 MB build.
The first playable screen needs about **5.6 MB of assets** plus the small code
files; the remaining **3.7 MB** downloads while the player uses the menus or
first chamber. If a returning player selects a later chamber before its art is
ready, the game shows download progress and waits there.
Two things make this safe:

- **The originals are never touched.** `assets/` keeps the full-quality
  artwork, so the macOS and Windows builds are byte-for-byte unaffected and
  nothing is lost from the repository. Only the browser copy is compressed.
- **No asset path changes.** `resolveAssetPath()` in `src/assets.c` looks for
  the original name first and falls back to the re-encoded extension, so the
  game still asks for `assets/backgrounds/level_1.png` either way.

The web build of raylib is compiled with `-DSUPPORT_FILEFORMAT_JPG`, which
raylib leaves out by default, so it can decode the JPEGs.

Emscripten's `--use-preload-cache` stores the startup bundle in the browser's
cache. The later chamber bundle uses a versioned URL so returning players can
reuse their cached download.

If `ffmpeg` or `pngquant` is missing the script says so and copies the files
through unchanged — the build still succeeds, it is just a much larger
download.

### Running it locally

The build must be served over HTTP. From the repository root:

```sh
python3 -m http.server 8000 --directory dist/web
```

Then open <http://localhost:8000/>.

**Opening `dist/web/index.html` by double-clicking it will not work.** A
double-clicked file loads through the `file://` scheme, and browsers refuse to
instantiate WebAssembly or fetch the `index.data` asset bundle from `file://`
for security reasons. The page will stall on the loading bar. Any static HTTP
server works; `python3 -m http.server` just happens to need no installation.

### Rebuilding after a source change

Run `./build_web.sh` again. The raylib dependency stays cached, so only the
game's own sources are recompiled and repackaged, which takes a few seconds.
Use `./build_web.sh --clean` only when you want to rebuild raylib itself.

Browsers cache `index.js` and `index.wasm` aggressively. After rebuilding,
reload the local page with cache bypass (<kbd>Shift</kbd>+<kbd>Reload</kbd>, or
<kbd>Cmd</kbd>/<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd>) or you may keep
testing the previous build.

### Differences in the browser version

- **No Quit button.** A web page cannot close itself, so the main menu shows
  four entries instead of five, and <kbd>Esc</kbd> on the main menu does
  nothing. Every other screen's <kbd>Esc</kbd> behaviour is unchanged.
- **Progress is saved automatically during play.** The desktop build writes a
  resumable save when you close the window; a browser tab can be closed or
  reloaded without warning, so the web build also writes one every five seconds
  while a run is in progress.
- Saved data lives in the browser rather than in `saves/`. See
  [Saved data in the browser](#saved-data-in-the-browser).

## Compiling without immediately running

Both provided scripts accept `--build-only` as their first argument.

### macOS

```sh
./run.sh --build-only
./dxball
```

### Windows Command Prompt

```bat
build.bat --build-only
dxball.exe
```

### Windows PowerShell

```powershell
.\build.bat --build-only
.\dxball.exe
```

## Controls

| Context | Input | Action |
| --- | --- | --- |
| Menus | Left click | Select menu items and on-screen controls |
| Main menu | `Enter` | Start or continue player setup |
| Menus | `Esc` | Go back; from the main menu, quit (desktop builds only) |
| Name entry | Letters and numbers | Enter a player name of up to 12 characters |
| Name entry | `Backspace` / `Enter` | Delete a character / confirm the name |
| Level intro | `Enter` or left click | Begin the level |
| Gameplay | Left/Right arrow keys | Move the paddle |
| Gameplay | Mouse movement | Move the paddle when Mouse Control is enabled |
| Gameplay | `Space` or left click | Launch the ball |
| Gameplay | `P` or `Esc` | Pause or resume the game |
| Paused or game over | `R` | Restart the current level |
| End screen | `M` | Return to the main menu |
| Developer shortcut | `Shift` + `L` | Clear the current level |

The objective is to break every destructible brick without running out of
lives. Catch dropped power-ups to gain effects such as a wider paddle,
multiple balls, extra speed, invincibility, or an extra life.

## Configuration and saved data

Always launch the executable **from the repository root**. Asset and save-file
paths are relative to the current working directory:

```text
assets/...
saves/settings.txt
saves/highscores.txt
saves/progress_<PLAYER>.txt
saves/unlocked_<PLAYER>.txt
saves/level_scores_<PLAYER>.txt
```

For example, use `./dxball` while your terminal is in the repository root.
Starting the executable with another working directory will prevent the game
from finding its images, fonts, and audio, and may create a separate `saves/`
directory in the wrong location.

The `saves/` directory must be writable if you want progress and settings to
persist. It is created automatically when missing. The game uses these files:

- `saves/settings.txt` stores sound, mouse-control, difficulty, ball-speed,
  paddle-speed, and starting-life preferences.
- `saves/highscores.txt` stores the global high-score table.
- `saves/progress_<PLAYER>.txt` stores a resumable game, including the level,
  score, lives, and remaining brick state.
- `saves/unlocked_<PLAYER>.txt` stores the highest unlocked level for a
  player.
- `saves/level_scores_<PLAYER>.txt` stores that player's best score for each
  level.

No manual configuration is required for a first run. Sound, mouse control,
ball speed, paddle speed, and difficulty are changed through the in-game
Settings screen and saved automatically. Player names are converted to
uppercase and may contain up to 12 ASCII letters or numbers.

The Settings screen's reset action restores the default options, clears the
current player's saved progress, unlocked levels, and per-level scores, and
clears the global high-score table.

### Saved data in the browser

The web build keeps exactly the same save files, with the same names and the
same formats. Instead of a `saves/` directory on disk, `saves/` is mounted as an
Emscripten [IDBFS](https://emscripten.org/docs/api_reference/Filesystem-API.html)
filesystem backed by the browser's **IndexedDB**, so settings, progress,
unlocked levels, per-level scores, and the high-score table all survive a reload
or a closed tab.

Mechanically: the mount is created and read back before `main()` runs, so the
game never reads an empty save directory; and every write or delete in
`src/storage.c` is followed by a flush to IndexedDB. The desktop builds are
unaffected — there the flush compiles to an empty function and writes go
straight to disk as before.

What this means in practice:

- **Scores are local to one browser on one device.** There is no server and no
  global leaderboard. The "high scores" table ranks the players created in
  *that* browser profile. The same game opened in a different browser, on a
  different machine, or in a private window starts with an empty save.
- **Clearing site data erases progress.** Clearing cookies and site data for the
  hosting origin, or using a private/incognito window, removes or disables the
  IndexedDB store. Some browsers also evict IndexedDB for rarely visited sites
  when disk space runs low.
- **The itch.io origin owns the data.** A game played on itch.io and the same
  build played from `localhost` are different origins and therefore keep
  separate saves.
- Your own `saves/` directory is **not** part of the web release. Only
  `assets/` is packaged, so published builds ship with no personal data.

## Project layout

```text
.
├── .github/workflows/  GitHub Pages build-and-deploy workflow
├── assets/             Fonts, backgrounds, sprites, UI images, and audio
├── build/              Generated: cached raylib source and web build (ignored)
├── dist/               Generated: web release and itch.io archive (ignored)
├── output/             Generated project documentation
├── raylib/
│   ├── include/        Bundled raylib 5.5 headers for Windows
│   └── lib/            Bundled 64-bit Windows raylib libraries
├── saves/              Player progress, unlocks, scores, and settings (ignored)
├── src/                Game source and header files
│   └── web/            Web shell page and Emscripten glue (web build only)
├── tools/
│   └── optimize_web_assets.sh   Re-encodes assets/ for the web build
├── build.bat           Windows build-and-run script
├── build_web.sh        WebAssembly build and itch.io packaging script
└── run.sh              macOS build-and-run script
```

Every current `.c` file in `src/` is listed in all three build scripts. New
source files must be added to the compile command in `run.sh`, `build.bat`, and
`build_web.sh`; `build_web.sh` fails with a clear message if it finds a `.c`
file in `src/` that it does not know about.

The files under `src/web/` are used only by the WebAssembly build and are not
compiled into the desktop builds:

- `src/web/shell.html`: the page that hosts the canvas — responsive 4:3 layout,
  loading indicator, fullscreen button, and control hint;
- `src/web/pre.js`: mounts `saves/` on IndexedDB and loads it before `main()`;
  and
- `src/web/library_dxball.js`: the one native function the game calls to flush
  saves to IndexedDB.

Key modules include:

- `src/main.c`: application startup, the single-frame update-and-render
  function, the desktop and browser main loops, and shutdown;
- `src/game.c` and `src/gameplay.c`: game state, transitions, and frame
  updates;
- `src/input.c`: keyboard and mouse input;
- `src/render.c`, `src/menus.c`, and `src/hud.c`: rendering and user
  interface;
- `src/bricks.c`, `src/ball.c`, `src/levels.c`, and `src/powerups.c`: core
  game systems;
- `src/assets.c` and `src/audio.c`: resource loading, playback, and cleanup;
  and
- `src/storage.c`: settings, progress, level unlocks, and score persistence.

## Troubleshooting

### `clang is required` on macOS

Install or repair the Xcode Command Line Tools:

```sh
xcode-select --install
```

### `Homebrew is required` or `raylib is not installed` on macOS

Install [Homebrew](https://brew.sh/) if necessary, then install raylib:

```sh
brew install raylib
```

### `gcc was not found on PATH` on Windows

Install a 64-bit MinGW-w64 GCC distribution and ensure its `bin` directory is
on `PATH`. Open a new terminal and verify it with:

```bat
gcc --version
```

### `raylib\lib\libraylib.a` is missing on Windows

Restore the bundled `raylib/` directory. The provided build script requires
`raylib\include\raylib.h` and `raylib\lib\libraylib.a`.

### The window opens but assets or audio are missing

Close the game, change to the repository root, and launch the executable from
there. The working directory must contain `assets/`; the game creates
`saves/` automatically when needed.

### Progress or settings do not persist

Check that your user account can write to the `saves/` directory. Also confirm
that the game is being launched from the repository root rather than another
working directory.

### `Emscripten (emcc) was not found`

The web build needs an activated Emscripten SDK. Install it and run
`. ~/emsdk/emsdk_env.sh` in the current shell, as described under
[Web build](#web-build-webassembly). The script searches `$EMSDK`, `~/emsdk`,
`/usr/local/emsdk`, and `/opt/emsdk` automatically.

### The web page stays on the loading bar

Most often the page was opened by double-clicking `index.html`. Serve it over
HTTP instead — see [Running it locally](#running-it-locally). If it is already
being served, open the browser's developer console: a failed `index.data`
request means the asset bundle is missing from the directory being served.

### The browser build has no sound until I click

Browsers refuse to start audio until the player interacts with the page. This is
expected: the game loads and renders normally, and music and effects begin on
the first click or key press. Nothing is lost, and the Sound setting keeps
working as usual.

### The web build still shows my previous changes

The browser is serving a cached `index.js` or `index.wasm`. Reload with cache
bypass (<kbd>Cmd</kbd>/<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd>).

### `raylib 5.5 checksum mismatch`

The download was incomplete or corrupted. The script deletes the bad archive
itself, so simply run `./build_web.sh` again.

### The game has no sound

Check that Sound is enabled in Settings and that the operating system has an
active audio output device. Music and sound-effect files are loaded from
`assets/sounds/`.

## Publishing to itch.io

`dist/dxball-web-itch.zip` is a complete, directly uploadable itch.io HTML game.
Uploading and publishing are manual steps — nothing in this repository logs in
to or posts to itch.io on your behalf.

1. **Create or edit the project.** Sign in to itch.io and go to
   *Dashboard → Create new project*, or open an existing project to edit it.
2. **Select "HTML Game".** Set *Kind of project* to **HTML**. This is what makes
   the *"This file will be played in the browser"* option available later.
3. **Upload the archive.** Under *Uploads*, choose
   `dist/dxball-web-itch.zip`. The archive is about 10 MB, which is within
   itch.io's limits for a browser game; the upload itself may take a while.
4. **Mark it playable in the browser.** Tick **"This file will be played in the
   browser"** on the uploaded file. itch.io serves `index.html` from the archive
   root, which is why the ZIP must not contain a wrapping folder — it does not.
5. **Configure a 4:3 embed size.** Under *Embed options*, set the viewport to a
   4:3 size such as **960 × 720** (or 800 × 600 / 1024 × 768). The page scales
   the canvas to fit its container and preserves the aspect ratio, so a 4:3
   frame leaves no bars.
6. **Enable fullscreen.** Tick **"Fullscreen button"**. The page also provides
   its own fullscreen button beneath the canvas, so either works. Leaving
   *"Mobile friendly"* off is reasonable: the game needs a keyboard or a mouse.
7. **Save as a draft first.** Set *Visibility & access* to **Draft** (or
   **Restricted**) and save. Do not set it to Public yet.
8. **Test the uploaded build before publishing.** Open the draft page and
   confirm that it loads to the main menu, that the paddle responds to the
   arrow keys and the mouse, that sound starts after the first click, and that
   a setting survives a page reload. Only then set the project to Public.

A good embed description is worth adding: mention that the game is played with
the mouse or arrow keys, and that **high scores are stored locally in the
player's own browser** rather than on a server.

## Hosting on GitHub Pages

GitHub Pages can host the same `dist/web/` build for free, which is handy for
sharing a playable link while developing. The build needs no changes: every
path it requests is relative, so it works both at a domain root
(`https://you.github.io/`) and in the subdirectory a project site uses
(`https://you.github.io/dxball/`). GitHub Pages also serves `.wasm` with the
`application/wasm` content type that browsers require, and the game uses no
threads, so none of the cross-origin isolation headers are needed.

Read [Which host to use](#which-host-to-use) before choosing this over itch.io
for a public release.

### Putting the project on GitHub

The repository is not a Git repository yet. From the repository root:

```sh
git init
git add .
git commit -m "DX Ball with WebAssembly build"
git branch -M main
git remote add origin https://github.com/<you>/<repo>.git
git push -u origin main
```

`.gitignore` already excludes the generated directories (`build/`, `dist/`),
the native executables, your personal `saves/` data, and `.DS_Store`, so none
of those are committed. The `assets/` directory is about 71 MB and *is*
committed — that is fine for GitHub, whose per-file limit is 100 MB and whose
largest asset here is about 3 MB.

### Option A — build and deploy automatically (recommended)

`.github/workflows/deploy-pages.yml` is included and ready to use. On every
push to `main` it installs the pinned Emscripten SDK, runs `./build_web.sh
--no-zip`, and publishes `dist/web/` to Pages. The Emscripten SDK and the
raylib build are cached between runs, so only the first run is slow.

Enable it once, in the repository on github.com:

1. **Settings → Pages → Build and deployment → Source: "GitHub Actions"**.
2. Push to `main`, or re-run the latest workflow from the **Actions** tab.
3. The finished run prints the published URL, which is
   `https://<you>.github.io/<repo>/` for a project repository.

Step 1 cannot be automated from inside the workflow. Creating a Pages site calls
`POST /repos/{owner}/{repo}/pages`, which needs admin rights on the repository,
and the `GITHUB_TOKEN` a workflow runs with never has them — so
`actions/configure-pages` with `enablement: true` fails on a repository where
Pages has never been switched on. It has to be done once in the web UI.

Pages is free on public repositories. On a private repository it requires a plan
that includes Pages; if the repository is private and the deploy step fails,
that is the usual reason.

Nothing generated is ever committed with this option: the build output goes
straight to Pages as an artifact, so the repository stays at roughly the size
of `assets/` no matter how many times you deploy.

To move to a newer Emscripten later, change the version in both the cache key
and the install step of the workflow.

### Option B — commit the build by hand

If you would rather not use Actions, build locally and commit the output into a
`docs/` directory, which GitHub Pages can serve directly:

```sh
./build_web.sh --no-zip
rm -rf docs
mkdir docs
cp -R dist/web/. docs/
git add docs
git commit -m "Deploy web build"
git push
```

Then set **Settings → Pages → Build and deployment → Source: "Deploy from a
branch" → Branch: `main`, folder: `/docs`**, and repeat the commands above
after every rebuild.

Be aware of what this costs. `index.data` and `deferred.data` total about 10 MB,
so **each** deployment adds another ~10 MB to the repository's history,
permanently — Git keeps every
version, and the only way to reclaim that space later is a history rewrite.
Large generated files can still make repository history grow quickly.
Option A avoids all of it, which is why it is the recommended one.

### Which host to use

| | itch.io | GitHub Pages |
| --- | --- | --- |
| Built for games | Yes — the embed, fullscreen button, and page are provided | No — it is a static site host |
| Suits an ~10 MB game | Yes | Yes |
| Bandwidth | Intended for game downloads | ~100 GB/month soft limit, about 9,000 full loads of this build |
| Site size | Generous | 1 GB soft limit |
| Best for | The public release | Dev builds, previews, and sharing a link while iterating |

Use **both** if you like — they are independent. Note that saved data is tied
to the origin, so a player's progress on `you.github.io` and on `itch.io` are
separate, exactly as described under
[Saved data in the browser](#saved-data-in-the-browser).

## Licensing and release hygiene

### What is already covered

- **raylib** is distributed under the zlib/libpng licence. `raylib_LICENSE.txt`
  is in the repository root and is copied into every web release as
  `raylib_LICENSE.txt`, next to `index.html`, so the licence travels with the
  binary it is linked into.
- **Cinzel** (copyright 2020 The Cinzel Project Authors) and **Cinzel
  Decorative** (copyright 2012 Natanael Gama, with Reserved Font Name "Cinzel"),
  both in `assets/fonts/`, are licensed under the **SIL Open Font License,
  Version 1.1**. Their licence files, `OFL-Cinzel.txt` and
  `OFL-CinzelDecorative.txt`, are in `assets/fonts/` and are also copied into
  each web release. The OFL permits bundling and redistribution with a work,
  including commercially, as long as the licence text travels with the fonts and
  the fonts are not sold on their own. The Reserved Font Name means a *modified*
  version of the font may not keep the "Cinzel" name; the fonts here are
  unmodified.

### Release checklist

The checklist below is a practical reminder, not legal advice. Work through it
before making an itch.io page public.

- [ ] **Artwork.** Confirm you hold redistribution rights for every image in
      `assets/backgrounds/`, `assets/sprites/`, and `assets/ui/`. For anything
      commissioned, generated, purchased, or downloaded, record where it came
      from and what its licence actually permits — in particular whether it
      allows *public distribution*, and whether it allows *commercial* use if
      you intend to charge for the game or accept donations.
- [ ] **Sounds and music.** Do the same for every file in `assets/sounds/`.
      Royalty-free and Creative Commons tracks frequently require attribution;
      if so, add the required credit to the itch.io page and the in-game credits
      screen.
- [ ] **Fonts.** Keep `OFL-Cinzel.txt` and `OFL-CinzelDecorative.txt` in the
      release. Both are packaged automatically; do not strip them.
- [ ] **Names and trademarks.** The game's level names, UI text, and artwork
      draw on the Wizarding World / Harry Potter setting. Those names,
      characters, house crests, and related imagery are protected by copyright
      and trademark held by their respective owners. Publishing a game that uses
      them — especially one that is monetised, or that uses the names in its
      title, cover art, or store tags — can draw a takedown or a legal
      complaint regardless of intent.
- [ ] **Do not rely on a disclaimer.** A "this is a fan project, not affiliated
      with…" notice does **not** by itself grant permission, create fair use or
      fair dealing, or resolve a copyright or trademark problem. It is a
      courtesy, not a defence. If you intend to publish publicly — and
      especially if money is involved — get the rights, obtain permission, or
      replace the protected material. Consider advice from someone qualified in
      your jurisdiction if you are unsure.
- [ ] **Project metadata.** Check that the itch.io title, tags, cover image, and
      description reflect the same decisions you reached above.
- [ ] **Package contents.** Open `dist/dxball-web-itch.zip` (or read
      `MANIFEST.txt`) and confirm it contains only the ten generated release
      files — no source code, no desktop executables, no development libraries,
      and no personal `saves/` data.
