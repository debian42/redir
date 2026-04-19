# Redir: Cross-Platform Environment Filter Wrapper & Debug Proxy (Console)
- The text were written by a Slop-Machine

[Reason](https://www.codecoverage.de/posts/cpp/redir_proxy/)

**Redir** ist ein hochperformanter, plattformübergreifender (Windows/Linux) Wrapper für Konsolenanwendungen, geschrieben in C++20. Er ermöglicht das gezielte Filtern von Umgebungsvariablen und bietet ein mächtiges Diagnose-Toolkit zur tiefgehenden Prozessanalyse.

## Kernfunktionen

- **Umgebungs-Filterung**: Präzises Hinzufügen (`+`) oder Entfernen (`-`) von Variablen über eine Konfigurationsdatei.
- **Transparente Weiterleitung**: Signale (Ctrl+C, SIGUSR1/2, etc.) und Exit-Codes werden ohne Verzug an den Child-Prozess durchgereicht.
- **I/O Proxy Mode**: Echtzeit-Monitoring von stdin, stdout und stderr über binäre Dumps mittels dedizierter Relay-Threads.
- **Persistent Signal Logging**: Protokollierung aller eintreffenden Signale während der gesamten Laufzeit.
- **Statische Binaries**: Komplett unabhängig von externen Laufzeitbibliotheken.

## Diagnose & Debugging (`REDIR_DEBUG`)

Die Steuerung der Diagnose erfolgt über die Umgebungsvariable `REDIR_DEBUG`. Diese erwartet eine Liste von Flags (getrennt durch `,`).

### Verfügbare Flags:

| Flag | Beschreibung | Dateisuffix |
| :--- | :--- | :--- |
| `PRE_ENV` | Dumpt das Environment exakt so, wie der Wrapper es empfangen hat. | `*_pre_env.txt` |
| `POST_ENV` | Dumpt das Environment nach Anwendung aller Filter (`_conf.env`). | `*_post_env.txt` |
| `DUMP_ARGS` | Protokolliert die vollständigen Kommandozeilenargumente. | `*_args.txt` |
| `DUMP_PIPES` | Analysiert die Stream-Typen (TTY, Pipe, File) vor dem Start. | `*_pipes.txt` |
| `DUMP_SIGNALS`| Erzeugt ein permanentes Log aller empfangenen Signale/Events. | `*_signals.txt` |
| `DUMP_IO` | Aktiviert den Mitlese-Proxy für stdin, stdout und stderr. | `*_stdin.bin`, etc. |

**Beispiel für die Aktivierung:**
```bash
# Mehrere Diagnosen gleichzeitig unter Linux
export REDIR_DEBUG="POST_ENV,DUMP_SIGNALS,DUMP_IO"
./myapp
```

### Dateinamenschema
Alle Diagnose-Dateien folgen einem konsistenten Schema für eindeutige Zuordnung:
`[AppName]_[PID]_[Zeitstempel]_[Kategorie].[Endung]`

- **PID**: Die Prozess-ID des Wrappers.
- **Zeitstempel**: Hochauflösend im Format `YYYYMMDD_HHMMSS_mmm` (Millisekunden-Präzision).

## Konfiguration (`*_conf.env`)

Die Filter-Logik wird über eine Datei gesteuert, die den Namen der ausführbaren Datei trägt, ergänzt um das Suffix `_conf.env`.

**Syntax:**
- `#` oder `;` leiten Kommentare ein.
- `+ KEY = VALUE`: Setzt eine Variable (Whitespaces um das `=` werden automatisch entfernt).
- `- KEY`: Entfernt die Variable aus der Umgebung des Kindprozesses.

## Sicherheits- & Stabilitätsmerkmale

- **Memory Safety**: Konsequente Nutzung von modernem C++20 (`std::string_view`, `std::unique_ptr`).
- **Ressourcen-Limits**: Schutz vor DoS durch Begrenzung der Konfigurationsgröße (1MB) und Zeilenlänge (32KB).
- **Deadlock-Prävention**: Der I/O-Proxy nutzt Hintergrund-Threads mit `CancelSynchronousIo` (Win) / `pthread_cancel` (Linux), um Hänger bei blockierenden Konsolen-Eingaben zu vermeiden.
- **Async-Signal Safety**: Linux-Signal-Logging nutzt ausschließlich sichere System-Calls (`write`).

## Build-Anweisungen

### Windows (MSVC)
Nutze die `build.cmd` für einen `/analyze`-geprüften Build.
```cmd
build.cmd
```

### Linux (GCC/Clang)
Nutze die `build.sh` für eine komplett statisch gelinkte Binary.
```bash
./build.sh
```

---

