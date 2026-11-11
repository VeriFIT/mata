// The playground page: it sends the automata and the operation to the worker as they change and shows the outcome.

import { EXAMPLES } from "./examples.js";
import {
    decodeState,
    encodeState,
    regexToBytes,
    showWord,
    wordNames,
} from "./automata.js";

// A computation is stopped after this time.
const TIMEOUT_MS = 20_000;
// Typing waits this long before the automata are recomputed.
const DEBOUNCE_MS = 250;

const USES_B = new Set([
    "intersection",
    "union",
    "concatenation",
    "difference",
    "is_included",
    "are_equivalent",
]);
const WITH_ALPHABET = new Set(["complement", "is_universal"]);

const RESULT_TITLES = {
    determinize: "A, determinized",
    minimize: "The minimal deterministic automaton for A",
    reduce: "A, reduced by simulation",
    trim: "A, trimmed",
    complement: "The complement of A",
    reverse: "The reversal of A",
    remove_epsilon: "A without ε-transitions",
    intersection: "The intersection of A and B",
    union: "The union of A and B",
    concatenation: "The concatenation of A and B",
    difference: "The difference of A and B: the words in A but not in B",
};

// Names of the result in its tab.
const RESULT_LABELS = {
    determinize: "Determinized",
    minimize: "Minimal DFA",
    reduce: "Reduced",
    trim: "Trimmed",
    complement: "Complement",
    reverse: "Reversal",
    remove_epsilon: "Without ε",
    intersection: "Intersection",
    union: "Union",
    concatenation: "Concatenation",
    difference: "Difference",
};

const HINTS = {
    regex: "RE2 syntax over bytes, such as (a|b)*abb or [0-9]+.",
    mata: "An @NFA-explicit automaton with %Alphabet-auto; write <eps> for ε.",
};

const WORD_HINTS = {
    regex: "Type the word character by character; write \\xHH for any byte. Leave it empty for the empty word.",
    mata: "Write the names of the symbols, separated by spaces. Leave it empty for the empty word.",
};

const MATA_TEMPLATE = `@NFA-explicit
%Alphabet-auto
%Initial s
%Final f
s a f
`;

const DEFAULT_STATE = {
    aFormat: "regex",
    aText: "",
    bFormat: "regex",
    bText: "",
    operation: "show",
    word: "",
    alphabet: "used",
};

const form = document.getElementById("controls");
const output = {
    status: document.getElementById("status"),
    verdict: document.getElementById("verdict"),
    notes: document.getElementById("notes"),
    views: document.getElementById("views"),
    tabs: document.querySelector("#views .view-tabs"),
    panels: document.querySelector("#views .view-panels"),
    message: document.getElementById("message"),
};

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

function wordElement(text) {
    return text === "ε"
        ? el("span", {}, el("code", { class: "word" }, "ε"), ", the empty word")
        : el("code", { class: "word" }, text);
}

// Browsers measure time coarsely, often in steps of 0.1 ms, so shorter times are not told apart.
function formatTime(milliseconds) {
    if (milliseconds < 0.1) return "under 0.1 ms";
    if (milliseconds < 1) return `${milliseconds.toFixed(1)} ms`;
    if (milliseconds < 1000)
        return `${milliseconds.toFixed(milliseconds < 10 ? 2 : 1)} ms`;
    return `${(milliseconds / 1000).toFixed(2)} s`;
}

const plural = (count, noun) =>
    `${count.toLocaleString("en")} ${noun}${count === 1 ? "" : "s"}`;

// Reading and writing the form.

function readForm() {
    const elements = form.elements;
    return {
        aFormat: elements.aFormat.value,
        aText: elements.aText.value,
        bFormat: elements.bFormat.value,
        bText: elements.bText.value,
        operation: elements.operation.value || "show",
        word: elements.word.value,
        alphabet: elements.alphabet.value,
    };
}

function writeForm(state) {
    const elements = form.elements;
    elements.aFormat.value = state.aFormat === "mata" ? "mata" : "regex";
    elements.aText.value = state.aText;
    elements.bFormat.value = state.bFormat === "mata" ? "mata" : "regex";
    elements.bText.value = state.bText;
    elements.operation.value =
        state.operation in RESULT_TITLES || isQuestion(state.operation)
            ? state.operation
            : "show";
    elements.word.value = state.word;
    elements.alphabet.value = state.alphabet === "bytes" ? "bytes" : "used";
    for (const key of ["a", "b"])
        lastTexts[key][elements[`${key}Format`].value] =
            elements[`${key}Text`].value;
}

