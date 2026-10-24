# Ink Diary 4.1 — reMarkable 2 (Claude port)

> **Claude port.** This tree replaces the OpenAI back end with Anthropic's Messages API. See **PORT_NOTES.md** for what changed, what was dropped (artwork generation) and what is untested. The bundled `bin/ink-diary.so` was built from this source with the rM2 3.27 SDK and passes the host tests, but has not yet been tested on a device. Sections below that still mention OpenAI, `gpt-*` models or artwork generation describe the original and are superseded by PORT_NOTES.md.

**Installation and configuration: [INSTALL.md](INSTALL.md).**

This repository contains version 4.1 source (Claude port), the prebuilt config helper, tests, assets, licenses and installation scripts. It contains no API credentials. Configure your own key privately on the tablet.

A ChatGPT button inside the standard notebook app. Tap it to send the visible notebook area to Anthropic's Claude API over the tablet's Wi-Fi, render the answer in Caveat handwriting, and insert it beneath your writing as a single handwriting image. No companion computer is needed after setup.

Target: **reMarkable 2, OS 3.27.3.0**. This is a development prototype, not a published Vellum catalog package. Hardware validation is in progress; see VALIDATION.md for the actual tested state.

## Demo mode

For offline testing, config.json may contain `{"demo_mode":true}`. Holding the icon shows **ChatGPT (demo)** and inserts a fixed handwriting sample below your writing without making any API request. Running `configure.sh` replaces demo mode with the real API configuration.

## First use

1. Configure your own Anthropic API key privately in the reManager terminal:
   ```sh
   sh /home/root/.config/ink-diary/configure.sh
   ```
   The key entry is hidden. The default model is `claude-sonnet-5-5`; another vision-capable Claude model (for example `claude-haiku-5-5`, cheaper and faster) may be entered. API usage is billed to your Anthropic API account. A ChatGPT app conversation is not created.
2. Enable Wi-Fi. Open a disposable notebook with a **blank or lightly ruled template**, in **portrait orientation**, at normal zoom.
3. The answer image uses black handwriting independently of the selected pen.
4. Write a short question anywhere in the visible capture area, including at the top, away from the left toolbar. Leave the lower half clear.
5. Tap the **ChatGPT icon** in the bottom-right. Keep the page still and lift your pen until it finishes. While working, the icon becomes a stop square; tap it to cancel. Completion and error messages appear briefly above the button. Hold the icon to see its current status.
6. The answer is laid out in the Caveat handwriting font and inserted as one transparent image. Letters are not editable pen strokes. The native notebook app controls image sizing and selection; placement and sizing need device validation.

## Web search

Write `Search` followed by a question to explicitly request a web lookup, for example `Search what is the screen resolution of reMarkable 2?`. Questions about current facts, news, weather, prices and schedules are also routed to search automatically by the model. Include a location when needed. Ordinary timeless questions and function plots do not normally need a lookup.

The existing image-reading request transcribes/routes the question; search requests then make a second Messages API call with Anthropic's `web_search` tool enabled (up to three searches). The tool is not forced, so the plugin rejects any answer that lacks a completed search. The plugin checks for a completed search and citation metadata. Failed searches still produce a status message. A completed search without citation links (which can happen with weather results) now displays its answer with an explicit source-links-unavailable notice; links are never invented. Automatic routing and handwriting recognition can still make mistakes; use the explicit Search command when a lookup matters.

Answers include numbered citations and a source list with the API-provided titles and full URLs. Sources use a smaller print font for legibility. Notebook PNGs are not clickable. Each searched answer also has a sibling `.png.html` file in `/home/root/.local/share/ink-diary/answers/` containing the same answer with clickable inline citations; copy it through reManager to read in a browser. Sources are extracted from API annotations, not invented by the model. HTML text and links are escaped.

Search requests use your local timezone, currently `America/New_York`, with automatic daylight-saving conversion. A tablet-generated “Checked at” line records when the response arrived; it is not the weather observation time. Another IANA timezone can be set with the optional `timezone` configuration field.

