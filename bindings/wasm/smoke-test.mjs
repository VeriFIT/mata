// Checks the WebAssembly build of the playground under Node:
//
//   node bindings/wasm/smoke-test.mjs build-wasm/mata-playground.js

import assert from "node:assert/strict";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

const modulePath = resolve(process.argv[2] ?? "build-wasm/mata-playground.js");
const { default: createMataPlayground } = await import(
    pathToFileURL(modulePath).href
);
const mata = await createMataPlayground();

function evaluate(request) {
    const defaults = {
        aFormat: "regex",
        aText: "",
        bFormat: "regex",
        bText: "",
        word: "",
        alphabet: "used",
    };
    return JSON.parse(mata.evaluate({ ...defaults, ...request }));
}

const wordOf = (response) =>
    response.check.word.map((index) => response.symbols[index]).join("");

// The minimal DFA of the words ending in abb.
let response = evaluate({ aText: "(a|b)*abb", operation: "minimize" });
assert.equal(response.ok, true);
assert.equal(response.automata.result.states, 4);
assert.equal(response.automata.result.transitionCount, 8);
assert.equal(response.automata.result.deterministic, true);

// A counterexample to an inclusion.
response = evaluate({
    aText: "(a|b)*b",
    bText: "(a|b)*abb",
    operation: "is_included",
});
assert.equal(response.check.holds, false);
assert.match(wordOf(response), /^[ab]*b$/);
assert.doesNotMatch(wordOf(response), /abb$/);

// An equivalence.
response = evaluate({
    aText: "(ab)*a",
    bText: "a(ba)*",
    operation: "are_equivalent",
});
assert.equal(response.check.holds, true);

// An automaton in the .mata format with ε-transitions, combined with a regular expression.
const withEpsilon =
    "@NFA-explicit\n%Alphabet-auto\n%Initial s\n%Final f\ns a p\ns <eps> r\np b f\nr c f\n";
response = evaluate({
    aFormat: "mata",
    aText: withEpsilon,
    operation: "remove_epsilon",
});
assert.equal(response.ok, true);
assert.ok(response.automata.a.transitions.some(([, symbol]) => symbol === -1));
assert.equal(response.automata.a.deterministic, false);
assert.ok(
    response.automata.result.transitions.every(([, symbol]) => symbol !== -1),
);
response = evaluate({
    aFormat: "mata",
    aText: withEpsilon,
    bText: "c|ab",
    operation: "are_equivalent",
});
assert.equal(response.check.holds, true);

// Errors in the inputs are reported, which needs working C++ exceptions.
response = evaluate({ aText: "(a|b", operation: "show" });
assert.equal(response.ok, false);
assert.equal(response.source, "a");
response = evaluate({
    aFormat: "mata",
    aText: "@NFA-explicit\n%Alphabet-auto\ns a\n",
    operation: "show",
});
assert.equal(response.ok, false);
assert.equal(response.source, "a");

console.log(
    `The playground build of Mata (revision ${mata.revision()}) works.`,
);
