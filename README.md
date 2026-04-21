-# Redir: Cross-Platform Environment Filter Wrapper & Debug Proxy (Console)
-- The text were written by a Slop-Machine

-[Reason](https://www.codecoverage.de/posts/cpp/redir_proxy/)

**Redir** is a high-performance, cross-platform (Windows/Linux) wrapper for console applications, written in C++20. It enables precise environment variable filtering and provides a powerful diagnostic toolkit for deep process analysis.

## Core Features

- **Environment Filtering**: Precise addition (`+`) or removal (`-`) of variables via a configuration file.
- **Transparent Forwarding**: Signals (Ctrl+C, SIGUSR1/2, etc.) and exit codes are forwarded to the child process without delay.
- **I/O Proxy Mode**: Real-time monitoring of stdin, stdout, and stderr via binary dumps using dedicated relay threads.
- **Persistent Signal Logging**: Comprehensive logging of all incoming signals throughout the process lifecycle.
- **Static Binaries**: Fully independent, requiring no external runtime libraries.

## Diagnosis & Debugging (`REDIR_DEBUG`)

Diagnostic control is managed via the `REDIR_DEBUG` environment variable, which accepts a list of flags, separated by `,`.

### Available Flags:

| Flag | Description | File Suffix |
| :--- | :--- | :--- |
| `PRE_ENV` | Dumps the environment exactly as received by the wrapper. | `*_pre_env.txt` |
| `POST_ENV` | Dumps the environment after applying filters (`_conf.env`). | `*_post_env.txt` |
| `DUMP_ARGS` | Logs exact command-line arguments. | `*_args.txt` |
| `DUMP_PIPES` | Analyzes stream types (TTY, Pipe, File) before execution. | `*_pipes.txt` |
| `DUMP_SIGNALS`| Creates a persistent log of all received signals/events. | `*_signals.txt` |
| `DUMP_IO` | Activates the binary proxy for stdin, stdout, and stderr. | `*_stdin.bin`, etc. |

**Example Usage:**
```bash
# Enable multiple diagnostics on Linux
export REDIR_DEBUG="POST_ENV,DUMP_SIGNALS,DUMP_IO"
./myapp
```

### File Naming Convention
All diagnostic files follow a consistent schema for easy correlation:
`[AppName]_[PID]_[Timestamp]_[Category].[Extension]`

- **PID**: Process ID of the wrapper.
- **Timestamp**: High-resolution format `YYYYMMDD_HHMMSS_mmm` (millisecond precision).

> **NOTE: I/O Proxy (`DUMP_IO`) & Console Events**
> When `DUMP_IO` mode is active, standard streams on Windows are replaced by pipes. Pipes cannot transmit low-level console hardware events like `WINDOW_BUFFER_SIZE_EVENT`. Applications relying on window resize events will not receive them in proxy mode on Windows. 
> *On Linux, however, resize signals (`SIGWINCH`) are transparently forwarded by the wrapper, even when the I/O proxy is active.*

## Configuration (`*_conf.env`)

Configuration is managed via a file named after the executable, suffixed with `_conf.env`.

**Syntax:**
- `#` or `;` start comments.
- `+ KEY = VALUE`: Sets an environment variable (whitespaces around `=` are ignored).
- `- KEY`: Removes the variable from the child process's environment.

## Security & Stability Features

- **Memory Safety**: Consistent use of modern C++20 primitives (`std::string_view`, `std::unique_ptr`).
- **Resource Limits**: DoS protection by limiting config size (1MB) and line length (32KB).
- **Deadlock Prevention**: The I/O proxy uses background threads with `CancelSynchronousIo` (Win) / `pthread_cancel` (Linux) to prevent hangs on blocked console input.
- **Async-Signal Safety**: Linux signal logging uses secure system calls (`write`).

## Build Instructions

### Windows (MSVC)
Use `build.cmd` for a clean, `/analyze`-checked build.
```cmd
build.cmd
```

### Linux (GCC/Clang)
Use `build.sh` for a fully statically linked binary.
```bash
./build.sh
```

---
