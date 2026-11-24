// Checks the WebAssembly build of the Mata Script interpreter for the Script mode of the playground under Node, and
// runs every script example of the playground with it:
//
//   node bindings/wasm/script-smoke-test.mjs build/wasm-script/wasm32-unknown-emscripten/release/mata-playground-script.js

import assert from "node:assert/strict";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

const playground = new URL("../../site/playground/", import.meta.url);
const { dotSize, isDotEnd, isDotStart, loadScriptEngine, scriptDeclarations } =
    await import(new URL("script-engine.js", playground).href);
const { SCRIPT_EXAMPLE_GROUPS } = await import(
    new URL("examples.js", playground).href
);
const { decodeState, encodeState } = await import(
    new URL("automata.js", playground).href
);

const modulePath = resolve(
    process.argv[2] ??
        "build/wasm-script/wasm32-unknown-emscripten/release/mata-playground-script.js",
);
const { default: createMataPlaygroundScript } = await import(
    pathToFileURL(modulePath).href
);
const engine = await loadScriptEngine(createMataPlaygroundScript);

// Runs a script and returns its exit status and the lines it wrote to each stream.
function run(request) {
    const out = [];
    const err = [];
    const status = engine.run(request, (stream, text) =>
        (stream === "out" ? out : err).push(text),
    );
    return { status, out, err };
}

// The address of the page holds a script with the values of its declarations.
const shared = {
    mode: "script",
    script: '(declare-input a Nfa)\n(echo "#&=?")',
    inputs: { a: "@NFA-explicit\n%Initial q\n" },
    params: { "n.m": "1 2" },
};
assert.deepEqual(decodeState(`#${encodeState(shared)}`), shared);

// One script end to end: a declared input and parameter, records, a printed word, and a DOT graph, whose size the
// page reads from its text and the library reports.
const NFA =
    "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\nq1 b q1\nq1 c q1\nq1 a q2\nq2 b q0\n";
const script = `(mata-script "0.1.0")
(declare-input a Nfa)
(declare-param letter String "b")
(check "accepts" (nfa:in-language? a (word "a" letter)))
(check "empty" (nfa:language-empty? a))
(print (nfa:get-word a))
(define m (nfa:minimize (nfa:determinize a)))
(echo (to-string (nfa:number-of-states m)))
(echo (to-string (nfa:number-of-transitions m)))
(print (nfa:print-to-dot m))
`;
assert.deepEqual(scriptDeclarations(script), [
    { kind: "input", name: "a", sort: "Nfa", fallback: undefined },
    { kind: "param", name: "letter", sort: "String", fallback: '"b"' },
]);
let result = run({ script, inputs: { a: NFA } });
assert.equal(result.status, 0, result.err.join("\n"));
assert.deepEqual(result.err, []);
assert.deepEqual(result.out.slice(0, 3), [
    "accepts: true",
    "empty: false",
    "a",
]);
const start = result.out.findIndex(isDotStart);
const end = result.out.findIndex(isDotEnd);
assert.ok(start > 0 && end > start, "a DOT graph is printed");
assert.deepEqual(dotSize(result.out.slice(start, end + 1).join("\n")), {
    states: Number(result.out[3]),
    transitions: Number(result.out[4]),
});
result = run({ script, inputs: { a: NFA }, params: { letter: "a" } });
assert.equal(result.out[0], "accepts: false");

// A failed assertion, and an error with its place; the module keeps working after both.
result = run({ script: "(assert false)" });
assert.equal(result.status, 1);
assert.match(result.err.join("\n"), /assertion failed at 1:1/);
result = run({ script: "(echo (nfa:no-such-operation))" });
assert.equal(result.status, 2);
assert.match(result.err.join("\n"), /^mata: .*nfa:no-such-operation/);
result = run({ script: '(echo "still here")' });
assert.deepEqual(result, { status: 0, out: ["still here"], err: [] });

// The time limit stops an endless loop; what the script printed before stays.
const started = performance.now();
result = run({
    script: '(set-option :loops-and-recursion true)\n(echo "before")\n(loop ((i 0)) (recur i))',
    timeLimitMs: 300,
});
assert.ok(performance.now() - started < 20_000);
assert.equal(result.status, 2);
assert.deepEqual(result.out, ["before"]);
assert.match(result.err.join("\n"), /time limit of 0\.3 s/);

// Nested function calls of a script within the browser's stack.
result = run({
    script: "(set-option :loops-and-recursion true)\n(define (f n) (if (= n 0) 0 (+ 1 (f (- n 1)))))\n(print (f 200))",
});
assert.deepEqual(result.out, ["200"]);

// Every script example of the playground.
let examples = 0;
for (const group of SCRIPT_EXAMPLE_GROUPS) {
    for (const example of group.examples) {
        result = run({
            script: example.script,
            inputs: example.inputs,
            params: example.params,
        });
        assert.equal(
            result.status,
            example.status ?? 0,
            `${example.title}:\n${result.err.join("\n")}`,
        );
        assert.deepEqual(result.err, [], example.title);
        // Every declared input has a text, so that the page can run the example as it loads it.
        for (const { kind, name } of scriptDeclarations(example.script)) {
            if (kind === "input")
                assert.ok(
                    example.inputs?.[name],
                    `${example.title}: input ${name}`,
                );
        }
        examples++;
    }
}

console.log(
    `The Mata Script build of the playground works, and so do its ${examples} script examples.`,
);
