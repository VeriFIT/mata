# Contributing guidelines

If you'd like to contribute to the libmata,  please [fork the repository](https://github.com/VeriFIT/mata/fork),
create a new feature branch, and finally [create a new pull request](https://github.com/VeriFIT/mata/compare).

In case you run into some unexpected behaviour, error or anything suspicious either contact us directly through mail or
[create a new issue](https://github.com/VeriFIT/mata/issues/new/choose).
When creating a new issue, please, try to include everything necessary for us to know (such as the version, operating
system, etc.) so we can sucessfully replicate the issue.

## Design style

- **private** attributes of classes/structs should use `snake_case_` (note the trailing `_`).
- `snake_case` is to be used in context of:
    - variables, and
    - functions / class methods (which are neither constructors nor destructors).
- `UpperCamelCase` is to be used in the context of:
    - class/structure names,
    - aliases of types created using `using`,
    - constructors and destructors, and
    - elements of the type `enum` (prefer using `enum class`).

## Note to maintainers

By default, each merge automatically increases the `minor` version of the library
(i.e., `0.0.0 -> 0.1.0` ). This can be overruled using either tag `#patch` (increasing
patch version, i.e., `0.0.0 -> 0.0.1`) or `#major` (increasing major version, i.e.,
`0.0.0 -> 1.0.0`). This tag is specified in the merge message.

Generally, it is recommended to use `#major` for changes that introduces backward-incompatible
changes for people that used previous versions, and `#patch` for minor changes, such as bug-fixes,
performance fixes or refactoring.