function isQuestion(operation) {
    return [
        "is_empty",
        "is_universal",
        "is_included",
        "are_equivalent",
        "is_in_lang",
    ].includes(operation);
}

// The text each input had in each format, restored when the format is switched back.
const lastTexts = { a: { regex: "", mata: "" }, b: { regex: "", mata: "" } };

function fitTextarea(textarea) {
    const lines = textarea.value.split("\n").length;
    textarea.rows = Math.min(
        16,
        Math.max(2, lines + (textarea.value.endsWith("\n") ? 0 : 1)),
    );
}

function updateControls(state) {
    for (const key of ["a", "b"]) {
        const format = state[`${key}Format`];
        const unused = key === "b" && !USES_B.has(state.operation);
        document.getElementById(`${key}-hint`).textContent = unused
            ? "This operation does not use B."
            : HINTS[format];
        const textarea = document.getElementById(`${key}-text`);
        textarea.classList.toggle("is-mata", format === "mata");
        textarea.placeholder =
            format === "regex"
                ? key === "a"
                    ? "(a|b)*abb"
                    : "(a|b)*b"
                : MATA_TEMPLATE;
        fitTextarea(textarea);
    }
    document
        .getElementById("input-b")
        .classList.toggle("is-unused", !USES_B.has(state.operation));
    document.getElementById("word-option").hidden =
        state.operation !== "is_in_lang";
    document.getElementById("word-hint").textContent =
        WORD_HINTS[state.aFormat];
    document.getElementById("alphabet-option").hidden = !WITH_ALPHABET.has(
        state.operation,
    );
}

// Errors next to the inputs.

function setFieldError(key, message) {
    const error = document.getElementById(`${key}-error`);
    const field =
        key === "word"
            ? document.getElementById("word")
            : document.getElementById(`${key}-text`);
    error.hidden = !message;
    error.textContent = message ?? "";
    if (message) field.setAttribute("aria-invalid", "true");
    else field.removeAttribute("aria-invalid");
}

function clearFieldErrors() {
    for (const key of ["a", "b", "word"]) setFieldError(key, null);
}

// The worker. It computes one request at a time; only the newest request waits for it, and every computation has a
// time limit, also one whose result is no longer wanted.

let worker = null;
let nextId = 1;
// Whether the worker has loaded Mata.
let ready = false;
// The request the worker is computing: { id, started }.
let running = null;
let runningTimer = null;
// The newest request, waiting for the worker: { id, request }.
let queued = null;
// The request whose result is shown when it arrives: { id, state }.
let wanted = null;
let statusTimer = null;
let latestAutomata = {};
// The format and text of each input that `latestAutomata` were computed from.
let latestSources = {};

function startWorker() {
    ready = false;
    worker = new Worker("worker.js", { type: "module" });
    worker.addEventListener("message", onWorkerMessage);
    worker.addEventListener("error", (event) => {
        event.preventDefault();
        stopWorker(
            "The playground could not start Mata. Reload the page to try again.",
        );
    });
    // The worker answers this request as soon as it has loaded Mata, before the requests that follow it, so that a
    // costly computation can be stopped while it runs.
    worker.postMessage({ id: 0, request: null });
}

function restartWorker() {
    worker?.terminate();
    running = null;
    clearTimeout(runningTimer);
    startWorker();
}

function stopWaiting() {
    wanted = null;
    clearTimeout(statusTimer);
    output.status.textContent = "";
}

function send(job) {
    if (worker === null) startWorker();
    running = { id: job.id, started: performance.now() };
    worker.postMessage({ id: job.id, request: job.request });
    clearTimeout(runningTimer);
    runningTimer = setTimeout(() => {
        const timedOut = running?.id;
        restartWorker();
        if (wanted !== null && wanted.id === timedOut) {
            stopWaiting();
            showMessage(
                `Mata did not finish within ${TIMEOUT_MS / 1000} seconds, so the computation was stopped. The result may be too large for the browser.`,
            );
        }
        sendQueued();
    }, TIMEOUT_MS);
}

function sendQueued() {
    if (queued === null || running !== null) return;
    const job = queued;
    queued = null;
    send(job);
}

// Stops waiting for the current result, whose inputs have changed. A computation that has run for a while is
// stopped; a quick one is left to finish within its time limit, and its result is ignored.
function abandon() {
    stopWaiting();
    queued = null;
    if (running !== null && ready && performance.now() - running.started > 300)
        restartWorker();
}

