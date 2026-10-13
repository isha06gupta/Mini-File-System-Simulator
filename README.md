# MiniFS Learning Lab


**[Live Demo](https://isha06gupta.github.io/Mini-File-System-Simulator/) · [Source Code](https://github.com/isha06gupta/Mini-File-System-Simulator)**

> Try the deployed MiniFS Learning Lab in your browser. The C/WebAssembly lab requires its deployment build to complete successfully.


An interactive, browser-based learning environment for file-system commands and Operating Systems concepts. MiniFS Learning Lab combines a virtual filesystem simulator with guided practice, visual explanations, quizzes, and a separate C engine compiled to WebAssembly.

## Contents

- [What the project includes](#what-the-project-includes)
- [Technology stack](#technology-stack)
- [Run locally](#run-locally)
- [How to use the lab](#how-to-use-the-lab)
- [Testing](#testing)
- [Build the C/WebAssembly lab](#build-the-cwebassembly-lab)
- [Deploy to GitHub Pages](#deploy-to-github-pages)
- [Docker](#docker)
- [Project structure](#project-structure)
- [Architecture and limitations](#architecture-and-limitations)
- [Privacy and persistence](#privacy-and-persistence)
- [Troubleshooting](#troubleshooting)

## What the project includes

### 1. Filesystem Simulator

- Terminal-style command input with command history and autocomplete.
- Interactive file explorer and hierarchical directory tree.
- Virtual file and directory creation, deletion, copying, moving, renaming, and navigation.
- File contents, permissions, storage information, quota controls, and filesystem diagnostics.
- Sample workspace and reset functionality.
- Step mode and contextual “Learn as you go” explanations.
- Copy terminal output and export terminal history.
- Resizable File Explorer and terminal panels, plus panel expansion controls.
- Light and dark themes with saved preferences.

The browser simulator operates on a virtual filesystem in the browser. It does not read, modify, or delete files on the user's computer.

### 2. Command Guide

- Command syntax, descriptions, examples, categories, and complexity notes.
- Search across the command reference.
- Syntax-highlighted command examples.
- Selected excerpts from the native C source with explanations.

Complexity labels are educational guidance for the illustrated operation, not a guarantee of the performance of a real operating system.

### 3. Practice Missions

- Guided tasks with suggested commands and hints.
- Automatic validation against the current virtual filesystem.
- Saved completion progress in browser local storage.

### 4. Concepts & Quiz

- Filesystem metrics and directory-tree traversal visualization.
- An illustrative pointer/tree representation.
- OS concepts quiz with explanations and saved answers.

### 5. C Engine Lab (WebAssembly)

The separate **MiniFS C Engine Lab** runs the C implementation compiled to WebAssembly. Commands entered on that page are sent to the C engine, and the displayed directory tree is exported from the C engine.

This is a separate runtime from the main JavaScript simulator. The two engines should not be assumed to have identical command coverage or behavior.

## Technology stack

- **Frontend:** HTML, CSS, JavaScript.
- **Main browser simulator:** JavaScript virtual filesystem engine.
- **C engine:** C (`mini_fs.c`) and a WebAssembly adapter (`wasm/minifs_wasm.c`).
- **WebAssembly build:** Emscripten (`emcc`).
- **Local development server:** Node.js built-ins; no external npm runtime dependencies.
- **Quality checks:** Node.js tests and GCC-based native C/API smoke checks.
- **Deployment:** GitHub Actions and GitHub Pages.
- **Optional container:** Nginx via Docker.

## Requirements

For local use:
- A modern browser.
- Node.js 18 or newer if using the included development server.

For the complete automated test suite:
- Node.js 18 or newer.
- GCC available on `PATH` for native C/API smoke checks.

For rebuilding WebAssembly locally:
- Emscripten installed and `emcc` available on `PATH`.

You do not need to run `npm install` to use the project or run its tests because it has no external Node.js package dependencies.

## Run locally

1. Clone the repository or download and extract the project.
2. Open the `MiniFS-Learning-Lab` project folder in a terminal.
3. Start the local server:

   ```bash
   npm run serve
   ```

4. Open [http://127.0.0.1:8000](http://127.0.0.1:8000).
5. Stop the server with `Ctrl+C`.

To use a different port:

**PowerShell**
```powershell
$env:PORT=8080
npm run serve
```

**macOS/Linux**
```bash
PORT=8080 npm run serve
```

Opening `web/index.html` directly may work for the main page, but using the local server is recommended, particularly for WebAssembly loading.

## How to use the lab

### Main Simulator

Try these commands in the terminal:

```text
help
pwd
ls
tree
mkdir projects
touch projects/notes.txt
write projects/notes.txt Hello MiniFS
read projects/notes.txt
```

Use the File Explorer to browse the virtual directory tree. The command guide, practice missions, and concepts quiz are available from the sidebar.

### C Engine Lab

Open the **C Engine Lab** link from the main page or visit `/wasm-lab.html` on the locally served site. The page enables its command controls when the WebAssembly module loads successfully.

Try:

```text
help
mkdir docs
touch docs/notes.txt
tree
fsck
```

## Testing

Run the full project test suite from the repository root:

```bash
npm test
```

The suite includes JavaScript syntax checks, filesystem engine assertions, UI/static smoke checks, quality checks, and C API smoke tests. The C API smoke test requires GCC.

The project also includes a native C syntax check:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -fsyntax-only mini_fs.c
```

To compile and run the native terminal program:

**Windows (PowerShell with GCC installed)**
```powershell
gcc -std=c11 -Wall -Wextra -o mini_fs.exe mini_fs.c
.\mini_fs.exe
```

**macOS/Linux**
```bash
gcc -std=c11 -Wall -Wextra -o mini_fs mini_fs.c
./mini_fs
```

The main browser simulator, native C program, and WebAssembly C lab are separate execution targets. Passing one target's tests does not automatically prove behavioral parity across all three.

## Build the C/WebAssembly lab

The repository includes the C adapter and build scripts. To rebuild the generated WebAssembly assets:

1. Install Emscripten following the [official installation guide](https://emscripten.org/docs/getting_started/downloads.html).
2. Open a terminal where `emcc` is available.
3. From the project root, run:

   ```bash
   npm run build:wasm
   ```

4. Confirm these files were generated:

   - `web/minifs-c.js`
   - `web/minifs-c.wasm`

5. Run `npm run serve` and open `/wasm-lab.html`.

The generated Emscripten artifacts are ignored by `.gitignore` to avoid accidentally committing build output. The GitHub Pages workflow builds these artifacts during deployment before publishing the `web/` directory.

## Deploy to GitHub Pages

The repository includes `.github/workflows/pages.yml`, which builds the WebAssembly assets and publishes the static `web/` directory.

1. Push the repository to GitHub using the `main` branch.
2. Open **Repository → Settings → Pages**.
3. Set the deployment source to **GitHub Actions**.
4. Ensure the workflow file is committed.
5. Open the **Actions** tab and check the **Deploy MiniFS Learning Lab to GitHub Pages** workflow. You can run it manually or trigger it by pushing a change to a configured project path on `main`.
6. Wait for both the build and deployment jobs to succeed.
7. Open the published URL shown in the workflow's deployment environment.

The site uses relative asset paths so it can be hosted under a repository subpath. The main simulator remains available if WebAssembly fails to initialize; the C Engine Lab reports the failure instead of silently switching to the JavaScript engine.

## Docker

Docker is optional. From the project root:

```bash
docker build -t minifs-learning-lab .
docker run --rm -p 8080:80 minifs-learning-lab
```

Open [http://127.0.0.1:8080](http://127.0.0.1:8080).

The Docker image serves the existing `web/` directory through Nginx. It does not compile the C/WebAssembly assets as part of the Docker build, so build the WebAssembly files first if you want the C Engine Lab included in the container.

## Project structure

```text
MiniFS-Learning-Lab/
├── .github/
│   └── workflows/
│       ├── ci.yml                # Automated quality checks
│       └── pages.yml             # Build and deploy to GitHub Pages
├── scripts/
│   ├── serve.cjs                 # Local static server
│   ├── benchmark.cjs             # Informational JS-engine benchmark
│   ├── build-wasm.cjs            # Cross-platform WebAssembly build entry
│   ├── build-wasm.sh             # macOS/Linux build script
│   └── build-wasm.ps1            # Windows build script
├── tests/
│   ├── engine.test.cjs
│   ├── phase2-smoke.test.cjs
│   ├── phase3-quality.test.cjs
│   ├── phase4-quality.test.cjs
│   ├── run-wasm-api.cjs
│   └── wasm-api-smoke.c
├── wasm/
│   ├── minifs_wasm.c              # C-to-WebAssembly adapter
│   └── README.md
├── web/
│   ├── index.html                 # Main learning lab
│   ├── styles.css
│   ├── design-system.css
│   ├── theme.js
│   ├── engine.js                  # Main JavaScript filesystem engine
│   ├── app.js                     # Main UI and learning interactions
│   ├── syntax-highlight.js
│   ├── ui-enhancements.css
│   ├── ui-enhancements.js
│   ├── wasm-lab.html               # Separate C/WebAssembly lab
│   ├── wasm-lab.css
│   ├── wasm-lab.js
│   ├── minifs-c.js                 # Generated Emscripten loader (build output)
│   └── minifs-c.wasm               # Generated WebAssembly binary (build output)
├── mini_fs.c                       # Native C filesystem implementation
├── ARCHITECTURE.md
├── Dockerfile
├── package.json
└── README.md
```

## Architecture and limitations

- **Main simulator:** runs `web/engine.js`, a JavaScript teaching implementation.
- **C Engine Lab:** runs the C implementation through WebAssembly and is kept separate from the main simulator.
- **Behavior differences:** the JavaScript and C implementations may support different commands and semantics. Consult each engine's command help.
- **Illustrations:** some diagrams explain concepts visually and should not be interpreted as instruction-level traces of native OS behavior.
- **Benchmark:** `npm run benchmark` measures the JavaScript engine in Node.js; it is informational and is not a native C/WebAssembly benchmark.
- **Static hosting:** GitHub Pages and the included Nginx container host static assets; there is no server-side account or cloud database layer.

## Privacy and persistence

- Files and directories created in the main simulator belong to the browser-based virtual filesystem, not the user's real disk.
- Theme selection, quiz answers, mission progress, and virtual filesystem state use browser local storage where implemented.
- Local storage is specific to the browser and origin. It is not synchronized to an account or another device.
- Clearing browser site data may remove saved progress and simulator state.

## Troubleshooting

### The local server does not start

- Confirm Node.js 18+ is installed with `node --version`.
- Run `npm run serve` from the project root, not from the `web/` folder.
- If port 8000 is busy, set `PORT=8080` as shown above.

### The C Engine Lab says WebAssembly is missing

- Run `npm run build:wasm` in a terminal where Emscripten is activated.
- Confirm `web/minifs-c.js` and `web/minifs-c.wasm` exist.
- Restart the local server and reload `/wasm-lab.html`.
- On GitHub Pages, inspect the Pages workflow build logs and confirm the WebAssembly build step succeeded.

### Tests fail at the C API smoke test

- Ensure GCC is installed and available on `PATH`.
- Check that `gcc --version` works in the same terminal used to run `npm test`.

## Contributing

Issues and improvements are welcome. Before submitting a change, run `npm test` and, when modifying the C implementation or adapter, verify the native C checks and WebAssembly build as applicable.

## License

No license file is currently included in this repository. Unless a license is added, reuse and redistribution are subject to the default copyright rules. Add a `LICENSE` file if you want to grant explicit open-source permissions.
