// Behaviour shared by the Mata website pages: the theme switch, language tabs, copy buttons, and the
// automaton in the hero of the landing page.

const storage = {
    get(key) {
        try {
            return localStorage.getItem(key);
        } catch {
            return null;
        }
    },
    set(key, value) {
        try {
            localStorage.setItem(key, value);
        } catch {
            // Storage is unavailable; the choice only lasts for this page view.
        }
    },
};

const prefersDark = window.matchMedia("(prefers-color-scheme: dark)");
const prefersReducedMotion = window.matchMedia(
    "(prefers-reduced-motion: reduce)",
);

function currentTheme() {
    return (
        document.documentElement.dataset.theme ||
        (prefersDark.matches ? "dark" : "light")
    );
}

function initThemeToggle() {
    for (const button of document.querySelectorAll(".theme-toggle")) {
        const update = () => {
            const next = currentTheme() === "dark" ? "light" : "dark";
            button.setAttribute("aria-label", `Switch to the ${next} theme`);
            button.title = `Switch to the ${next} theme`;
        };
        button.addEventListener("click", () => {
            const next = currentTheme() === "dark" ? "light" : "dark";
            const root = document.documentElement;
            root.classList.add("no-transitions");
            root.dataset.theme = next;
            requestAnimationFrame(() =>
                requestAnimationFrame(() =>
                    root.classList.remove("no-transitions"),
                ),
            );
            storage.set("mata-theme", next);
            update();
        });
        prefersDark.addEventListener("change", update);
        update();
    }
}

// Tabs following the WAI-ARIA tabs pattern; the chosen tab is remembered across visits.
function initTabs() {
    for (const container of document.querySelectorAll("[data-tabs]")) {
        const tabs = [...container.querySelectorAll('[role="tab"]')];
        const select = (tab, focus) => {
            for (const other of tabs) {
                const selected = other === tab;
                other.setAttribute("aria-selected", String(selected));
                other.tabIndex = selected ? 0 : -1;
                document.getElementById(
                    other.getAttribute("aria-controls"),
                ).hidden = !selected;
            }
            if (focus) tab.focus();
            storage.set("mata-language", tab.id);
        };
        const saved = tabs.find(
            (tab) => tab.id === storage.get("mata-language"),
        );
        if (saved) select(saved, false);
        container
            .querySelector('[role="tablist"]')
            .addEventListener("keydown", (event) => {
                const index = tabs.indexOf(document.activeElement);
                if (index < 0) return;
                const moves = {
                    ArrowLeft: index - 1,
                    ArrowRight: index + 1,
                    Home: 0,
                    End: tabs.length - 1,
                };
                if (!(event.key in moves)) return;
                event.preventDefault();
                select(
                    tabs[(moves[event.key] + tabs.length) % tabs.length],
                    true,
                );
            });
        for (const tab of tabs)
            tab.addEventListener("click", () => select(tab, false));
    }
}

function initCopyButtons() {
    if (!navigator.clipboard) return;
    for (const pre of document.querySelectorAll("pre.code")) {
        const wrapper = document.createElement("div");
        wrapper.className = "code-block";
        pre.replaceWith(wrapper);
        wrapper.append(pre);
        const button = document.createElement("button");
        button.type = "button";
        button.className = "copy-button";
        button.textContent = "Copy";
        let timer;
        button.addEventListener("click", async () => {
            try {
                await navigator.clipboard.writeText(pre.textContent);
                button.textContent = "Copied";
            } catch {
                button.textContent = "Copy failed";
            }
            button.classList.add("is-done");
            clearTimeout(timer);
            timer = setTimeout(() => {
                button.textContent = "Copy";
                button.classList.remove("is-done");
            }, 1600);
        });
        wrapper.append(button);
    }
}