function submit(state, request) {
    const job = { id: nextId++, request };
    wanted = { id: job.id, state };
    if (running === null) {
        send(job);
    } else {
        queued = job;
        // The computation in the way gets a moment to finish before it is stopped for the newer request.
        const blocking = running.id;
        setTimeout(() => {
            if (ready && running?.id === blocking && queued !== null) {
                restartWorker();
                sendQueued();
            }
        }, 300);
    }
    statusTimer = setTimeout(() => {
        output.status.textContent = ready ? "Computing…" : "Loading Mata…";
    }, 200);
}

// Stops the worker that could not load Mata. Starting another one would only fail again, so the next request tries.
function stopWorker(message) {
    worker?.terminate();
    worker = null;
    running = null;
    queued = null;
    clearTimeout(runningTimer);
    stopWaiting();
    showMessage(message);
}

function onWorkerMessage({ data }) {
    if (data.id === 0 && data.failure) {
        stopWorker(data.failure);
        return;
    }
    ready = true;
    if (data.revision !== undefined) {
        if (/^[0-9a-f]{7,40}$/.test(data.revision)) {
            const link = el(
                "a",
                {
                    href: `https://github.com/VeriFIT/mata/commit/${data.revision}`,
                },
                data.revision.slice(0, 7),
            );
            document
                .getElementById("revision")
                .replaceChildren(" (", link, ")");
        }
        return;
    }
    if (running !== null && data.id === running.id) {
        running = null;
        clearTimeout(runningTimer);
    }
    const shown = wanted !== null && data.id === wanted.id ? wanted : null;
    if (shown !== null) stopWaiting();
    if (data.failure) {
        // The worker may be broken after a failure, so it is started anew.
        if (shown !== null) showMessage(data.failure);
        restartWorker();
    } else if (shown !== null) {
        render(shown.state, data.response, data.drawings);
    }
    sendQueued();
}

// Evaluation.

let debounceTimer = null;

function schedule() {
    clearTimeout(debounceTimer);
    debounceTimer = setTimeout(evaluate, DEBOUNCE_MS);
}

function evaluate() {
    clearTimeout(debounceTimer);
    const state = readForm();
    history.replaceState(null, "", `#${encodeState(state)}`);
    updateControls(state);
    clearFieldErrors();
    abandon();

    if (state.aText.trim() === "") {
        showMessage(
            "Enter automaton A: a regular expression, or an automaton in the .mata format.",
        );
        return;
    }
    if (USES_B.has(state.operation) && state.bText.trim() === "") {
        showMessage("Enter automaton B: this operation uses both A and B.");
        return;
    }
    const request = {
        aFormat: state.aFormat,
        aText: state.aText,
        bFormat: state.bFormat,
        bText: USES_B.has(state.operation) ? state.bText : "",
        operation: state.operation,
        word:
            state.operation === "is_in_lang"
                ? wordNames(state.word, state.aFormat).join("\n")
                : "",
        alphabet: state.alphabet,
    };
    for (const key of ["a", "b"]) {
        if (request[`${key}Format`] !== "regex") continue;
        try {
            request[`${key}Text`] = regexToBytes(request[`${key}Text`]);
        } catch (error) {
            setFieldError(key, error.message);
            showMessage(
                `Correct automaton ${key.toUpperCase()} to see the result.`,
            );
            return;
        }
    }
    submit(state, request);
}

// Showing the outcome.

function showMessage(text) {
    output.verdict.hidden = true;
    output.notes.hidden = true;
    output.views.hidden = true;
    output.message.hidden = false;
    output.message.textContent = text;
}

function verdictFor(state, response, symbols) {
    const { holds, word } = response.check;
    const witness =
        word === null ? null : showWord(word.map((index) => symbols[index]));
    const answer = el(
        "strong",
        { class: holds ? "yes" : "no" },
        holds ? "Yes." : "No.",
    );
    switch (state.operation) {
        case "is_empty":
            return holds
                ? [answer, " A accepts no word."]
                : [answer, " A accepts ", wordElement(witness), "."];
        case "is_universal":
            return holds
                ? [answer, " A accepts every word over its alphabet."]
                : [answer, " A rejects ", wordElement(witness), "."];
        case "is_included":
            return holds
                ? [answer, " Every word in A is also in B."]
                : [answer, " ", wordElement(witness), " is in A but not in B."];
        case "are_equivalent":
            if (holds) return [answer, " A and B accept the same words."];
            return [
                answer,
                " ",
                wordElement(witness),
                response.witnessInA
                    ? " is in A but not in B."
                    : " is in B but not in A.",
            ];
        case "is_in_lang": {
            const typed = showWord(wordNames(state.word, state.aFormat));
            return [
                answer,
                holds ? " A accepts " : " A rejects ",
                wordElement(typed),
                ".",
            ];
        }
        default:
            return [];
    }
}

