# Linux Shell in C++

A lightweight Unix-style shell implemented in C++ to explore process creation, command execution, pipes, I/O redirection, background processes, and Linux system calls.

## Features

- **External command execution** – runs programs with their arguments in a child process.
- **PATH lookup** – resolves command names by searching the directories in `PATH`; commands containing a `/` (e.g. `./program`, `/bin/ls`) are run directly.
- **Pipelines** – connects any number of commands with `|`.
- **Input/output redirection** – `<` reads stdin from a file, `>` writes stdout to a file (created or truncated).
- **Background execution** – a trailing `&` starts the command without waiting for it.
- **Background job tracking** – `myjobs` lists background processes with their PID, status (`Running` / `Done`) and command line.
- **Command history** – every executed command line is appended to `history.txt` in the current directory; `myhistory` prints it.

### Built-in commands

| Command     | Description                                   |
|-------------|-----------------------------------------------|
| `myjobs`    | List background processes and their status    |
| `myhistory` | Print the command history                     |
| `exit`      | Exit the shell (Ctrl-D also exits)            |

## Technical concepts

- **`fork()`** – each command in a pipeline runs in its own child process.
- **`execv()`** – replaces the child process image with the resolved program.
- **`waitpid()`** – the shell waits for all processes of a foreground pipeline, and polls finished background processes with `WNOHANG` so they are reaped and their status is updated.
- **`pipe()`** – creates the channel between consecutive pipeline stages.
- **`dup2()`** – rewires `stdin`/`stdout` onto pipe ends or redirection files before `execv()`.
- **File descriptors** – unused pipe ends are closed in both parent and child so readers receive EOF correctly.
- **Process management** – foreground vs. background execution and tracking of background jobs.

## Build and run

Requires Linux, a C++17 compiler (GCC or Clang) and CMake 3.10+.

```bash
cmake -S . -B build
cmake --build build
./build/shell
```

Or compile directly with g++:

```bash
g++ -std=c++17 -Wall -Wextra -o shell src/main.cpp
./shell
```

## Example session

```text
shell> ls -l
shell> ls | grep cpp
shell> cat < input.txt
shell> echo hello > output.txt
shell> sleep 10 &
Started process with PID: 4321
shell> myjobs
PID     Status          Command
4321    Running         sleep 10 &
shell> myhistory
ls -l
ls | grep cpp
cat < input.txt
echo hello > output.txt
sleep 10 &
shell> exit
```

## Limitations

This is a learning project, not a full shell. Tokens are split on whitespace, so operators must be separated by spaces (`ls | wc`, not `ls|wc`). Quoting, escaping, environment variable expansion, globbing, `cd` and other shell built-ins, `>>` append and `2>` redirection are not supported.

## Project structure

```text
.
├── CMakeLists.txt
├── README.md
└── src/
    └── main.cpp
```
