# Claude port notes

Port of Ink Diary 4.1 from OpenAI's Responses API to Anthropic's Messages API.
Device-side code (screenshot capture, page guards, Caveat/Markdown rendering,
plots, diagrams, templates, calendars, native image insertion) is unchanged.

## Status: builds and passes host tests; not device-tested

- `./test.sh` passes under Qt6 on Ubuntu (WSL2).
- `bin/ink-diary.so` was cross-compiled with the rM2 3.27 SDK (ARMv7) from this source. It replaces the OpenAI build.
- Not yet run on a tablet. No live Anthropic request has been made. Start with a disposable notebook.

## What changed

| File | Change |
|---|---|
| `src/api.h` | Endpoint `https://api.anthropic.com/v1/messages`; headers `x-api-key`, `anthropic-version: 2023-06-01`. Image-generation mode removed. HTTP errors show only the short error type (e.g. `not_found_error`). Timeout 180 s. |
| `src/response.h` | Parses `content[]` blocks and `stop_reason` (`max_tokens`, `refusal`, `pause_turn` rejected). New `diaryToolJson()` extracts a forced tool call's input. |
| `src/plot.h` | New `diaryAnswerTool()` wraps the existing schema as tool `ink_diary_answer`. `image` removed from the `kind` enum. |
| `src/diary.cpp` | Request is `system` + one user message (image block, then text) with `tool_choice` forcing `ink_diary_answer`. Image-generation branch replaced by an explicit error. Prompt: artwork routing replaced by a "not available" notice; "return JSON" replaced by "call the tool". |
| `src/webanswer.h` | Search uses tool `web_search_20250305` (`max_uses` 3, `user_location.timezone`). Parser keeps only text after the last search result, converts per-fragment `web_search_result_location` citations to `[n]` markers, and rejects answers with no successful search result or with a tool-level error (HTTP 200 + `web_search_tool_result_error`). OpenAI-specific weather-block and private-use-marker cleanup removed. |
| `configure.sh`, `config.example.json` | Anthropic wording; default model `claude-sonnet-5-5`. |
| `README.md`, `INSTALL.md`, `VERSION` | Updated for Anthropic; version `4.1-claude-port.1`. |

## Dropped

- Artwork / comic / manga generation (Claude has no raster image output).
  `src/images.h` is kept only because `tests/visuals.cpp` includes it; it is no
  longer called. The optional `image_model` config field is ignored.

## Things to check first on a real device

1. **Web search access.** An organization admin must enable web search in the
   Anthropic Console, and the chosen model must support `web_search_20250305`.
   If search fails, ordinary questions should still work.
2. **Search is not forced.** OpenAI's `tool_choice: required` has no verified
   equivalent here, so instructions plus the post-check ("Web search did not
   complete") enforce it. If Claude answers from memory you get that error rather
   than a stale answer.
3. **Image downscaling.** The 1174x1752 capture exceeds Claude's long-edge limit
   (about 1568 px), so it is downscaled about 10% server-side. Check recognition
   of small handwriting.
4. **Forced tool use and schema.** The schema is no longer enforced as strict
   structured output; Claude may occasionally omit `template`/`calendar`. Decoding
   only reads those for the kinds that need them.
5. **Model choice.** `claude-haiku-5-5` is cheaper and faster; `claude-sonnet-5-5`
   is the default for handwriting accuracy. Change with
   `sh /home/root/.config/ink-diary/configure.sh --model <name>`.
6. **Branding.** The on-screen button label and icon still say "ChatGPT"
   (`inkDiary.qmd`, `icons/`, `diary_status`). Left untouched to avoid breaking the
   QML/status contract; rename together or not at all.

## Licensing

Upstream is GPL-3.0-only; this port keeps that license. See `THIRD_PARTY.md`.