let selectedView = "result";

function render(state, response, drawings) {
    clearFieldErrors();
    output.status.textContent = "";
    if (!response.ok) {
        if (
            response.source === "a" ||
            response.source === "b" ||
            response.source === "word"
        ) {
            setFieldError(response.source, response.error);
            const name =
                response.source === "word"
                    ? "the word"
                    : `automaton ${response.source.toUpperCase()}`;
            showMessage(`Correct ${name} to see the result.`);
        } else {
            showMessage(response.error);
        }
        return;
    }

    latestAutomata = response.automata;
    latestSources = {
        a: `${state.aFormat}\n${state.aText}`,
        b: `${state.bFormat}\n${state.bText}`,
    };
    output.message.hidden = true;
    output.status.textContent = `Mata took ${formatTime(response.milliseconds)}.`;

    output.verdict.hidden = !response.check;
    if (response.check)
        output.verdict.replaceChildren(
            el("p", {}, ...verdictFor(state, response, response.symbols)),
        );

    const removed = response.epsilonRemoved.map((key) => key.toUpperCase());
    output.notes.hidden = removed.length === 0;
    if (removed.length > 0) {
        const which = removed.join(" and ");
        output.notes.replaceChildren(
            el(
                "p",
                {},
                `Mata removed the ε-transitions of ${which} first, because this operation needs automata without them.`,
            ),
        );
    }

    const keys = ["result", "a", "b"].filter((key) => key in response.automata);
    if (!keys.includes(selectedView)) selectedView = keys[0];
    output.tabs.replaceChildren();
    output.panels.replaceChildren();
    for (const key of keys) {
        const label =
            key === "result"
                ? RESULT_LABELS[state.operation]
                : `Automaton ${key.toUpperCase()}`;
        const title = key === "result" ? RESULT_TITLES[state.operation] : label;
        const tab = el(
            "button",
            {
                type: "button",
                class: "tab",
                role: "tab",
                id: `view-tab-${key}`,
                "aria-controls": `view-${key}`,
                "aria-selected": String(key === selectedView),
                tabindex: key === selectedView ? "0" : "-1",
                onclick: () => selectView(key, false),
            },
            label,
        );
        output.tabs.append(tab);
        const panel = el(
            "div",
            {
                class: "view",
                role: "tabpanel",
                id: `view-${key}`,
                "aria-labelledby": `view-tab-${key}`,
                hidden: key !== selectedView,
            },
            viewContent(key, title, response.automata[key], drawings[key]),
        );
        output.panels.append(panel);
    }
    output.views.hidden = false;
}

function selectView(key, focus) {
    selectedView = key;
    for (const tab of output.tabs.querySelectorAll('[role="tab"]')) {
        const selected = tab.id === `view-tab-${key}`;
        tab.setAttribute("aria-selected", String(selected));
        tab.tabIndex = selected ? 0 : -1;
        if (selected && focus) tab.focus();
    }
    for (const panel of output.panels.children)
        panel.hidden = panel.id !== `view-${key}`;
}

output.tabs.addEventListener("keydown", (event) => {
    const tabs = [...output.tabs.querySelectorAll('[role="tab"]')];
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
    const tab = tabs[(moves[event.key] + tabs.length) % tabs.length];
    selectView(tab.id.replace("view-tab-", ""), true);
});

