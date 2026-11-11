// Helpers of the playground that do not need Mata: symbol names, words, labels, and drawing automata in the DOT
// language of Graphviz. They are shared by the page and its worker.

// The name of byte `byte` in Mata's playground: the character for printable ASCII characters, and \xHH otherwise.
// The library printer quotes a name when the reader would take it for something else. Backslash stays escaped: its
// own name would be a trailing backslash, which the .mata reader cannot take back from a quoted token. It must
// agree with byte_name() in bindings/wasm/playground.cc.
export function byteName(byte) {
    const char = String.fromCharCode(byte);
    if (byte > 0x20 && byte < 0x7f && byte !== 0x5c) return char;
    return `\\x${byte.toString(16).padStart(2, "0")}`;
}

// The byte a symbol name stands for, or undefined if it is not the name of a byte.
export function byteOfName(name) {
    if (name.length === 1) {
        const code = name.charCodeAt(0);
        return code > 0x20 && code < 0x7f && code !== 0x5c ? code : undefined;
    }
    const match = /^\\x([0-9a-f]{2})$/.exec(name);
    return match ? parseInt(match[1], 16) : undefined;
}

const SHORT_ESCAPES = { 0x09: "\\t", 0x0a: "\\n", 0x0d: "\\r", 0x20: "␣" };

// How a byte is shown in labels and words.
function showByte(byte) {
    if (byte in SHORT_ESCAPES) return SHORT_ESCAPES[byte];
    if (byte > 0x20 && byte < 0x7f) return String.fromCharCode(byte);
    return `\\x${byte.toString(16).padStart(2, "0")}`;
}

// A byte inside a bracket expression, where `]`, `\`, `^` and `-` need a backslash.
function showClassByte(byte) {
    const char = String.fromCharCode(byte);
    return "]\\^-".includes(char) ? `\\${char}` : showByte(byte);
}

function showRanges(bytes) {
    const parts = [];
    for (let i = 0; i < bytes.length;) {
        let j = i;
        while (j + 1 < bytes.length && bytes[j + 1] === bytes[j] + 1) j++;
        if (j - i >= 2)
            parts.push(`${showClassByte(bytes[i])}-${showClassByte(bytes[j])}`);
        else for (let k = i; k <= j; k++) parts.push(showClassByte(bytes[k]));
        i = j + 1;
    }
    return parts.join("");
}

// The label of an edge with the symbols named `names` (null stands for ε). A set of bytes is shown as a bracket
// expression of regular expressions, such as [a-z] or [^\n]; other names are listed.
export function edgeLabel(names) {
    const epsilon = names.includes(null);
    const symbols = names.filter((name) => name !== null);
    const parts = epsilon ? ["ε"] : [];
    if (symbols.length > 0) {
        const bytes = symbols.map(byteOfName);
        if (symbols.length === 1) {
            parts.push(
                bytes[0] === undefined ? symbols[0] : showByte(bytes[0]),
            );
        } else if (bytes.every((byte) => byte !== undefined)) {
            bytes.sort((x, y) => x - y);
            const present = new Set(bytes);
            const missing = [];
            for (let byte = 0; byte < 256; byte++)
                if (!present.has(byte)) missing.push(byte);
            parts.push(
                missing.length < bytes.length / 2
                    ? `[^${showRanges(missing)}]`
                    : `[${showRanges(bytes)}]`,
            );
        } else {
            parts.push(symbols.join(", "));
        }
    }
    const label = parts.join(", ");
    return label.length > 48 ? `${label.slice(0, 47)}…` : label;
}

const ESCAPED_BYTES = { n: 0x0a, r: 0x0d, t: 0x09, "\\": 0x5c };

// Splits what the user typed into the names of the symbols of a word over automaton A in `format`. For an automaton
// in the .mata format, the names are separated by spaces, so that a name can have several characters. A regular
// expression is over bytes: each character is a byte, and \xHH, \n, \r, \t and \\ stand for the bytes they name.
export function wordNames(text, format) {
    if (format === "mata") {
        const trimmed = text.trim();
        return trimmed === "" ? [] : trimmed.split(/\s+/);
    }
    const chars = [...text];
    const names = [];
    for (let i = 0; i < chars.length; i++) {
        let code = chars[i].codePointAt(0);
        if (chars[i] === "\\" && i + 1 < chars.length) {
            const hex = chars.slice(i + 2, i + 4).join("");
            if (chars[i + 1] === "x" && /^[0-9a-fA-F]{2}$/.test(hex)) {
                code = parseInt(hex, 16);
                i += 3;
            } else if (chars[i + 1] in ESCAPED_BYTES) {
                code = ESCAPED_BYTES[chars[i + 1]];
                i += 1;
            }
        }
        names.push(code < 256 ? byteName(code) : String.fromCodePoint(code));
    }
    return names;
}

