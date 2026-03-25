# Contributing guidelines

Thank you for your interest in contributing to `mata`.

We welcome contributions from the community and appreciate your help in improving this project.

For information on contributing to the Python bindings, refer to [bindings/python/CONTRIBUTING.md](bindings/python/CONTRIBUTING.md).

## Getting started

1. If you'd like to contribute to the Mata project, please [fork the repository](https://github.com/VeriFIT/mata/fork).
2. Clone your fork locally:

   ```shell
   git clone https://github.com/YOUR_USERNAME/mata.git
   cd mata
   ```

3. Create a new feature branch.
4. Build the project.
   Refer to [README#Building and installing from sources](README#Building-and-installing-from-sources) for instructions
   on how to build the project from sources.
5. Commit the changes and push to your fork.
6. Finally, [create a new pull request](https://github.com/VeriFIT/mata/compare).
   When opening a PR, either set its status to `Open` (the default behaviour) when you want the PR to be immediately
   up for review, or set its status to `Draft` when the PR is work-in-progress and you do not want the PR to be
   reviewed yet.
   When the draft PR is finished and ready for review, switch the status to `Open` to indicate we should review the PR.
   Optionally, you can tag `@Adda0` (and any other contributors you explicitly want to review the PR) to request the
   review directly, be it an open PR or a draft.

## Getting Help

In case you run into some unexpected behaviour, error or anything suspicious, either contact us directly through mail or
[create a new issue](https://github.com/VeriFIT/mata/issues/new/choose).
When creating a new issue, please, try to include everything necessary for us to know (such as the version, operating
system, etc.) so we can sucessfully replicate the issue.

- Check the [wiki](https://github.com/VeriFIT/mata/wiki) for detailed documentation.
- Look at existing issues and discussions for similar problems or questions.
- Feel free to ask questions in new issues or discussions.

## Bug Fixes

**Bug fix pull requests are always welcome.**
If you have found a bug and have a fix for it, feel free to submit a pull request directly.
Please include:

- A clear description of the bug you're fixing.
- Steps to reproduce the issue (if applicable).
- Your solution and why it fixes the problem.

## New Features

For new features, we prefer a discussion-first approach:

1. **Open an issue first** to discuss the feature and potential implementations.
2. Wait for feedback from maintainers and the community.
3. Implement the agreed-upon design.
4. Submit a pull request with the implementation.

This process helps ensure that:

- The feature aligns with the project's goals.
- We avoid duplicate work.
- The implementation follows the project's patterns and conventions.

## Pull Request Process

1. Ensure your code builds successfully. <!-- 2. If you have made a change to the dependencies then update the `nix/vendor-hash` file. -->
2. (Optional) Submit your pull request with screenshots (if applicable).

## Code and design style

Follow standard C++ conventions and formatting.

### Code Formatting

Do not forget to format your code.
This project uses [treefmt](https://github.com/numtide/treefmt) to format the code.

#### With Nix

```shell
nix fmt
```

#### Without Nix

Install `treefmt` and the required formatters, then run:

```shell
treefmt
```

For required formatters, refer to [.treefmt.toml](.treefmt.toml).

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
for (auto it{ xyz.begin() }, xyz_end{ xyz.end() }; it < xyz_end ; ++it)
// Note: Using prefix is more performant. Use whenever possible.
```

or (if the iterated variable and the end condition variable are not of the same data type, or you want to make the
condition variable explicitly `const`):

```cpp
const auto xyz_end{ xyz.end() };
for (auto it{ xyz.begin() }; it < xyz_end ; ++it) // Note: Using prefix is more performant. Use whenever possible.
```

### Testing

- Write unit tests for multiple cases, including edge cases.

## Development Tips

- The main entry point is `include/mata/nfa/nfa.hh`, `include/mata/nft/nft.hh`, etc.

<!-- TODO: - Set `DEBUG=1` environment variable for printing debug messages to `debug.log` file -->

## For maintainers

The information in this section concerns only maintainers of the project.

### Versioning during PR merge

By default, each merge automatically increases the `minor` version of the library
(i.e., `0.0.0 -> 0.1.0` ). This can be overruled using either tag `#patch` (increasing
patch version, i.e., `0.0.0 -> 0.0.1`) or `#major` (increasing major version, i.e.,
`0.0.0 -> 1.0.0`). This tag is specified in the merge message.

Generally, it is recommended to use `#major` for changes that introduces backward-incompatible
changes for people that used previous versions, and `#patch` for minor changes, such as bug-fixes,
performance fixes or refactoring.

---

Thank you for contributing to Mata!
