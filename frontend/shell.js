// iideck shell.
//
// The Go side owns the catalog, the controller device and the artwork routes.
// This file owns the grid, navigation between tiles, and rendering.
//
// Navigation is a spatial search over tile geometry rather than an index
// arithmetic, so mixed square and multi-column tiles move focus correctly
// whichever layout the catalog produced.

// Wails injects the Go bindings on window.go before this script runs.
const go = window.go.app.App;

const grid = document.getElementById("grid");
const dots = document.getElementById("dots");
const activeTitle = document.getElementById("active-title");
const statusLine = document.getElementById("status");
const clockLine = document.getElementById("clock");
const emptyNote = document.getElementById("empty");
const toast = document.getElementById("toast");

const state = {
    games: [],
    /** tiles currently rendered, in reading order */
    tiles: [],
    focus: 0,
    page: 0,
    pages: 1,
    /** tile indices belonging to the visible page */
    visible: [],
};

/** The shell's own keyboard handling, for the brief window before a pad appears. */
const keys = new Set();

/* ---------------------------------------------------------------- catalog */

async function loadCatalog() {
    try {
        const games = await go.Games();
        render(games || []);
    } catch (err) {
        statusLine.textContent = "Could not read the library";
        showToast(String(err), true);
    }
}

/* ---------------------------------------------------------------- rendering */

/** Grid shape follows the panel width, so the layout holds on any display. */
function columnCount() {
    const width = window.innerWidth;
    if (width < 900) return 4;
    if (width < 1400) return 6;
    if (width < 1800) return 7;
    return 9;
}

function render(games) {
    state.games = games;
    document.documentElement.style.setProperty("--cols", String(columnCount()));

    grid.replaceChildren();
    state.tiles = games.map((game) => buildTile(game));
    for (const tile of state.tiles) {
        grid.append(tile);
    }

    emptyNote.hidden = games.length > 0;
    state.focus = 0;
    paginate();
    applyFocus();
    updateActiveTitle();
    statusLine.textContent = describeLibrary(games);
}

function describeLibrary(games) {
    if (games.length === 0) return "No games found";
    const installed = games.filter((g) => g.installed).length;
    const stores = new Set(games.map((g) => g.source));
    const label = installed === games.length ? `${games.length} games` : `${installed} of ${games.length} installed`;
    return `${label} · ${[...stores].join(" · ")}`;
}

function buildTile(game) {
    const tile = document.createElement("div");
    tile.className = `tile ${game.size || "square"}`;
    tile.setAttribute("role", "listitem");
    if (!game.installed) {
        tile.classList.add("uninstalled");
    }

    const art = game.artworkWide && game.size !== "square" ? game.artworkWide : game.artwork;
    if (art) {
        const img = document.createElement("img");
        img.src = art;
        img.alt = game.title;
        // Artwork that fails to load falls back to the generated card rather than
        // leaving a broken image in the grid.
        img.addEventListener("error", () => {
            img.remove();
            tile.prepend(placeholder(game));
        });
        tile.append(img);
    } else {
        tile.append(placeholder(game));
    }

    if (game.badge) {
        const badge = document.createElement("span");
        badge.className = "badge";
        badge.textContent = game.badge;
        tile.append(badge);
    }

    const caption = document.createElement("div");
    caption.className = "caption";
    caption.textContent = game.title;
    tile.append(caption);

    if (game.hint) {
        const hint = document.createElement("span");
        hint.className = "hint";
        hint.textContent = game.hint;
        tile.append(hint);
    }

    tile.addEventListener("click", () => {
        const index = state.tiles.indexOf(tile);
        if (index >= 0) {
            state.focus = index;
            applyFocus();
            launch();
        }
    });

    return tile;
}

/** A generated card, so a title without artwork still reads as a tile. */
function placeholder(game) {
    const box = document.createElement("div");
    box.className = "placeholder";
    box.textContent = game.title;
    // Hue from the title, so the same game always gets the same colour.
    let hash = 0;
    for (const ch of game.title) {
        hash = (hash * 31 + ch.codePointAt(0)) % 360;
    }
    box.style.background = `linear-gradient(150deg, hsl(${hash} 62% 62%), hsl(${(hash + 48) % 360} 58% 44%))`;
    return box;
}

/* ---------------------------------------------------------------- paging */