function viewContent(key, title, automaton, drawing) {
    const facts = [
        plural(automaton.states, "state"),
        plural(automaton.transitionCount, "transition"),
        automaton.deterministic ? "deterministic" : "nondeterministic",
    ].join(", ");
    const figure = el("div", { class: "drawing" });
    if (drawing) {
        figure.innerHTML = drawing.slice(drawing.indexOf("<svg"));
        const svg = figure.querySelector("svg");
        svg?.querySelector("g.graph > title")?.remove();
        // A large drawing is shrunk to fit, but not below 60 % of its size, so that its labels stay legible.
        if (svg)
            svg.style.minWidth = `${Math.round(svg.width.baseVal.value * 0.6)}px`;
        svg?.setAttribute("role", "img");
        svg?.setAttribute("aria-label", `${title}: ${facts}`);
    } else {
        figure.classList.add("is-empty");
        figure.append(
            el(
                "p",
                {},
                "This automaton is too large to draw. Its text in the .mata format is below.",
            ),
        );
    }

    const actions = el("div", { class: "text-actions" });
    const copy = el(
        "button",
        { type: "button", class: "button button-small" },
        "Copy",
    );
    copy.addEventListener("click", async () => {
        try {
            await navigator.clipboard.writeText(automaton.mata);
            copy.textContent = "Copied";
        } catch {
            copy.textContent = "Copy failed";
        }
        setTimeout(() => (copy.textContent = "Copy"), 1600);
    });
    const download = el(
        "button",
        { type: "button", class: "button button-small" },
        "Download",
    );
    download.addEventListener("click", () => {
        const url = URL.createObjectURL(
            new Blob([automaton.mata], { type: "text/plain" }),
        );
        el("a", {
            href: url,
            download: `${key === "result" ? "result" : `automaton-${key}`}.mata`,
        }).click();
        setTimeout(() => URL.revokeObjectURL(url), 0);
    });
    actions.append(copy, download);
    if (key !== "a") {
        const use = el(
            "button",
            {
                type: "button",
                class: "button button-small",
                disabled: automaton.mataTruncated,
            },
            "Use as A",
        );
        use.addEventListener("click", () => {
            lastTexts.a[form.elements.aFormat.value] =
                form.elements.aText.value;
            form.elements.aFormat.value = "mata";
            form.elements.aText.value = automaton.mata;
            lastTexts.a.mata = automaton.mata;
            selectedView = "result";
            evaluate();
            document.getElementById("a-text").focus();
        });
        actions.append(use);
    }

    return [
        el("p", { class: "view-title" }, title),
        el("p", { class: "facts" }, facts),
        figure,
        el(
            "div",
            { class: "text-head" },
            el("h3", {}, "In the .mata format"),
            actions,
        ),
        el("pre", { class: "mata-text" }, el("code", {}, automaton.mata)),
        automaton.mataTruncated
            ? el(
                  "p",
                  { class: "hint" },
                  "The text is cut short; the automaton is larger.",
              )
            : null,
    ];
}

// Events.

form.addEventListener("submit", (event) => event.preventDefault());

form.addEventListener("input", (event) => {
    const target = event.target;
    if (target.name === "aText" || target.name === "bText") {
        fitTextarea(target);
        schedule();
    } else if (target.name === "word") {
        schedule();
    }
});

form.addEventListener("change", (event) => {
    const target = event.target;
    if (target.name === "aFormat" || target.name === "bFormat") {
        const key = target.name[0];
        const textarea = form.elements[`${key}Text`];
        const previous = target.value === "mata" ? "regex" : "mata";
        lastTexts[key][previous] = textarea.value;
        // Switching to the .mata format shows the automaton of the regular expression in that format, provided the
        //  latest result was computed from this very expression and its text is complete.
        const automaton = latestAutomata[key];
        const converted =
            target.value === "mata" &&
            automaton !== undefined &&
            !automaton.mataTruncated &&
            latestSources[key] === `${previous}\n${textarea.value}`
                ? automaton.mata
                : undefined;
        textarea.value =
            converted ??
            (lastTexts[key][target.value] ||
                (target.value === "mata" ? MATA_TEMPLATE : ""));
        evaluate();
    } else if (target.name === "operation" || target.name === "alphabet") {
        if (target.name === "operation") selectedView = "result";
        evaluate();
    }
});

// The state an example sets: everything but its title.
function exampleState(example) {
    const state = { ...DEFAULT_STATE, ...example };
    delete state.title;
    return state;
}

const exampleSelect = document.getElementById("example");
EXAMPLES.forEach((example, index) =>
    exampleSelect.append(el("option", { value: String(index) }, example.title)),
);
exampleSelect.addEventListener("change", () => {
    const example = EXAMPLES[Number(exampleSelect.value)];
    exampleSelect.value = "";
    if (!example) return;
    writeForm(exampleState(example));
    selectedView = "result";
    evaluate();
});

const copyLink = document.getElementById("copy-link");
copyLink.addEventListener("click", async () => {
    try {
        const url = new URL(location.href);
        url.hash = encodeState(readForm());
        await navigator.clipboard.writeText(url.href);
        copyLink.textContent = "Link copied";
    } catch {
        copyLink.textContent = "Copy failed";
    }
    setTimeout(() => (copyLink.textContent = "Copy link"), 1600);
});

window.addEventListener("hashchange", () => {
    if (location.hash.slice(1) === encodeState(readForm())) return;
    writeForm({ ...DEFAULT_STATE, ...decodeState(location.hash) });
    selectedView = "result";
    evaluate();
});

// Start: the state from the address, or the first example.
writeForm(
    location.hash.length > 1
        ? { ...DEFAULT_STATE, ...decodeState(location.hash) }
        : exampleState(EXAMPLES[0]),
);
output.status.textContent = "Loading Mata…";
startWorker();
evaluate();
