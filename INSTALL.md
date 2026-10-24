# Ink Diary 4.1 — installation and configuration

## Requirements

- **reMarkable 2 running exactly OS 3.27.3.0.** This build is not compatible with other tablets or firmware versions. The installer and plugin enforce this requirement; do not bypass it.
- Working SSH access and Vellum/XOVI on the tablet. Install reManager from its [official releases](https://github.com/rmitchellscott/reManager/releases), connect your tablet, and set up Vellum. Install `xovi` and `qt-resource-rebuilder` through its package manager before proceeding. See the [official reManager guide](https://github.com/rmitchellscott/reManager/blob/main/README.md).
- Tablet internet access for dependency installation and API requests.
- Your own Anthropic API key (Console) with billing for the model you use, and web search enabled for your organization in Console if you want web lookups. A Claude.ai subscription alone does not configure this plugin. This bundle contains no working key.

This is a manual installation bundle, not a published reManager catalog package. **Claude port:** `bin/ink-diary.so` was built from this source with the rM2 3.27 SDK but is not yet device-tested. Rebuild with `build.sh` if you change the source. Source and license files are included. Keep a backup of your notebooks before installing third-party extensions. Remove this plugin before upgrading firmware until the new version has been tested.

## 1. Get the files and transfer them

Clone this repository or download and extract its source ZIP on your computer. The repository includes prebuilt ARM binaries in `bin/`, so no compilation is required to install this version.

Use reManager's file browser to create `/home/root/ink-diary-v4.1` on your tablet and upload the repository contents into it, including `bin`, `fonts`, `icons`, the scripts and `SHA256SUMS`. Do not upload `.git` or any personal configuration. Transfer these as files, not notebook documents.

Alternatively, from a terminal in your local repository, replace TABLET_IP with your tablet's reachable address:

```sh
git archive --format=tar HEAD | ssh root@TABLET_IP 'mkdir -p /home/root/ink-diary-v4.1 && tar -xf - -C /home/root/ink-diary-v4.1'
```

This transfers only committed files. Use a new/empty destination folder to avoid mixing versions.

If using the separately distributed `ink-diary-v4.1.tar.gz` installation bundle instead, upload it and its `.sha256` file to `/home/root`, verify with `sha256sum -c ink-diary-v4.1.sha256`, then extract with `tar -xzf ink-diary-v4.1.tar.gz`. The repository does not duplicate that archive; it contains the unpacked implementation.

## 2. Verify and install

Finish writing and return to the tablet's library. Open reManager's terminal (these commands run on the tablet as root):

```sh
cd /home/root/ink-diary-v4.1
sha256sum -c SHA256SUMS
```

All entries must say `OK`. These checksums describe this checked-in release; rebuilding or editing files requires regenerating them for a new release. Then install:

```sh
sh install.sh
```

The installer checks hardware/firmware, installs screenshot/message-broker dependencies through Vellum, and copies the plugin, notebook patch, fonts, icon and configuration helper. It preserves an existing API configuration. If installation reports an error, resolve it before restarting the notebook app.

## 3. Configure your own API key

For a new installation:

```sh
sh /home/root/.config/ink-diary/configure.sh
```

Enter your API key at the hidden prompt, then select the text/vision model. Pressing Enter at the model prompt uses `claude-sonnet-5-5`. The model must support image input and tool use; web lookup additionally needs the web search tool. Availability depends on your account.

The key is stored only on your tablet in `/home/root/.config/ink-diary/config.json`, with owner-only permissions. Do not share that file or post it in troubleshooting screenshots. No key is sent to the bundle's author. Requests go directly from your tablet to Anthropic.

**Upgrading an existing installation:** skip this full configuration step to retain your key and preferences. Running the full configuration command replaces the settings file. To change only the text model while preserving your key and other settings, replace MODEL_NAME below with your chosen model:

```sh
sh /home/root/.config/ink-diary/configure.sh --model MODEL_NAME
```

Settings are read on each tap; configuration changes do not require a restart.

Optional settings can be added to your existing JSON file using a private editor, retaining its api_key and model fields:

- `timezone`: IANA timezone name, such as `Europe/London`. Default: `America/New_York`.
- `demo_mode`: `true` for an offline fixed handwriting sample, `false` for normal API use. Demo mode does not test diagrams or calendars.

After manual editing, keep the JSON valid and restrict permissions:

```sh
chmod 600 /home/root/.config/ink-diary/config.json
```

## 4. Start Ink Diary

Return to the library, then run:

```sh
setsid bash /home/root/xovi/start >/tmp/ink-diary-start.log 2>&1 </dev/null &
```

This restarts the notebook app. A full tablet reboot is unnecessary. Open a notebook in portrait orientation at normal zoom; the ChatGPT icon appears at the bottom-right. If extensions are not automatically loaded after a future reboot, run the same start command from the library.

## 5. Use it

Write in the visible page area, away from the left toolbar and bottom-right icon. The top edge is included. Tap the icon and keep the page still while it works. Tap the stop square to cancel. Hold the icon to see status.

Try:

- `Why does the Moon shine?`
- `Create an October 2026 calendar` — dates are computed locally; October starts Thursday. Cells default to three writing lines. Ask for Monday-start if preferred.
- `Create a flow diagram of making tea` — compact wrapping flowchart.
- `Create a 10 item checklist without labels`
- `plot y=x^2`
- `Search the weather in Lowell MA right now`
- `Generate a three-panel black-and-white comic about a robot growing a flower`
- `Create a Venn diagram comparing cats and dogs`

Answers append below existing content and may extend the page; scroll down to see them. They are image objects, not editable pen strokes. Mermaid supports flowcharts and sequence diagrams, not every Mermaid type. Venn diagrams support two or three sets. Generated artwork is converted to grayscale; it can take several minutes and incurs image API charges. Cancellation cannot guarantee cancellation of server-side billing.

## Troubleshooting

- **Unsupported OS/hardware:** this release only supports reMarkable 2 on 3.27.3.0. Do not alter the compatibility checks.
- **No icon:** check installation completed and restart through `/home/root/xovi/start`. Open a notebook, not just the library.
- **Extension unavailable:** verify XOVI and its dependencies loaded. A reboot alone may not start XOVI in every installation.
- **HTTP/API error:** check Wi-Fi, API billing, key and selected model/tool access. Never share your configuration file to diagnose this.
- **Page changed:** keep the page still until the response finishes, then retry.
- **Response below the screen:** scroll down. Existing answers are not automatically replaced or corrected.
- **Diagram error:** request a simpler flowchart/sequence diagram with short labels. Limits are described in README.md.

## Remove or restore

```sh
cd /home/root/ink-diary-v4.1
sh uninstall.sh
```

Restart the notebook app using the start command above. Uninstallation removes the plugin and QML patch; it keeps configuration and generated answer files. To remove your stored credential as well, delete only `/home/root/.config/ink-diary/config.json` after uninstalling. Do not share your answer directory: it can contain notebook-derived text and images.

To restore this version later, verify the archive, run its installer, and restart from the library. Skip full configuration to preserve existing settings.

## Sharing and privacy

Share this repository or a release archive made from its committed files, together with this guide. Do not create a new archive from your tablet's installed configuration or answer directories. The included `config.example.json` contains only a placeholder. API credentials, notebook content, diagnostic screenshots and generated artwork are excluded from this release.

The visible notebook crop is sent to Anthropic when you tap the button. Rendering diagrams is local after transcription. See README.md for network behavior and limits, THIRD_PARTY.md for attribution, and LICENSE for redistribution terms.
