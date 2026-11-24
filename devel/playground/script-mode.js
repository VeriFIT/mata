// The Script mode of the playground: a Mata Script, the values of its declarations, and the output of a run. The
// interpreter runs in a worker of its own (script-worker.js), which is stopped when a run takes too long.

import { EXIT_MEANINGS, scriptDeclarations } from "./script-engine.js";

// Time limits offered to a run, in seconds. The interpreter stops the script at the limit; the worker is terminated
// a moment later, in case a single library operation runs past it.
export const TIME_LIMITS = [5, 30, 120];
export const DEFAULT_TIME_LIMIT = 30;
const GRACE_MS = 2_000;
// Typing waits this long before the declarations are read again.
const DEBOUNCE_MS = 300;
// The output stops growing after this many lines.
const MAX_OUTPUT_LINES = 10_000;

const SCRIPT_TEMPLATE = `(mata-script "0.1.0")
(set-option :alphabet "bytes")
(define language (nfa:create-from-regex "(a|b)*abb"))
(check "deterministic" (nfa:deterministic? language))
(print (nfa:print-to-dot (nfa:minimize (nfa:determinize language))))
`;

// Creates an element with the given attributes and children (strings become text).
function el(tag, attributes = {}, ...children) {
    const element = document.createElement(tag);
    for (const [name, value] of Object.entries(attributes)) {
        if (value === false || value === undefined || value === null) continue;
        if (name.startsWith("on"))
            element.addEventListener(name.slice(2), value);
        else element.setAttribute(name, value === true ? "" : value);
    }
    element.append(
        ...children
            .flat()
            .filter((child) => child !== null && child !== undefined),
    );
    return element;
}

function formatTime(milliseconds) {
    if (milliseconds < 1) return "under 1 ms";
    if (milliseconds < 1000) return `${milliseconds.toFixed(0)} ms`;
    return `${(milliseconds / 1000).toFixed(2)} s`;
}

const plural = (count, noun) =>
    `${count.toLocaleString("en")} ${noun}${count === 1 ? "" : "s"}`;

