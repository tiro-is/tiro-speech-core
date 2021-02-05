#!/bin/bash -eu

SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

silence_weight=0.1  # set this to a value less than 1 (e.g. 0) to enable silence weighting.
max_state_duration=40 # This only has an effect if you are doing silence
  # weighting.  This default is probably reasonable.  transition-ids repeated
  # more than this many times in an alignment are treated as silence.

lang_dir=data/lang
ivector_extractor_dir=exp/nnet3/extractor
nnet_dir=exp/chain/tdnn_sp_bi
graph_dir=${nnet_dir}/graph
lang_code=    # BCP-47 lang code for model
model_name=
description=  # short description of the packaged model
mfcc_config=conf/mfcc_hires.conf
fbank_config=
const_arpa=   # ConstArpa model for rescoring

help_message="Packages a Kaldi chain model for Tiro Speech

Usage: $0 <output-dir>
  --lang-dir <str|$lang_dir>
  --ivector-extractor-dir <str|$ivector_extractor_dir>
  --nnet-dir <str|$nnet_dir>
  --graph-dir <str|$graph_dir>
  --lang-code <str|$lang_code>      # BCP-47 language code for model
  --description <str|$description>  # short description of the model
  --mfcc-config <str|$mfcc_config>
  --fbank-config <str|$fbank_config> # Set either fbank-config or mfcc-config
  --const-arpa <str|$const_arpa>    # ConstArpa model for rescoring, empty string == no rescoring
  --model-name <str|$model_name>    # descriptive model name, used as a key
"

run_command="$0 $@"
. $SOURCE_DIR/internal/parse_options.sh

if [ $# -ne 1 ]; then
  echo "$help_message" >&2
  exit 1;
fi
output_dir=$1

mkdir $output_dir || { echo "Output dir exists!" && exit 1; }

echo "$run_command" > $output_dir/run_command

feature_opts=" --feature-type mfcc --mfcc-config \"$mfcc_config\" "
if [ ! -z "$fbank_config" ]; then
  feature_opts=" --feature-type fbank --fbank-config \"$fbank_config\" "
fi

$SOURCE_DIR/internal/prepare_online_decoding.sh \
  $feature_opts \
  "$lang_dir" \
  "$ivector_extractor_dir" \
  "$nnet_dir" \
  "$output_dir"

frame_subsampling_factor=$(cat "$output_dir/frame_subsampling_factor")
if [ ! -z "$frame_subsampling_factor" ]; then
  echo "--frame-subsampling-factor=$frame_subsampling_factor" >> "$output_dir/main.conf"
  echo "--acoustic-scale=1.0" >> "$output_dir/main.conf"
fi

cp -r "$graph_dir" "$output_dir/graph"

echo "--align-lexicon-int=graph/phones/align_lexicon.int" >> "$output_dir/main.conf"

if [ "$silence_weight" != "1.0" ]; then
  silphones=$(cat "$output_dir/graph/phones/silence.csl") || exit 1
  [ "x$max_state_duration" != x ] && \
    echo "--ivector-silence-weighting.max-state-duration=$max_state_duration" >> "$output_dir/main.conf"
  echo "--ivector-silence-weighting.silence_phones=$silphones" >> "$output_dir/main.conf"
  echo "--ivector-silence-weighting.silence-weight=$silence_weight" >> "$output_dir/main.conf"
fi

echo "--word-syms-rxfilename=$output_dir/graph/words.txt
--fst-rxfilename=graph/HCLG.fst
--nnet3-rxfilename=final.mdl" >> "$output_dir/main.conf"

echo "--language-code=$lang_code" >> "$output_dir/main.conf"
echo "Model '$model_name'" >> "$output_dir/README"
echo "$description" >> "$output_dir/README"

if [ "x$const_arpa" != x ]; then
  if [ ! -f "$const_arpa" ]; then
    echo "ConstArpa file '$const_arpa' does not exist!" >&2; exit 1
  fi
  cp "$const_arpa" "$output_dir/G.carpa"
  echo "--const-arpa-rxfilename=G.carpa" >> "$output_dir/main.conf"
fi

# DecoderConfig: These are currently configured at model level. This will
# probably change in the future.
{
  echo "--beam=15"
  echo "--max-active=7000"
  echo "--lattice-beam=6"
} >> "$output_dir/main.conf"

echo "Making paths relative to $output_dir" >&2
find $output_dir -name "*.conf" -exec sed -i "s:=\(.\+\)/conf:=conf:" {} \;
find $output_dir -name "*.conf" -exec sed -i "s:$output_dir/::" {} \;

find $output_dir -name "*.conf" -exec sed -i "s:--\(.+\):--model\.:" {} \;
