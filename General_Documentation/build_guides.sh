#!/usr/bin/env bash
# Build user guide + fast guide (EN/ES).
# Fast guide is delivered only as fast_guide_eng.pdf / fast_guide_esp.pdf
# (Android Codes asset names) — not as a second telecon_fast_guide.pdf copy.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"

build_lang() {
  local lang="$1"
  local fast_alias="$2"
  local src="$ROOT/$lang/src"
  local build="$ROOT/$lang/build"
  mkdir -p "$build"
  (cd "$src" && pdflatex -interaction=nonstopmode -output-directory="$build" telecon_user_guide.tex >/dev/null)
  (cd "$src" && pdflatex -interaction=nonstopmode -output-directory="$build" telecon_user_guide.tex >/dev/null)
  (cd "$src" && pdflatex -interaction=nonstopmode -output-directory="$build" telecon_fast_guide.tex >/dev/null)
  (cd "$src" && pdflatex -interaction=nonstopmode -output-directory="$build" telecon_fast_guide.tex >/dev/null)
  cp "$build/telecon_fast_guide.pdf" "$build/$fast_alias"
  rm -f "$build/telecon_fast_guide.pdf"
  echo "OK $lang: telecon_user_guide.pdf + $fast_alias"
}

build_lang en fast_guide_eng.pdf
build_lang es fast_guide_esp.pdf
