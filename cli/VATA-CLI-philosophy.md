# VATA-CLI Philosophy

The aim of this document is to describe the philosophy behind VATA2 CLI.
The intention is to have a binary (`./vata-code`) that serves as an interface to libVATA2.  `./vata-code` will have a
very simple interface: `./vata-code` will accept only a single file in the `.vtf` format (or it can read the standard input).
The `.vtf` file should have (at least) one `@CODE` section, which will give instructions to `./vata-code` about what to do.

To give a user-friendlier interface, there will be another executable `./vata` (probably a Python script), which will accept
higher-level commands and transform them into `.vtf` files.
For example, the command
```
./vata union aut1.vtf aut2.vtf
```
would generate the following `.vtf` file and then pass it into `./vata`:
```
@CODE
a1 = (load_aut "aut1.vtf")
a2 = (load_aut "aut2.vtf")
a3 = (union a1 a2)
(print a3)
```

## Future Directions
* `./vata` could be executed in an interactive (REPL) mode
