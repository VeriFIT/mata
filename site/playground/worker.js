// The worker of the playground. It runs Mata and Graphviz, both compiled to WebAssembly, so that a long computation
// does not block the page; the page stops a computation that takes too long by terminating the worker.

import createMataPlayground from "./mata-playground.js";
import { instance as createViz } from "./vendor/viz.js";
import { toDot } from "./automata.js";

// Larger automata are not drawn: Graphviz would take long, and the drawing would not be readable.
const MAX_DRAWN_STATES = 120;
const MAX_DRAWN_TRANSITIONS = 400;

const modules = Promise.all([createMataPlayground(), createViz()]);

function draw(viz, automaton, symbols) {
    if (
        automaton.transitions === undefined ||
        automaton.states > MAX_DRAWN_STATES ||
        automaton.transitionCount > MAX_DRAWN_TRANSITIONS
    ) {
        return null;
    }
    return viz.renderString(toDot(automaton, symbols), {
        format: "svg",
        engine: "dot",
    });
}

self.addEventListener("message", async (event) => {
    const { id, request } = event.data;
    let mata, viz;
    try {
        [mata, viz] = await modules;
    } catch (error) {
        self.postMessage({
            id,
            failure: `Mata could not be loaded: ${error?.message ?? error}`,
        });
        return;
    }
    if (request === null) {
        self.postMessage({ id, revision: mata.revision() });
        return;
    }
    try {
        const response = JSON.parse(mata.evaluate(request));
        const drawings = {};
        if (response.ok) {
            for (const [key, automaton] of Object.entries(response.automata)) {
                drawings[key] = draw(viz, automaton, response.symbols);
            }
        }
        self.postMessage({ id, response, drawings });
    } catch (error) {
        self.postMessage({
            id,
            failure: `The computation failed: ${error?.message ?? error}`,
        });
    }
});
