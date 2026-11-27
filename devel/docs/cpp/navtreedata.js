/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Mata", "index.html", [
    [ "Mata: The Automata Library", "index.html", "index" ],
    [ "Nondeterministic Finite Automata (NFAs)", "nfa.html", [
      [ "The design of NFAs in Mata", "nfa.html#nfa_design", null ],
      [ "Working with NFAs", "nfa.html#nfa_usage", null ]
    ] ],
    [ "Nondeterministic Finite Transducers (NFTs)", "nft.html", [
      [ "The design of NFTs in Mata", "nft.html#nft_design", null ],
      [ "Working with NFTs", "nft.html#nft_usage", null ],
      [ "Representing words in NFTs", "nft.html#nft_word", null ]
    ] ],
    [ "Synchronized Iterator", "synchronized_iterator.html", null ],
    [ "Contributing guidelines", "contributing.html", [
      [ "Getting started", "contributing.html#getting-started", null ],
      [ "Getting Help", "contributing.html#getting-help", null ],
      [ "Bug Fixes", "contributing.html#bug-fixes", null ],
      [ "New Features", "contributing.html#new-features", null ],
      [ "Pull Request Process", "contributing.html#pull-request-process", [
        [ "Commit messages", "contributing.html#commit-messages", null ],
        [ "Referencing issues", "contributing.html#referencing-issues", null ]
      ] ],
      [ "Code and design style", "contributing.html#code-and-design-style", [
        [ "Code Formatting", "contributing.html#code-formatting", [
          [ "With Nix", "contributing.html#with-nix", null ],
          [ "Without Nix", "contributing.html#without-nix", null ]
        ] ],
        [ "Naming Conventions", "contributing.html#naming-conventions", null ],
        [ "TODO and similar message format", "contributing.html#todo-and-similar-message-format", null ],
        [ "Code quality", "contributing.html#code-quality", null ],
        [ "Testing", "contributing.html#testing", null ]
      ] ],
      [ "Development Tips", "contributing.html#development-tips", null ],
      [ "For maintainers", "contributing.html#for-maintainers", [
        [ "Versioning during PR merge", "contributing.html#versioning-during-pr-merge", null ]
      ] ]
    ] ],
    [ "Guidelines for Profiling, Performance, and Integration Testing of the Mata library", "benchmarking.html", [
      [ "Repositories", "benchmarking.html#repositories", null ],
      [ "Benchmarking in VeriFIT/mata", "benchmarking.html#benchmarking-in-verifitmata", [
        [ "Benchmarking through VeriFIT/mata <span class=\"tt\">./tests-integration/</span>", "benchmarking.html#benchmarking-through-verifitmata-tests-integration", [
          [ "Structure of the benchmarking infrastructure", "benchmarking.html#structure-of-the-benchmarking-infrastructure", null ],
          [ "Creating new benchmarking tests", "benchmarking.html#creating-new-benchmarking-tests", null ],
          [ "Run on one benchmark instance", "benchmarking.html#run-on-one-benchmark-instance", null ],
          [ "Run multiple benchmarks through the benchmarking tool <span class=\"tt\">pycobench</span>", "benchmarking.html#run-multiple-benchmarks-through-the-benchmarking-tool-pycobench", null ],
          [ "Additional benchmarking options", "benchmarking.html#additional-benchmarking-options", null ]
        ] ],
        [ "Optional optimization settings", "benchmarking.html#optional-optimization-settings", [
          [ "Profile-guided optimization", "benchmarking.html#profile-guided-optimization", null ]
        ] ],
        [ "Instrumenting the benchmarks for gcov", "benchmarking.html#instrumenting-the-benchmarks-for-gcov", null ],
        [ "Profiling", "benchmarking.html#profiling", [
          [ "Heap and stack space profiling", "benchmarking.html#heap-and-stack-space-profiling", null ],
          [ "Callgraph", "benchmarking.html#callgraph", null ]
        ] ]
      ] ]
    ] ],
    [ "The Mata file format for automata (.mata format)", "automata-format.html", [
      [ "Top-level file structure", "automata-format.html#top-level-file-structure", null ],
      [ "Alphabet type", "automata-format.html#alphabet-type", null ],
      [ "Automata by their transition type", "automata-format.html#automata-by-their-transition-type", [
        [ "NFA (Nondeterministic finite automata)", "automata-format.html#nfa-nondeterministic-finite-automata", null ],
        [ "AFA (Alternating finite automata)", "automata-format.html#afa-alternating-finite-automata", null ]
      ] ],
      [ "Distinguishing between states, symbols, and nodes", "automata-format.html#distinguishing-between-states-symbols-and-nodes", null ],
      [ "AFA and NFA Examples", "automata-format.html#afa-and-nfa-examples", null ],
      [ "Transducer", "automata-format.html#transducer", null ],
      [ "Special formulas and relations over symbols and transducer track names.", "automata-format.html#special-formulas-and-relations-over-symbols-and-transducer-track-names", null ],
      [ "Aliases", "automata-format.html#aliases", null ],
      [ "Attributes", "automata-format.html#attributes", null ],
      [ "How the Mata library reads the format", "automata-format.html#how-the-mata-library-reads-the-format", null ]
    ] ],
    [ "The `mata` command", "cli.html", [
      [ "Command tree", "cli.html#command-tree", null ],
      [ "Scripts", "cli.html#scripts", [
        [ "<span class=\"tt\">mata script run</span>", "cli.html#mata-script-run", null ],
        [ "<span class=\"tt\">mata script check</span>", "cli.html#mata-script-check", null ],
        [ "<span class=\"tt\">mata script repl</span>", "cli.html#mata-script-repl", null ]
      ] ],
      [ "Direct operations", "cli.html#direct-operations", [
        [ "Direct queries and exit status", "cli.html#direct-queries-and-exit-status", null ]
      ] ],
      [ "Conversion", "cli.html#conversion", [
        [ "Regex conventions", "cli.html#regex-conventions", null ],
        [ "DOT conventions", "cli.html#dot-conventions", null ]
      ] ],
      [ "Output conventions", "cli.html#output-conventions", null ],
      [ "Exit status", "cli.html#exit-status", null ],
      [ "Versions", "cli.html#versions", null ],
      [ "Why Rust", "cli.html#why-rust", null ],
      [ "Implementation notes", "cli.html#implementation-notes", null ]
    ] ],
    [ "(Auto)Mata Script Language", "script-language.html", [
      [ "A first example", "script-language.html#a-first-example", null ],
      [ "Design principles", "script-language.html#design-principles", null ],
      [ "Lexical structure", "script-language.html#lexical-structure", null ],
      [ "Grammar", "script-language.html#grammar", null ],
      [ "Values and sorts", "script-language.html#values-and-sorts", [
        [ "Checking", "script-language.html#checking", null ]
      ] ],
      [ "Definitions, functions and conditionals", "script-language.html#definitions-functions-and-conditionals", [
        [ "Functions", "script-language.html#functions", null ],
        [ "Pipelines", "script-language.html#pipelines", null ],
        [ "Lists", "script-language.html#lists", null ],
        [ "Loops and recursion", "script-language.html#loops-and-recursion", null ],
        [ "Termination", "script-language.html#termination", null ]
      ] ],
      [ "Inputs", "script-language.html#inputs", [
        [ "What loading does", "script-language.html#what-loading-does", null ]
      ] ],
      [ "Alphabets", "script-language.html#alphabets", null ],
      [ "Operations", "script-language.html#operations", [
        [ "Names", "script-language.html#names", null ],
        [ "Arguments", "script-language.html#arguments", null ],
        [ "One call per application", "script-language.html#one-call-per-application", null ],
        [ "NFA operations", "script-language.html#nfa-operations", null ],
        [ "NFT operations", "script-language.html#nft-operations", null ],
        [ "The <span class=\"tt\">automaton:</span> namespace", "script-language.html#the-automaton-namespace", null ],
        [ "The <span class=\"tt\">lazy:</span> namespace", "script-language.html#the-lazy-namespace", [
          [ "Plans and policies", "script-language.html#plans-and-policies", null ]
        ] ],
        [ "The <span class=\"tt\">ws1s:</span> namespace", "script-language.html#the-ws1s-namespace", null ],
        [ "Other namespaces", "script-language.html#other-namespaces", null ],
        [ "Core functions", "script-language.html#core-functions", null ]
      ] ],
      [ "Reporting results", "script-language.html#reporting-results", [
        [ "Exit status", "script-language.html#script-exit-status", null ],
        [ "Witnesses", "script-language.html#witnesses", null ]
      ] ],
      [ "Timing", "script-language.html#timing", null ],
      [ "Options", "script-language.html#options", null ],
      [ "Optimizer", "script-language.html#optimizer", null ],
      [ "Portable core", "script-language.html#portable-core", null ],
      [ "Script information", "script-language.html#script-information", null ],
      [ "Versions", "script-language.html#script-versions", null ],
      [ "Examples", "script-language.html#examples", null ],
      [ "Open questions", "script-language.html#open-questions", null ]
    ] ],
    [ "Branches", "branches.html", [
      [ "Main branches", "branches.html#main-branches", [
        [ "<a href=\"https://github.com/VeriFIT/mata/tree/master\"><span class=\"tt\">master</span></a>", "branches.html#masterhttpsgithubcomverifitmatatreemaster", null ],
        [ "<a href=\"https://github.com/VeriFIT/mata/tree/devel\"><span class=\"tt\">devel</span></a>", "branches.html#develhttpsgithubcomverifitmatatreedevel", null ]
      ] ],
      [ "Backups", "branches.html#backups", [
        [ "<a href=\"https://github.com/VeriFIT/mata/tree/simulation-samo538.bak\"><span class=\"tt\">simulation-samo538.bak</span></a>", "branches.html#simulation-samo538bakhttpsgithubcomverifitmatatreesimulation-samo538bak", null ],
        [ "<a href=\"https://github.com/VeriFIT/mata/tree/simulation-reduction-rules-samo538.bak\"><span class=\"tt\">simulation-reduction-rules-samo538.bak</span></a>", "branches.html#simulation-reduction-rules-samo538bakhttpsgithubcomverifitmatatreesimulation-reduction-rules-samo538bak", null ],
        [ "<a href=\"https://github.com/VeriFIT/mata/tree/boost-determinization-inclusion-lacko.bak\"><span class=\"tt\">boost-determinization-inclusion-lacko.bak</span></a>", "branches.html#boost-determinization-inclusion-lackobakhttpsgithubcomverifitmatatreeboost-determinization-inclusion-lackobak", null ]
      ] ]
    ] ],
    [ "Todo List", "todo.html", null ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Concepts", "concepts.html", "concepts" ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"alphabet_8hh.html",
"classmata_1_1AutomatonBase.html#ad7f7024bcf368b25ea66565db75eb574",
"classmata_1_1Delta.html#afa6896d1e96fdb060f87bd4fbc4134f0",
"classmata_1_1FormulaNode.html#ac5683341b82c078fc776615503e61c76",
"classmata_1_1NameStore.html#a9c4c8de0f5e4467da186e3e5606ba8c7",
"classmata_1_1StatePost.html#a79e51cc285d7db2a7947af9b40eb55ed",
"classmata_1_1applications_1_1strings_1_1replace_1_1ReluctantReplace.html#a0e50fd2a20531b85c75f87315c18edc0",
"classmata_1_1nfa_1_1Nfa.html#aa77634d156ad88686d320276223a316a",
"classmata_1_1nft_1_1Nft.html#a24380a09bc8571212f0437beba4470e2",
"classmata_1_1parser_1_1ExplicitSectionReader.html#a1eaab656caed8a37e17852de9b1b967faac101b32dda4448cf13a93fe283dddd8",
"classmata_1_1posts_1_1DeltaBase.html#ad06ddaa4a8be8736ea0b9b3459e1286a",
"classmata_1_1posts_1_1Post.html#a3a4d7a069691e20bb38362401065e958",
"classmata_1_1posts_1_1PostMoves_1_1const__iterator.html",
"classmata_1_1utils_1_1OrdVector.html#a52dea4c18991a1bbce8c4a5c0df6f485",
"classmata_1_1utils_1_1SmallOrdVector.html#aedeb8f47ba319dc6c19637f208266faf",
"cli.html#command-tree",
"namespacemata.html#abae2e619c22853c3329748e827d9414aaf87e1d9eeec5afa7dbae75f935e991f1",
"namespacemata_1_1nft.html#a4c5063d7075c5865ae36a2c66ad9915f",
"nfa.html#nfa_design",
"structmata_1_1OnTheFlyAlphabet_1_1SymbolNames.html#a4b03a814e986eae2ba7f9d20622d6d70",
"structmata_1_1parser_1_1BodyToken.html#af0f5d2de7e09cf6e6b3a5e40f784dbab",
"tarjan_8hh_source.html"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
var LISTOFALLMEMBERS = 'List of all members';