/** Rows available on one page. */
function pageCapacity() {
    const cols = columnCount();
    const rows = window.innerHeight < 620 ? 2 : window.innerHeight < 900 ? 3 : 4;
    return cols * rows;
}

function paginate() {
    // Tiles are laid out in reading order; the visible window is one page of
    // them. Wide and hero tiles occupy more space, so a page holds fewer of
    // those; measuring after layout would be more precise but the grid is
    // already ordered by size, with the featured tiles first.
    const capacity = pageCapacity();
    const pages = [];
    let current = [];
    let used = 0;
    for (const [index, game] of state.games.entries()) {
        const cost = game.size === "hero" ? 4 : game.size === "wide" ? 2 : 1;
        if (used + cost > capacity && current.length > 0) {
            pages.push(current);
            current = [];
            used = 0;
        }
        current.push(index);
        used += cost;
    }
    if (current.length > 0) {
        pages.push(current);
    }
    state.pages = Math.max(pages.length, 1);
    state.page = Math.min(state.page, state.pages - 1);
    state.pageIndex = pages.length > 0 ? pages : [[]];
    renderDots();
    applyPage();
}

function renderDots() {
    dots.replaceChildren();
    for (let i = 0; i < state.pages; i += 1) {
        const dot = document.createElement("span");
        dot.className = i === state.page ? "dot active" : "dot";
        dots.append(dot);
    }
}

function applyPage() {
    const visible = new Set(state.pageIndex[state.page] || []);
    state.visible = [...visible];
    for (const [index, tile] of state.tiles.entries()) {
        tile.hidden = !visible.has(index);
    }
    if (state.visible.length > 0 && !visible.has(state.focus)) {
        state.focus = state.visible[0];
    }
}

function movePage(delta) {
    const next = state.page + delta;
    if (next < 0 || next >= state.pages) {
        return false;
    }
    state.page = next;
    renderDots();
    applyPage();
    applyFocus();
    updateActiveTitle();
    return true;
}

/* ---------------------------------------------------------------- focus */

/** applyFocus marks the focused tile. */
function applyFocus() {
    for (const [index, tile] of state.tiles.entries()) {
        tile.classList.toggle("focused", index === state.focus);
    }
    const tile = state.tiles[state.focus];
    if (tile) {
        tile.scrollIntoView({ block: "nearest", inline: "nearest" });
    }
}

function updateActiveTitle() {
    const game = state.games[state.focus];
    activeTitle.textContent = game ? game.title : "iideck";
}

/**
 * moveFocus moves focus in a direction using tile geometry, so a hero tile and a
 * square tile are both handled correctly.
 */
function moveFocus(dx, dy) {
    const from = state.tiles[state.focus];
    if (!from) {
        return;
    }
    const origin = from.getBoundingClientRect();
    const originX = origin.left + origin.width / 2;
    const originY = origin.top + origin.height / 2;

    let best = null;
    let bestScore = Infinity;
    for (const index of state.visible) {
        if (index === state.focus) {
            continue;
        }
        const box = state.tiles[index].getBoundingClientRect();
        const dxp = box.left + box.width / 2 - originX;
        const dyp = box.top + box.height / 2 - originY;

        // Movement must be in the pressed direction. A tile that overlaps the
        // origin on an axis is not a candidate along it.
        if (dx > 0 && dxp <= 0) continue;
        if (dx < 0 && dxp >= 0) continue;
        if (dy > 0 && dyp <= 0) continue;
        if (dy < 0 && dyp >= 0) continue;

        // Prefer the nearest tile along the axis, then the closest across it.
        const along = Math.abs(dx) > 0 ? Math.abs(dxp) : Math.abs(dyp);
        const across = Math.abs(dx) > 0 ? Math.abs(dyp) : Math.abs(dxp);
        const score = along + across * 3;
        if (score < bestScore) {
            bestScore = score;
            best = index;
        }
    }
    if (best === null) {
        // At the edge of the page, turn the page instead.
        if (dx > 0) movePage(1);
        else if (dx < 0) movePage(-1);
        else if (dy > 0) movePage(1);
        else if (dy < 0) movePage(-1);
        return;
    }
    state.focus = best;
    applyFocus();
    updateActiveTitle();
}

/* ---------------------------------------------------------------- actions */