Search adds latency and API tool charges. Your existing key and model are retained; the model must support the hosted web-search tool. The question text and relevant notebook context are sent in the second request. Source files are root-only and persist with answer PNGs. Demo mode still makes no API requests.

## Plot commands

Write `plot` followed by one equation, then tap the ChatGPT icon. Examples:

- `plot y = x^2`
- `plot y = sin(x)`
- `plot y = 2x + 1 from x = -10 to 10`

Handwritten superscripts are accepted through image recognition. The model transcribes the equation; a restricted on-tablet math parser computes the curve. No generated code is executed and no image-generation service is used. The interpreted equation appears above the graph so you can check recognition.

The default x range is -5 to 5; y is automatically scaled from sampled values. Axes have independent scales. Trigonometric functions use radians; ln/log mean natural logarithm. Supported operations: +, -, multiplication, division, powers, parentheses, sin, cos, tan, sqrt, abs, exp, ln, log, pi and e. Only explicit single-variable functions y=f(x) are supported, not implicit equations, multiple curves or parametric plots. Very narrow features can be missed by sampling; undefined values and detected asymptotes break the curve.

Answers and plots append below existing page content, extending beyond the visible screen when necessary. Scroll down to see the rest. Graphs use a white background for legibility over ruled templates and are inserted as image objects. Ordinary questions retain handwriting-image answers. Demo mode remains the original fixed text sample. The configured model must support Responses API Structured Outputs (the default gpt-4.1-mini does).

## Generated images and diagrams

Write a request and tap the same ChatGPT icon. Examples:

- `Create a Mermaid flowchart for making tea.`
- `Create a sequence diagram of a tablet sending a question to Anthropic.`
- `Create a Venn diagram comparing cats and dogs.`

Artwork requests (pictures, comics, manga) are not supported in the Claude port: Claude does not generate raster images, so the plugin replies with a short notice and suggests a diagram. Flowcharts, sequence diagrams and Venn diagrams below are unaffected.

Diagrams are drawn locally with 3–4 px black lines and print labels. No third-party rendering service or companion computer is used. The model produces a restricted Mermaid subset: flowcharts using TD/TB source syntax (up to 12 nodes and 24 arrows), laid out in a three-column wrapping grid, or sequence diagrams (2–5 participants and up to 20 messages). This is a native subset renderer, not the complete Mermaid JavaScript engine. Class, ER, Gantt, state diagrams, arbitrary Mermaid directives and mixed Markdown/diagram documents are not supported yet. Flowcharts support rectangular, rounded and diamond nodes, arrows and short edge labels. Labels may be quoted or unquoted; inline node declarations, chained arrows, semicolon-separated statements and implicit node references are accepted. Complex branching/loop layouts may need a simpler request.

Venn diagrams support two or three sets with numbered exclusive regions and a readable content legend underneath. Circle areas are not proportional to counts. Both blank templates and labeled comparisons work. Artwork prompts and diagram source are saved privately beside answer PNGs as `.png.source.txt`; ordinary text retains `.png.md`. The frozen version 3.0 release is unchanged.

## Prototype limits

- Portrait orientation and short answers only. Light gray full-width horizontal rules and thin vertical margin lines (including those partly hidden by plots) are filtered out when locating handwriting. Dark or complex templates may still be treated as ink.
- The working area is screen pixels x=150..1323, y=0..1751. Keep questions there. The side and bottom UI margins are cropped out of the uploaded screenshot; the top edge is included.
- Native image insertion may extend the notebook page below the viewport. Scroll down to follow it; there is no automatic scrolling or new-page creation. The visible question still needs to be inside the capture area. Answers have a fixed image-size limit, independent of remaining screen space.
- Visible page content supplies conversation context; there is no separate persistent conversation history.
- Page identity and a second screenshot are checked before insertion. Keep the page still until completion. Native image insertion is asynchronous; cancellation cannot retract an insertion already handed to the notebook app.
- Font coverage limits output languages and symbols. Unsupported characters produce an error rather than missing glyphs.
- Native image insertion and screen-to-page coordinate mapping must be supported by the installed firmware.
- Firmware updates require revalidation. The native extension refuses to load outside 3.27.3.0; remove its QMD patch before changing firmware.

