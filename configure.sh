#!/bin/sh
set -eu
umask 077
if [ "${1:-}" = --model ]; then
    [ "$#" = 2 ] || { echo 'Usage: configure.sh --model MODEL'; exit 1; }
    exec /home/root/.config/ink-diary/config-model "$2"
fi
[ "$#" = 0 ] || { echo 'Usage: configure.sh [--model MODEL]'; exit 1; }
base=/home/root/.config/ink-diary
mkdir -p "$base"
printf 'Anthropic API key (hidden): ' > /dev/tty
stty -echo < /dev/tty
trap 'stty echo < /dev/tty' EXIT HUP INT TERM
IFS= read -r diary_key < /dev/tty
stty echo < /dev/tty
printf '\n' > /dev/tty
case "$diary_key" in ''|*[!A-Za-z0-9_-]*) echo 'Invalid key format'; exit 1;; esac
printf 'Model [claude-sonnet-5-5]: ' > /dev/tty
IFS= read -r diary_model < /dev/tty
diary_model=${diary_model:-claude-sonnet-5-5}
case "$diary_model" in *[!A-Za-z0-9_.-]*) echo 'Invalid model name'; exit 1;; esac
printf '{"api_key":"%s","model":"%s"}\n' "$diary_key" "$diary_model" > "$base/config.json.new"
mv "$base/config.json.new" "$base/config.json"
unset diary_key
printf 'Configuration saved. No UI restart needed.\n'
