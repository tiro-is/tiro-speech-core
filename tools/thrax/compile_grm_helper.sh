#!/usr/bin/env bash
PROJDIR="$0.runfiles/com_gitlab_tiro_is_tiro_speech_core"
INPUT_GRAMMAR=$1
FAR="$PWD/$2"

cd "$PROJDIR"
exec external/org_opengrm_thrax/compiler --input_grammar="$INPUT_GRAMMAR" --output_far="$FAR"