## Installation from this archive

Install Vellum/reManager and a working xovi/qt-resource-rebuilder setup first. Copy this directory to the tablet, then run as root:

```sh
cd /home/root/ink-diary-setup
sh install.sh
sh /home/root/.config/ink-diary/configure.sh
```

The installer adds screenshot/message-broker dependencies through Vellum, installs the `.so`, notebook patch and font, and preserves an existing configuration. It does not restart the UI. After returning to the library, load the extension with:

```sh
setsid bash /home/root/xovi/start >/tmp/ink-diary-start.log 2>&1 </dev/null &
```

## Removal

```sh
sh /home/root/ink-diary-setup/uninstall.sh
```

Restart the UI afterwards. Your notebooks and existing Vellum packages remain. The API key/configuration remains at `/home/root/.config/ink-diary/config.json`; remove that specific file if you want to delete the saved credential.

## Privacy and network behavior

Only tapping ChatGPT starts a request. The cropped visible notebook image goes to `https://api.anthropic.com/v1/messages` over HTTPS with certificate verification. Retention follows your Anthropic account and organization settings. Screenshots use a private temporary directory and are deleted on normal completion/error. Answer PNGs remain in the root-only directory `/home/root/.local/share/ink-diary/answers` because the notebook app imports them asynchronously. An app crash may leave temporary files until reboot. Diagram rendering remains local after the initial transcription request. Notebook content and credentials are not printed to logs. The API key is saved locally with root-only permissions.

## Build and test

Built using the official rM2 SDK for the 3.27 series:
https://storage.googleapis.com/remarkable-codex-toolchain/3.27.0.97/rm2/remarkable-production-image-5.7.119-rm2-public-x86_64-toolchain.sh

```sh
export SDK_ENV=/absolute/path/to/sdk/environment-setup-cortexa7hf-neon-remarkable-linux-gnueabi
export XOVI_REPO=/absolute/path/to/xovi
./build.sh
```

Output: `bin/ink-diary.so` (ARMv7). Host checks require a C++ compiler and Qt6 Core/Gui development packages:

```sh
./test.sh
```

The checks cover placement boundaries, Messages API JSON handling, incomplete/refusal/error cases, and actual font rendering. They do not test live network requests or physical pen injection.

Upstream provenance and licenses: THIRD_PARTY.md. API documentation: https://docs.claude.com/en/docs/agents-and-tools/tool-use/web-search-tool

Append-placement regression checks can also be run with `node tests/append.cjs`.

## Markdown answers

Answers now render GitHub-style Markdown using the tablet's Qt text engine: six heading levels, bold/italic/strikethrough, ordered and unordered nested lists, task checkboxes, tables with column alignment, inline and fenced/indented code, blockquotes, links, horizontal rules, and Markdown escapes. Paragraphs and headings use Caveat handwriting; tables and code use readable print/monospaced fonts. Code is displayed, not executed, and has no syntax highlighting. Links and checkboxes in the notebook are static ink images; links work in the saved sibling HTML file.

Text answers can grow to 8,000 pixels high and append below existing content. Very large answers or tables are rejected with a request to shorten them. Lines wrap, including long code lines. The model is asked for concise answers and tables with at most five columns. Citations, source notices, and local checked-at times remain visible.

Markdown dialects differ: raw HTML, LaTeX, Mermaid, footnotes, and embedded remote images are not supported. Image markup becomes an alt-text placeholder; it never downloads an image or reads a local file. Function plots still use the existing `plot` command. Formatting changes apply to new answers; existing notebook images stay as they are.

