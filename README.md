# Redir: Cross-Platform Environment Filter Wrapper
- Text written by Slop-Machine
  
**Redir** is a high-performance, cross-platform (Windows/Linux) environment variable filter and wrapper written in C++20. It allows you to transparently wrap any executable and modify its environment variables (add, update, or remove) before execution, without modifying the original binary.

## How It Works

The wrapper operates by acting as a proxy for a target executable:
1. **Naming Convention**: If your original application is `myapp.exe`, you rename it to `myapp_org.exe` and name the Redir binary `myapp.exe`.
2. **Configuration**: Redir looks for a configuration file named `myapp_conf.env` in the same directory.
3. **Redirection**: When `myapp.exe` (the wrapper) is launched, it reads the config, modifies the environment, and executes `myapp_org.exe` (the target), passing through all command-line arguments and standard I/O streams.

## Features

- **Surgical Environment Control**: Add (`+`) or remove (`-`) variables with precision.
- **Cross-Platform**: Native implementations for Windows (Win32 API) and Linux (POSIX/fork/exec).
- **Robust Parser**: Handles UTF-8 (with/without BOM), different line endings (LF/CRLF), and trims whitespace automatically.
- **Zero Dependencies**: Static builds ensure the wrapper runs on any system without extra DLLs or libraries.
- **Security Limits**: 1MB config file limit and 32KB per-line limit to prevent abuse.
- **Diagnostic Mode**: Can dump the resulting environment to a file for troubleshooting.

## Building the Project

### Prerequisites
- **Windows**: MSVC (Visual Studio) with C++20 support.
- **Linux**: GCC or Clang with C++20 support.

### Windows Build
Run the provided batch script in a Developer Command Prompt:
```cmd
build.cmd
```
This produces `redir.exe` and a `mock_org.exe` (for testing) using static linking (`/MT`).

### Linux Build
Run the shell script:
```bash
chmod +x build.sh
./build.sh
```
This produces a statically linked and stripped `redir` binary.

## Usage

### 1. Setup
1. Rename your target application to include the `_org` suffix (e.g., `tool.exe` -> `tool_org.exe`).
2. Copy the `redir` binary to the original name (`tool.exe`).
3. Create a configuration file `tool_conf.env`.

### 2. Configuration (`*_conf.env`)
The configuration file uses a simple prefix-based syntax:

```env
# Use '+' to add or overwrite a variable (format: + KEY = VALUE)
+ API_KEY = 12345-ABCDE
+ APP_DEBUG = true

# Use '-' to remove a variable (format: - KEY)
- HTTP_PROXY
- TEMP_SECRET

# Empty values are supported
+ CLEAR_VAR = 

# Comments start with # or ;
```

### 3. Execution Flags
Redir is controlled by specific environment variables:

| Variable | Value | Description |
| :--- | :--- | :--- |
| `REDIR_ENABLE_REDIR` | `1` | **Required**. Must be set to `1` to activate filtering. Otherwise, it performs a simple pass-through. |
| `REDIR_DUMP_ENV` | `1` | Optional. If set, dumps the final environment to `[exe]_pid_env.txt`. |
| `REDIR_ENABLE_U16TEXT`| `1` | (Windows only). Enables UTF-16 mode for console output. |

## Testing

The project includes an exhaustive "Dual-Mode Validation" suite that tests edge cases like BOM handling, whitespace robustness, and exit code propagation.

### Windows (PowerShell)
```powershell
./run_test.ps1
```

### Linux (Bash)
```bash
./run_test.sh
```

## Exit Codes

Redir propagates the exit code of the target application. If an error occurs within the wrapper itself, it returns:
- `100`: Internal/System Error (e.g., STL Exception).
- `101`: Configuration Error (e.g., invalid syntax in `.env`).
- `102`: Target Not Found (e.g., `*_org` file is missing).

---

## Credits & Evolution

This project is a sophisticated synthesis of **Vibe Coding** and **Augmented Coding**, developed in a collaborative partnership between a Senior Developer and **Gemini 3 Flash (Preview)**.

### The Hybrid Approach
*   **Vibe Coding (Discovery)**: The initial vision and cross-platform logic were driven by high-level intent and rapid iteration. This phase focused on "the vibe"—getting a functional tool for Windows and Linux into the world at lightning speed.
*   **Augmented Coding (Refinement)**: The project was then hardened using rigorous software engineering principles. This involved "Senior-level" architectural decisions like the **Bridge Pattern**, strict C++20 compliance, and exhaustive validation.

### An Honest Note on Process
While AI-assisted coding is fast in the "creative spark" phase, reaching production-ready quality (handling UTF-8 BOMs, cross-platform handle management, etc.) was a long and arduous journey. In fact, it's fair to say that a Senior Developer might have reached the finish line faster with traditional coding. 

However, the result is a unique "Vibe-Coded" artifact that stands as a testament to human-AI patience and the pursuit of perfection through hundreds of iterations.

