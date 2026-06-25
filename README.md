# NMD — Notepad++ Markdown Preview {#root}

A Notepad++ plugin that renders a live Markdown preview in a side panel powered by WebView2.

## Installation {#installation}

1. Build `NMD.dll` (Visual Studio 2022, x64 Release).
2. Copy it to `%APPDATA%\Notepad++\plugins\NMD\NMD.dll`.
3. Restart Notepad++.

The preview panel toggles via **Plugins → NMD → Toggle Preview**.

## Syntax Highlighting {#syntax-highlighting}

Fenced code blocks are highlighted automatically when a language tag is provided:

````markdown
```csharp
public record Point(int X, int Y);
```
````

Powered by [Shiki](https://shiki.style/) (loaded from CDN on first use — requires internet).

### Supported languages {#syntax-highlighting.supported-languages}

| Tag | Language |
|---|---|
| `bash` / `shell` | Bash / Shell |
| `cpp` | C++ |
| `csharp` | C# |
| `css` | CSS |
| `go` | Go |
| `html` | HTML |
| `javascript` | JavaScript |
| `json` | JSON |
| `markdown` | Markdown |
| `python` | Python |
| `rust` | Rust |
| `sql` | SQL |
| `typescript` | TypeScript |
| `xml` | XML |
| `yaml` | YAML |

Blocks without a language tag are rendered as plain text (no highlighting).

## Markdown Features {#markdown-features}

- CommonMark + GitHub Flavored Markdown (tables, task lists, strikethrough)
- Front-matter `acronyms:` block — defines abbreviations rendered as `<abbr>` tooltips
- Live preview updates 300 ms after the last keystroke