A synthetic example is available in `markdown-preview.png`. The archived version 2.0 is unchanged.

### Current dates and recent facts

Both the notebook-reading stage and web-search stage receive the tablet's current local date, time, and timezone. The reading stage is instructed to treat this as the present even when it is later than the model's training cutoff, and to disregard stale prior notebook answers. A fallback checks the transcribed latest question for English freshness phrases and current-officeholder questions and forces the search route if the model incorrectly labels it ordinary text. Other languages still rely on model routing. This does not guarantee source accuracy; search is instructed to admit when current facts cannot be verified. The tablet clock must remain correct. Prefixing a question with `Search` remains available for explicit web lookup.

### Change only the model

In reManager's terminal, run:

```sh
sh /home/root/.config/ink-diary/configure.sh --model claude-haiku-5-5
```

The `--model` option preserves the saved API key and every other configuration field. It validates the name and existing JSON, then saves atomically with owner-only permissions. No restart is needed; the next question uses the new model. This changes the model for both notebook reading and web search. Use an image-capable Responses model supporting structured outputs and hosted web search, available to your API account. Model names alone do not guarantee account access. GPT-5.4 is a compatible option; costs and latency differ from GPT-4.1 mini. Running the helper without `--model` remains the original full key/model setup flow.

The prompts now explicitly say the answer is rendered as GitHub Markdown. Tables and templates must be emitted outside fences; fences are reserved for programming code or explicitly requested raw Markdown source.

Blank checklists are supported: request `create a 10 item checklist with checkboxes without labels`. Each empty task item gets an invisible layout placeholder so the checkbox retains its own writable row. Standalone checkbox symbols on separate lines are normalized to task items. Existing labels and fenced/indented code examples are preserved.

Text answers also retain their original model-returned Markdown in a private sibling `.png.md` file. This helps distinguish generation problems from rendering problems. The diagnostic message `ink-diary$probe` with argument `checklist` tests ten synthetic blank rows inside the notebook process without inserting anything.

## Structured templates

Checklist and single-table creation requests now use dedicated response kinds in the existing API call. The model supplies a count and optional labels for checklists, or columns and rows for tables. The plugin generates blank rows itself, escapes cell/label text, validates counts and dimensions, and renders the result. Blank templates cannot collapse because of empty model-generated Markdown. Supported limits are 1–50 items/body rows and up to 8 table columns; image size limits still apply. Mixed explanatory answers and web-search answers continue to use Markdown. No additional classifier request is needed.

Examples: `Create a 10 item checklist without labels`; `Create a pros and cons table with 6 blank rows`. With no number specified, the prompts default to ten checklist items or five blank table rows. Handwriting recognition/classification can still make mistakes.

Checkbox outlines, table borders, horizontal dividers, quote bars and plot frames/axes use 3-pixel strokes at source-image resolution; plot gridlines use 2 pixels. Checkbox outlines are painted explicitly for consistency. These changes apply to new answers. Saved `.png.md` files contain model Markdown for ordinary answers and locally generated Markdown for structured templates.

### Writable calendar/table cells

Table templates now carry `cell_lines` (1–10). `1` keeps the compact layout; each additional handwriting line reserves 60 source-image pixels beneath the cell's text. Header rows stay compact. For example: `Create a calendar for November 2026 with each day cell 3 rows tall so I can write in it`. For monthly calendars the model identifies the month, year and requested height; the tablet computes the Gregorian dates and weekday positions using QDate. Sunday is the default first column; ask for Monday-start if preferred. Calendar cells default to three writing lines. HTML `<br>` tags are not used for spacing. This also works for other tables with requested writing space. Large layouts may extend down the notebook page or reach the existing image-height limit.

Calendar requests now default to three handwriting lines per day cell, even when you do not specify a height. An explicit requested height takes precedence. Ordinary tables retain their compact one-line default. This change is newer than the frozen version 3.0 snapshot.
