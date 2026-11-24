// The Mata Script interpreter compiled to WebAssembly (mata-playground-script.js, built from mata-cli with the feature
// `web`), without the page: the worker of the Script mode and the smoke test under Node share it.

// The exit statuses of `mata script run`.
export const EXIT_MEANINGS = {
    0: "The script ran to its end.",
    1: "An assertion failed.",
    2: "An error stopped the script.",
};

const INPUT_DIR = "/inputs";

// Loads the module created by `createModule`. Every run hands the lines the script writes, as ("out" | "err", text),
// to its own callback, as soon as they are written.
export async function loadScriptEngine(createModule) {
    let sink = null;
    const mata = await createModule({
        print: (text) => sink?.("out", text),
        printErr: (text) => sink?.("err", text),
    });
    mata.FS.mkdir(INPUT_DIR);
    return {
        // Runs `script` with the texts of its declared inputs and the values of its parameters (both objects keyed by
        // name), and returns the exit status. After `timeLimitMs` (0 for none), the interpreter stops the script
        // before its next form, call or loop round; a library operation that runs then is not interrupted.
        run({ script, inputs = {}, params = {}, timeLimitMs = 0 }, onLine) {
            const files = [];
            const inputLines = Object.entries(inputs).map(
                ([name, text], index) => {
                    const path = `${INPUT_DIR}/${index}.mata`;
                    mata.FS.writeFile(path, text);
                    files.push(path);
                    return `${name}=${path}`;
                },
            );
            const paramLines = Object.entries(params).map(([name, value]) => {
                if (/[\r\n]/.test(value))
                    throw new Error(
                        `The value of the parameter ${name} must be one line.`,
                    );
                return `${name}=${value}`;
            });
            sink = onLine;
            try {
                return mata.ccall(
                    "mata_script_run",
                    "number",
                    ["string", "string", "string", "number"],
                    [
                        script,
                        inputLines.join("\n"),
                        paramLines.join("\n"),
                        timeLimitMs,
                    ],
                );
            } finally {
                sink = null;
                for (const path of files) mata.FS.unlink(path);
            }
        },
    };
}

// The declarations of a script that the page asks values for: `declare-input` (an automaton) and `declare-param` (a
// value with a sort, and maybe a default). Comments are skipped; `declare-inputs` takes the positional inputs, which
// the playground does not give.
export function scriptDeclarations(script) {
    const code = script
        .split("\n")
        .map((line) => line.replace(/;.*$/, ""))
        .join("\n");
    const declarations = [];
    const pattern =
        /\(\s*(declare-input|declare-param)\s+([^\s()";]+)\s+([^\s()";]+|\([^()]*\))(?:\s+("(?:[^"\\]|\\.)*"|[^\s()";]+))?\s*\)/g;
    for (const [, form, name, sort, fallback] of code.matchAll(pattern)) {
        if (declarations.some((declaration) => declaration.name === name))
            continue;
        declarations.push({
            kind: form === "declare-input" ? "input" : "param",
            name,
            sort: sort.replace(/\s+/g, " "),
            fallback,
        });
    }
    return declarations;
}

// A DOT graph written by the script (`nfa:print-to-dot`), recognized line by line in the output.
export function isDotStart(line) {
    return /^\s*digraph\b.*\{\s*$/.test(line);
}

export function isDotEnd(line) {
    return /^\s*\}\s*$/.test(line);
}

// The number of states and transitions of the automaton a DOT graph of Mata draws, to decide whether to draw it. An
// edge with several symbols, `label="a,b"` (a name's own comma is escaped), is one transition per symbol.
export function dotSize(dot) {
    const states = new Set();
    let transitions = 0;
    for (const line of dot.split("\n")) {
        const edge = /^\s*(\w+)\s*->\s*(\w+)/.exec(line);
        if (edge) {
            // The arrows into initial states start at invisible nodes, which are not states.
            if (/^\d+$/.test(edge[1])) {
                const label = /label="((?:[^"\\]|\\.)*)"/.exec(line);
                transitions += label ? label[1].split(/(?<!\\),/).length : 1;
                states.add(edge[1]);
            }
            states.add(edge[2]);
            continue;
        }
        const node = /^\s*(\d+)\s*(\[|;|$)/.exec(line);
        if (node) states.add(node[1]);
    }
    return { states: states.size, transitions };
}
