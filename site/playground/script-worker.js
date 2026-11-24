// The worker of the Script mode. It runs the Mata Script interpreter, compiled to WebAssembly, and Graphviz, which
// draws the DOT graphs the script prints. It posts the output as it is written, so the page keeps what a run printed
// before it was stopped, and the page stops a run that takes too long by terminating the worker.

import createMataPlaygroundScript from "./mata-playground-script.js";
import { instance as createViz } from "./vendor/viz.js";
import { DOT_STYLE } from "./automata.js";
import {
    dotSize,
    isDotEnd,
    isDotStart,
    loadScriptEngine,
} from "./script-engine.js";

// Larger automata are not drawn, as in the operation mode.
const MAX_DRAWN_STATES = 120;
const MAX_DRAWN_TRANSITIONS = 400;
// A longer DOT graph is listed only up to this many lines.
const MAX_LISTED_LINES = 5000;

const modules = Promise.all([
    loadScriptEngine(createMataPlaygroundScript),
    createViz(),
]);

function draw(viz, dot) {
    const { states, transitions } = dotSize(dot);
    if (states > MAX_DRAWN_STATES || transitions > MAX_DRAWN_TRANSITIONS)
        return { states, transitions, svg: null };
    // The defaults of the playground's drawings go first, so that the graph's own attributes override them.
    const [head, ...body] = dot.split("\n");
    try {
        return {
            states,
            transitions,
            svg: viz.renderString([head, ...DOT_STYLE, ...body].join("\n"), {
                format: "svg",
                engine: "dot",
            }),
        };
    } catch {
        return { states, transitions, svg: null };
    }
}

self.addEventListener("message", async (event) => {
    const { id, request } = event.data;
    let engine, viz;
    try {
        [engine, viz] = await modules;
    } catch (error) {
        self.postMessage({
            id,
            failure: `The Mata Script interpreter could not be loaded: ${error?.message ?? error}`,
        });
        return;
    }
    if (request === null) {
        self.postMessage({ id, ready: true });
        return;
    }
    // The lines of a DOT graph are collected until it ends, and then posted as one drawing. `print` ends the graph's
    // text, which ends with a line break, with a line break of its own; that empty line is left out.
    let graph = null;
    let afterGraph = false;
    const onLine = (stream, text) => {
        const skip = afterGraph && stream === "out" && text === "";
        afterGraph = false;
        if (skip) return;
        if (stream === "out" && graph === null && isDotStart(text)) {
            graph = [text];
        } else if (stream === "out" && graph !== null) {
            graph.push(text);
            if (isDotEnd(text)) {
                const dot = graph.join("\n");
                graph = null;
                afterGraph = true;
                const lines = dot.split("\n");
                self.postMessage({
                    id,
                    dot: {
                        text: lines.slice(0, MAX_LISTED_LINES).join("\n"),
                        truncated: lines.length > MAX_LISTED_LINES,
                        ...draw(viz, dot),
                    },
                });
            }
        } else {
            self.postMessage({ id, line: { stream, text } });
        }
    };
    const started = performance.now();
    try {
        const status = engine.run(request, onLine);
        // An unfinished graph is shown as the lines it has.
        for (const text of graph ?? [])
            self.postMessage({ id, line: { stream: "out", text } });
        self.postMessage({
            id,
            done: { status, milliseconds: performance.now() - started },
        });
    } catch (error) {
        // An abort of the module, or an overflow of the browser's stack, leaves the module unusable. The browser's
        // stack is smaller than that of the `mata` command: a few hundred nested function calls of a script fill it.
        const overflow = error instanceof RangeError;
        self.postMessage({
            id,
            failure: overflow
                ? "The script nests too deeply for the browser's stack, so the interpreter stopped. Run it with the mata command instead."
                : `The interpreter failed: ${error?.message ?? error}`,
        });
    }
});