async function launch() {
    const game = state.games[state.focus];
    if (!game) {
        return;
    }
    const program = game.launch && game.launch.program;
    if (!program) {
        showToast(`No emulator configured for ${game.source}`, true);
        return;
    }
    try {
        await go.Launch(game.id);
    } catch (err) {
        showToast(String(err), true);
    }
}

async function refresh() {
    statusLine.textContent = "Refreshing…";
    try {
        await go.Refresh();
        await loadCatalog();
    } catch (err) {
        showToast(String(err), true);
    }
}

async function showDetails() {
    const game = state.games[state.focus];
    if (!game) {
        return;
    }
    const minutes = game.playtimeMinutes || 0;
    const played = minutes > 0 ? `${Math.round(minutes / 60)} h played` : "never played";
    const last = game.lastPlayed ? new Date(game.lastPlayed).toLocaleDateString() : "";
    showToast(`${game.title} · ${game.source} · ${played}${last ? ` · ${last}` : ""}`);
}

let toastTimer = 0;

function showToast(message, isError = false) {
    toast.textContent = message;
    toast.classList.toggle("error", isError);
    toast.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => {
        toast.hidden = true;
    }, 4000);
}

/* ---------------------------------------------------------------- input */

const DIRECTIONS = {
    up: [0, -1],
    down: [0, 1],
    left: [-1, 0],
    right: [1, 0],
};

const ACTIONS = {
    a: launch,
    x: refresh,
    y: showDetails,
    select: showDetails,
    l1: () => movePage(-1),
    r1: () => movePage(1),
    start: () => {
        state.focus = 0;
        state.page = 0;
        applyPage();
        applyFocus();
        updateActiveTitle();
    },
};

/** repeatDelay then repeatInterval, matching how a console menu behaves on a
 * held direction. */
const repeatDelay = 380;
const repeatInterval = 110;
const heldSince = new Map();

function onButton(event) {
    if (event.kind === "connected") {
        statusLine.textContent = describeLibrary(state.games);
        return;
    }
    if (event.kind === "disconnected") {
        showToast(`${event.device} disconnected`);
        return;
    }
    if (event.kind !== "button") {
        return;
    }
    const name = event.button;
    if (event.pressed) {
        // A held direction repeats after a delay, like a console menu.
        heldSince.set(name, performance.now());
        fireButton(name);
        return;
    }
    heldSince.delete(name);
}

function fireButton(name) {
    const direction = DIRECTIONS[name];
    if (direction) {
        moveFocus(direction[0], direction[1]);
        return;
    }
    const action = ACTIONS[name];
    if (action) {
        action();
    }
}

/** Repeated directions while a button is held. */
setInterval(() => {
    const now = performance.now();
    for (const [name, since] of heldSince) {
        const direction = DIRECTIONS[name];
        if (!direction) {
            continue;
        }
        if (now - since > repeatDelay) {
            moveFocus(direction[0], direction[1]);
            heldSince.set(name, now);
        }
    }
}, 40);

/* Keyboard mirrors the pad so the shell is usable before one is plugged in. */
const KEY_DIRECTIONS = {
    ArrowUp: "up",
    ArrowDown: "down",
    ArrowLeft: "left",
    ArrowRight: "right",
};
const KEY_ACTIONS = {
    Enter: "a",
    z: "a",
    x: "x",
    y: "y",
    Escape: "b",
};

window.addEventListener("keydown", (event) => {
    const name = KEY_DIRECTIONS[event.key] || KEY_ACTIONS[event.key];
    if (!name || keys.has(name)) {
        return;
    }
    keys.add(name);
    event.preventDefault();
    if (name === "b") {
        go.ShowWindow();
        return;
    }
    fireButton(name);
});

window.addEventListener("keyup", (event) => {
    const name = KEY_DIRECTIONS[event.key] || KEY_ACTIONS[event.key];
    keys.delete(name);
    heldSince.delete(name);
});

window.addEventListener("resize", () => {
    document.documentElement.style.setProperty("--cols", String(columnCount()));
    paginate();
});

/* ---------------------------------------------------------------- clock */

function tickClock() {
    const now = new Date();
    clockLine.textContent = now.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

tickClock();
setInterval(tickClock, 15000);

/* ---------------------------------------------------------------- start */

window.runtime.EventsOn("gamepad", (event) => onButton(event));
window.runtime.EventsOn("library", () => loadCatalog());

loadCatalog();