// How the word with the symbols named `names` is shown: as a string when all symbols are bytes, with spaces between
// the names otherwise.
export function showWord(names) {
    if (names.length === 0) return "ε";
    const bytes = names.map(byteOfName);
    if (bytes.every((byte) => byte !== undefined))
        return bytes.map(showByte).join("");
    return names.join(" ");
}

// Regular expressions are over bytes (Latin-1). Characters from U+0080 to U+00FF are written as \xHH escapes, so that
// they reach RE2 as single bytes; a character above U+00FF cannot be used. An escaped character (such as \é) becomes
// its escape, and inside \Q...\E, where RE2 reads everything literally, the quote is closed around the escape.
export function regexToBytes(pattern) {
    const escape = (char) => {
        const code = char.codePointAt(0);
        if (code > 0xff) {
            throw new Error(
                `The character ${char} is not a byte. Regular expressions here are over bytes, so use characters up to U+00FF.`,
            );
        }
        return code >= 0x80 ? `\\x${code.toString(16)}` : undefined;
    };
    const chars = [...pattern];
    let result = "";
    let quoted = false;
    for (let i = 0; i < chars.length; i++) {
        const char = chars[i];
        const next = chars[i + 1];
        if (quoted) {
            if (char === "\\" && next === "E") {
                result += "\\E";
                quoted = false;
                i++;
            } else {
                const escaped = escape(char);
                result += escaped === undefined ? char : `\\E${escaped}\\Q`;
            }
        } else if (char === "\\" && next !== undefined) {
            const escaped = escape(next);
            if (next === "Q") quoted = true;
            result += escaped ?? `\\${next}`;
            i++;
        } else {
            result += escape(char) ?? char;
        }
    }
    return result;
}

const quote = (text) => `"${text.replace(/\\/g, "\\\\").replace(/"/g, '\\"')}"`;

// The DOT text of `automaton` (as described by the playground's evaluate()), with the symbols named `symbols`.
// Parallel transitions are merged into one edge.
export function toDot(automaton, symbols) {
    const finals = new Set(automaton.final);
    const lines = [
        "digraph {",
        '  graph [rankdir=LR, bgcolor="transparent", nodesep=0.3, ranksep=0.45, pad=0.1];',
        '  node [shape=circle, fontname="Courier", fontsize=13, width=0.42, height=0.42, fixedsize=false, margin=0.04];',
        '  edge [fontname="Courier", fontsize=13, arrowsize=0.7];',
    ];
    for (let state = 0; state < automaton.states; state++) {
        const classes = finals.has(state) ? "state final" : "state";
        const name = automaton.stateNames?.[state];
        const tooltip = name ? `q${state} (${name})` : `q${state}`;
        const shape = finals.has(state) ? ", shape=doublecircle" : "";
        lines.push(
            `  ${state} [label=${quote(String(state))}, class=${quote(classes)}, tooltip=${quote(tooltip)}${shape}];`,
        );
    }
    automaton.initial.forEach((state, index) => {
        lines.push(
            `  start${index} [shape=point, style=invis, width=0.01, height=0.01, label=""];`,
        );
        lines.push(`  start${index} -> ${state} [class="initial"];`);
    });
    const edges = new Map();
    for (const [source, symbol, target] of automaton.transitions) {
        const key = `${source} ${target}`;
        if (!edges.has(key)) edges.set(key, { source, target, names: [] });
        edges.get(key).names.push(symbol < 0 ? null : symbols[symbol]);
    }
    for (const { source, target, names } of edges.values()) {
        const label = edgeLabel(names);
        lines.push(
            `  ${source} -> ${target} [label=${quote(label)}, tooltip=${quote(label)}];`,
        );
    }
    lines.push("}");
    return lines.join("\n");
}

// The state of the playground in the fragment of its address, and back.
const STATE_KEYS = {
    aFormat: "af",
    aText: "a",
    bFormat: "bf",
    bText: "b",
    operation: "op",
    word: "w",
    alphabet: "alph",
};

export function encodeState(state) {
    const params = new URLSearchParams();
    for (const [key, short] of Object.entries(STATE_KEYS)) {
        if (state[key] !== undefined && state[key] !== "")
            params.set(short, state[key]);
    }
    return params.toString();
}

export function decodeState(fragment) {
    const params = new URLSearchParams(fragment.replace(/^#/, ""));
    const state = {};
    for (const [key, short] of Object.entries(STATE_KEYS)) {
        if (params.has(short)) state[key] = params.get(short);
    }
    return state;
}