// The minimal DFA for (a|b)*abb, as drawn in the hero. A token travels along each transition the run takes.
function initAutomatonDemo() {
    const figure = document.querySelector(".automaton-demo");
    if (!figure) return;
    const svg = figure.querySelector("svg.dfa");
    const input = figure.querySelector(".demo-input");
    const tape = figure.querySelector(".tape");
    const verdict = figure.querySelector(".verdict");
    const replay = figure.querySelector(".demo-replay");

    const delta = {
        0: { a: 1, b: 0 },
        1: { a: 1, b: 2 },
        2: { a: 1, b: 3 },
        3: { a: 1, b: 0 },
    };
    const initial = 0;
    const finals = new Set([3]);
    const states = new Map(
        [...svg.querySelectorAll(".state")].map((g) => [
            Number(g.dataset.state),
            g,
        ]),
    );
    const edges = new Map(
        [...svg.querySelectorAll(".edge-group")].map((g) => [
            `${g.dataset.from}${g.dataset.symbol}`,
            g,
        ]),
    );

    const token = document.createElementNS(
        "http://www.w3.org/2000/svg",
        "circle",
    );
    token.setAttribute("class", "token");
    token.setAttribute("r", "6.5");
    token.style.display = "none";
    svg.append(token);

    // Runs the automaton on a word: the transitions taken, and the position where the run got stuck, if it did.
    function run(word) {
        const steps = [];
        let state = initial;
        for (const symbol of word) {
            const target = delta[state][symbol];
            if (target === undefined)
                return { steps, end: state, stuck: steps.length };
            steps.push({ from: state, symbol, to: target });
            state = target;
        }
        return { steps, end: state, stuck: -1 };
    }

    const stateName = (state) => `<span class="q">q<sub>${state}</sub></span>`;
    const shown = (symbol) => (symbol === " " ? "␣" : symbol);
    const escape = (text) =>
        text.replace(/[&<>"]/g, (c) => `&#${c.charCodeAt(0)};`);

    function setActiveEdge(step) {
        for (const [key, group] of edges) {
            const active =
                step !== undefined && key === `${step.from}${step.symbol}`;
            group.classList.toggle("is-active", active);
            group
                .querySelector(".edge")
                .setAttribute(
                    "marker-end",
                    active ? "url(#arrow-active)" : "url(#arrow)",
                );
        }
    }

    function setCurrentState(state) {
        for (const [id, group] of states)
            group.classList.toggle("is-current", id === state);
    }

    // Shows the configuration after reading `read` symbols of the word; `done` also reports the result.
    function show(word, result, read, done) {
        const step = read > 0 ? result.steps[read - 1] : undefined;
        setCurrentState(step ? step.to : initial);
        setActiveEdge(step);
        tape.replaceChildren(
            ...[...word].map((symbol, index) => {
                const cell = document.createElement("li");
                cell.textContent = shown(symbol);
                if (index < read) cell.classList.add("is-read");
                if (index === read - 1) cell.classList.add("is-last");
                if (done && index === result.stuck)
                    cell.classList.add("is-stuck");
                return cell;
            }),
        );
        if (!done) {
            verdict.textContent = "";
            return;
        }
        if (result.stuck >= 0) {
            const symbol = escape(shown(word[result.stuck]));
            verdict.innerHTML = `<strong class="rejected">Rejected.</strong> There is no transition from ${stateName(result.end)} on <code>${symbol}</code>; the alphabet is {a, b}.`;
        } else if (finals.has(result.end)) {
            verdict.innerHTML = `<strong class="accepted">Accepted.</strong> The run ends in ${stateName(result.end)}, a final state.`;
        } else {
            verdict.innerHTML = `<strong class="rejected">Rejected.</strong> The run ends in ${stateName(result.end)}, which is not a final state.`;
        }
    }

    let generation = 0;
    const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
    const ease = (t) => (t < 0.5 ? 4 * t * t * t : 1 - (-2 * t + 2) ** 3 / 2);

    function moveToken(step, duration, current) {
        const path = edges
            .get(`${step.from}${step.symbol}`)
            .querySelector(".edge");
        const length = path.getTotalLength();
        token.style.display = "";
        return new Promise((resolve) => {
            const start = performance.now();
            const frame = (now) => {
                if (current !== generation) return resolve();
                const t = Math.min(1, (now - start) / duration);
                const point = path.getPointAtLength(ease(t) * length);
                token.setAttribute("cx", point.x);
                token.setAttribute("cy", point.y);
                if (t < 1) requestAnimationFrame(frame);
                else resolve();
            };
            requestAnimationFrame(frame);
        });
    }

    // Animates the steps of the run from position `from` on.
    async function animate(word, result, from) {
        const current = ++generation;
        const pace = word.length > 12 ? 0.6 : 1;
        show(word, result, from, false);
        if (from === 0) await wait(350 * pace);
        for (let read = from + 1; read <= result.steps.length; read++) {
            if (current !== generation) return;
            const step = result.steps[read - 1];
            setActiveEdge(step);
            await moveToken(step, 520 * pace, current);
            if (current !== generation) return;
            token.style.display = "none";
            show(word, result, read, false);
            await wait(160 * pace);
        }
        if (current !== generation) return;
        show(word, result, result.steps.length, true);
    }

    function showNow(word, result) {
        generation++;
        token.style.display = "none";
        show(word, result, result.steps.length, true);
    }

    let previous = input.value;
    input.addEventListener("input", () => {
        const word = input.value;
        const result = run(word);
        // Typing one more symbol reads it; any other edit shows the new run at once.
        const appended =
            word.length === previous.length + 1 && word.startsWith(previous);
        previous = word;
        if (appended && !prefersReducedMotion.matches && result.stuck < 0) {
            animate(word, result, word.length - 1);
        } else {
            showNow(word, result);
        }
    });

    replay.addEventListener("click", () => {
        const word = input.value;
        const result = run(word);
        if (prefersReducedMotion.matches) showNow(word, result);
        else animate(word, result, 0);
    });

    const word = input.value;
    const result = run(word);
    if (prefersReducedMotion.matches || !("IntersectionObserver" in window)) {
        showNow(word, result);
        return;
    }
    show(word, result, 0, false);
    const observer = new IntersectionObserver(
        (entries) => {
            if (!entries.some((entry) => entry.isIntersecting)) return;
            observer.disconnect();
            if (input.value === word) animate(word, result, 0);
        },
        { threshold: 0.6 },
    );
    observer.observe(svg);
}

initThemeToggle();
initTabs();
initCopyButtons();
initAutomatonDemo();
