# Contributing guidelines

In case you run into some unexpected behaviour, error or anything suspicious either contact us directly through mail or
[create a new issue](https://github.com/VeriFIT/mata/issues/new/choose).
When creating a new issue, please, try to include everything necessary for us to know (such as the version, operating
system, etc.) so we can sucessfully replicate the issue.

If you'd like to contribute to the Mata project, please [fork the repository](https://github.com/VeriFIT/mata/fork),
create a new feature branch, and finally [create a new pull request](https://github.com/VeriFIT/mata/compare).
When opening a PR, either set its status to `Open` (the default behaviour) when you want the PR to be immediately
up for review, or set its status to `Draft` when the PR is work-in-progress and you do not want the PR to be
reviewed yet.
When the draft PR is finished and ready for review, switch the status to `Open` to indicate we should review the PR.
Optionally, you can tag `@Adda0` (and any other contributors you explicitly want to review the PR) to request the
review directly, be it an open PR or a draft.

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

### TODO and similar message format

When commenting on a future TODO, an unresolved issue to be fixed, a bug or an unresolved note which is to be
resolved later on, use one of the following comment formats:

```cpp
// TODO: <message>.
// FIXME: <message>.
// BUG: <message>.
// NOTE: <message>.
```

For normal, permanent comments, `// NOTE:` should not be used (use just `// <message>.` instead).
Use `// NOTE:` only when specifically making a note supposed to be resolved later on
and which is to be removed afterwards.

Optionally, to limit the scope of the comment, `// TYPE(<scope>): <message>` can be used, e.g., `// TODO(nfa): Add a
new function.`.

### Code quality

If possible, go over the code and extract all repeatedly computed code in {for, while, do-while} loop bodies,
but also in loop declarations (headers) such as:

```cpp
for (auto it{ xyz.begin() }; it < xyz.end(); it++)
```

to:

```cpp
for (auto it{ xyz.begin() }, xyz_end{ xyz.end() }; it < xyz_end ; ++it) // Note: Using prefix is more performant. Use
whenever possible.
```

or (if the iterated variable and the end condition variable are not of the same data type, or you want to make the
condition variable explicitly `const`):

```cpp
const auto xyz_end{ xyz.end() };
for (auto it{ xyz.begin() }; it < xyz_end ; ++it) // Note: Using prefix is more performant. Use whenever possible.
```

# For maintainers

The information in this section concerns only maintainers of the project.

## Versioning during PR merge

By default, each merge automatically increases the `minor` version of the library
(i.e., `0.0.0 -> 0.1.0` ). This can be overruled using either tag `#patch` (increasing
patch version, i.e., `0.0.0 -> 0.0.1`) or `#major` (increasing major version, i.e.,
`0.0.0 -> 1.0.0`). This tag is specified in the merge message.

Generally, it is recommended to use `#major` for changes that introduces backward-incompatible
changes for people that used previous versions, and `#patch` for minor changes, such as bug-fixes,
performance fixes or refactoring.