// `onChange` is called when the script or a value changes, so that the page can update its address.
export function initScriptMode({ onChange }) {
    const form = document.getElementById("script-controls");
    const editor = document.getElementById("script-text");
    const declarationsBox = document.getElementById("script-declarations");
    const runButton = document.getElementById("script-run");
    const stopButton = document.getElementById("script-stop");
    const limitSelect = document.getElementById("script-limit");
    const status = document.getElementById("script-status");
    const summary = document.getElementById("script-summary");
    const outputBox = document.getElementById("script-output");
    const scriptError = document.getElementById("script-error");

    for (const seconds of TIME_LIMITS) {
        limitSelect.append(
            el(
                "option",
                {
                    value: String(seconds),
                    selected: seconds === DEFAULT_TIME_LIMIT,
                },
                `${seconds} s`,
            ),
        );
    }
    editor.placeholder = SCRIPT_TEMPLATE;

    // The values typed for each declaration, kept by name also while the script does not declare it.
    const values = { input: {}, param: {} };
    let declarations = [];

    // The worker, and the run it computes: { id, started, timer }.
    let worker = null;
    let ready = null;
    let nextId = 1;
    let running = null;
    let outputLines = 0;
    let cut = false;

    function fitEditor() {
        const lines = editor.value.split("\n").length;
        editor.rows = Math.min(30, Math.max(12, lines + 1));
    }

    function readState() {
        const inputs = {};
        const params = {};
        for (const { kind, name } of declarations) {
            const value = values[kind][name] ?? "";
            if (value === "") continue;
            (kind === "input" ? inputs : params)[name] = value;
        }
        return { mode: "script", script: editor.value, inputs, params };
    }

    function writeState(state) {
        editor.value = state.script ?? "";
        values.input = { ...(state.inputs ?? {}) };
        values.param = { ...(state.params ?? {}) };
        fitEditor();
        updateDeclarations();
        clearOutput();
        status.textContent = "";
    }

    // Declarations.

    function declarationPane(declaration) {
        const { kind, name, sort, fallback } = declaration;
        const id = `script-${kind}-${name.replace(/[^\w-]/g, "_")}`;
        const update = (value) => {
            values[kind][name] = value;
            onChange();
        };
        if (kind === "param") {
            const input = el("input", {
                id,
                type: "text",
                value: values.param[name] ?? "",
                placeholder: fallback === undefined ? "required" : fallback,
                "data-name": name,
                oninput: (event) => update(event.target.value),
            });
            return el(
                "div",
                { class: "option script-param" },
                el(
                    "label",
                    { for: id },
                    `Parameter `,
                    el("code", {}, name),
                    ` (${sort})`,
                ),
                input,
            );
        }
        const textarea = el("textarea", {
            id,
            class: "is-mata",
            rows: "6",
            "data-name": name,
            placeholder:
                "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1",
            oninput: (event) => update(event.target.value),
        });
        textarea.value = values.input[name] ?? "";
        const file = el("input", {
            type: "file",
            class: "visually-hidden",
            id: `${id}-file`,
            onchange: async (event) => {
                const [chosen] = event.target.files;
                if (!chosen) return;
                textarea.value = await chosen.text();
                update(textarea.value);
            },
        });
        return el(
            "fieldset",
            { class: "automaton-input script-input" },
            el("legend", {}, "Input ", el("code", {}, name), ` (${sort})`),
            el(
                "div",
                { class: "text-actions script-file" },
                file,
                el(
                    "label",
                    { class: "button button-small", for: `${id}-file` },
                    "Open a file…",
                ),
            ),
            textarea,
            el("p", { class: "hint" }, "An automaton in the .mata format."),
        );
    }

    function updateDeclarations() {
        const found = scriptDeclarations(editor.value);
        const key = (list) =>
            list
                .map((d) => `${d.kind} ${d.name} ${d.sort} ${d.fallback}`)
                .join("\n");
        if (
            key(found) === key(declarations) &&
            declarationsBox.childElementCount > 0
        )
            return;
        declarations = found;
        declarationsBox.replaceChildren(...declarations.map(declarationPane));
        declarationsBox.hidden = declarations.length === 0;
    }

    let debounceTimer = null;
    editor.addEventListener("input", () => {
        fitEditor();
        clearTimeout(debounceTimer);
        debounceTimer = setTimeout(() => {
            updateDeclarations();
            onChange();
        }, DEBOUNCE_MS);
    });
    editor.addEventListener("keydown", (event) => {
        if (event.key === "Enter" && (event.ctrlKey || event.metaKey)) {
            event.preventDefault();
            run();
        }
    });
    form.addEventListener("submit", (event) => {
        event.preventDefault();
        run();
    });
    stopButton.addEventListener("click", () => {
        if (running === null) return;
        finish();
        stopWorker();
        showSummary("The run was stopped.", "no");
    });

    // The worker.

    function startWorker() {
        worker = new Worker("script-worker.js", { type: "module" });
        ready = new Promise((resolve) => {
            worker.addEventListener("message", function onReady({ data }) {
                if (data.id !== 0) return;
                worker.removeEventListener("message", onReady);
                resolve(data.failure ?? null);
            });
        });
        worker.addEventListener("message", onMessage);
        worker.addEventListener("error", (event) => {
            event.preventDefault();
            if (running !== null) {
                finish();
                showSummary(
                    "The playground could not start the Mata Script interpreter.",
                    "no",
                );
            }
            stopWorker();
        });
        worker.postMessage({ id: 0, request: null });
    }

    function stopWorker() {
        worker?.terminate();
        worker = null;
        ready = null;
    }

    function finish() {
        clearTimeout(running?.timer);
        running = null;
        runButton.disabled = false;
        stopButton.disabled = true;
        status.textContent = "";
    }

    function onMessage({ data }) {
        if (running === null || data.id !== running.id) return;
        if (data.line) appendLine(data.line);
        if (data.dot) appendDot(data.dot);
        if (data.failure) {
            finish();
            // The module may be broken after a failure, so the next run starts another worker.
            stopWorker();
            showSummary(data.failure, "no");
        }
        if (data.done) {
            finish();
            const { status: exit, milliseconds } = data.done;
            showSummary(
                `${EXIT_MEANINGS[exit] ?? `The script exited with status ${exit}.`} It took ${formatTime(milliseconds)}.`,
                exit === 0 ? "yes" : "no",
            );
        }
    }

    // The output.

    function clearOutput() {
        outputBox.replaceChildren();
        summary.hidden = true;
        outputLines = 0;
        cut = false;
    }

    function showSummary(text, tone) {
        summary.replaceChildren(el("p", { class: tone }, text));
        summary.hidden = false;
    }

    function room() {
        if (outputLines < MAX_OUTPUT_LINES) return true;
        if (!cut) {
            cut = true;
            outputBox.append(
                el(
                    "p",
                    { class: "hint" },
                    `The output is cut after ${plural(MAX_OUTPUT_LINES, "line")}.`,
                ),
            );
        }
        return false;
    }

    function appendLine({ stream, text }) {
        if (!room()) return;
        outputLines++;
        let className = stream === "err" ? "out-line is-error" : "out-line";
        // A record of `check`, or a time of `time` (script-language.md, "Reporting results").
        const verdict = /^[^\s:][^:]*: (true|false)$/.exec(text);
        const time = /^[^\s:][^:]*: (\d+\.\d{6})$/.exec(text);
        if (stream === "out" && verdict)
            className += verdict[1] === "true" ? " is-true" : " is-false";
        else if (stream === "out" && time) className += " is-time";
        const line = el(
            "div",
            { class: className },
            text === "" ? "\u200b" : text,
        );
        if (stream === "out" && time)
            line.append(
                el(
                    "span",
                    { class: "out-note" },
                    ` ${formatTime(Number(time[1]) * 1000)}`,
                ),
            );
        let block = outputBox.lastElementChild;
        if (block === null || block.tagName !== "PRE") {
            block = el("pre", { class: "mata-text script-lines" });
            outputBox.append(block);
        }
        block.append(line);
    }

    function appendDot({ text, truncated, states, transitions, svg }) {
        if (!room()) return;
        outputLines += text.split("\n").length;
        const facts = `${plural(states, "state")}, ${plural(transitions, "transition")}`;
        const figure = el("div", { class: "drawing" });
        if (svg) {
            figure.innerHTML = svg.slice(svg.indexOf("<svg"));
            const drawn = figure.querySelector("svg");
            drawn?.querySelector("g.graph > title")?.remove();
            drawn?.setAttribute("role", "img");
            drawn?.setAttribute("aria-label", `An automaton: ${facts}`);
        } else {
            figure.classList.add("is-empty");
            figure.append(
                el(
                    "p",
                    {},
                    "This automaton is too large to draw. Its DOT text is below.",
                ),
            );
        }
        outputBox.append(
            el(
                "div",
                { class: "script-drawing" },
                el("p", { class: "facts" }, facts),
                figure,
                el(
                    "details",
                    { open: !svg },
                    el("summary", { class: "hint" }, "DOT"),
                    el("pre", { class: "mata-text" }, el("code", {}, text)),
                    truncated
                        ? el("p", { class: "hint" }, "The text is cut short.")
                        : null,
                ),
            ),
        );
    }

    // Running.

    function validate() {
        scriptError.hidden = true;
        if (editor.value.trim() === "") return "Write a script first.";
        for (const { kind, name, fallback } of declarations) {
            const value = values[kind][name] ?? "";
            if (kind === "input" && value.trim() === "")
                return `Give the input ${name}: an automaton in the .mata format.`;
            if (kind === "param" && value === "" && fallback === undefined)
                return `Give the parameter ${name}: it has no default.`;
            if (kind === "param" && /[\r\n]/.test(value))
                return `The parameter ${name} must be one line.`;
        }
        return null;
    }

    async function run() {
        if (running !== null) return;
        clearTimeout(debounceTimer);
        updateDeclarations();
        onChange();
        const problem = validate();
        if (problem !== null) {
            scriptError.textContent = problem;
            scriptError.hidden = false;
            return;
        }
        clearOutput();
        const { script, inputs, params } = readState();
        const limit = Number(limitSelect.value) * 1000;
        if (worker === null) startWorker();
        const id = nextId++;
        running = { id, timer: null };
        runButton.disabled = true;
        stopButton.disabled = false;
        status.textContent = "Loading the interpreter…";
        const failure = await ready;
        if (running?.id !== id) return;
        if (failure) {
            finish();
            stopWorker();
            showSummary(failure, "no");
            return;
        }
        status.textContent = "Running…";
        running.timer = setTimeout(() => {
            if (running?.id !== id) return;
            finish();
            stopWorker();
            showSummary(
                `The script did not finish within ${limit / 1000} seconds: a library operation ran past the time limit, so the interpreter was stopped. The output above is what it printed before.`,
                "no",
            );
        }, limit + GRACE_MS);
        worker.postMessage({
            id,
            request: { script, inputs, params, timeLimitMs: limit },
        });
    }

    return {
        readState,
        writeState,
        // Shows a script with a template when the mode is opened with none.
        activate() {
            if (editor.value === "" && declarations.length === 0)
                writeState({ script: SCRIPT_TEMPLATE });
        },
    };
}
