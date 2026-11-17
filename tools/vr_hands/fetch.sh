#!/bin/sh
# fetch.sh <folder>: downloads the gloves of Valve's SteamVR Unity plugin
# (BSD 3-clause) into <folder>: the two skinned models, their images and the
# three hand poses that bake.py uses.
set -e
out=$1
[ -n "$out" ] || { echo "usage: fetch.sh <folder>" >&2; exit 1; }
base=https://raw.githubusercontent.com/ValveSoftware/steamvr_unity_plugin/master/Assets/SteamVR
mkdir -p "$out"
for file in \
	Models/vr_glove_left_model_slim.fbx \
	Models/vr_glove_right_model_slim.fbx \
	Models/Materials/vr_glove_color.jpg \
	Models/Materials/vr_glove_normal.png \
	InteractionSystem/Poses/fallback_relaxed.asset \
	InteractionSystem/Poses/fallback_point.asset \
	InteractionSystem/Poses/fallback_fist.asset; do
	name=$(basename "$file")
	[ -s "$out/$name" ] || curl -fsSL -o "$out/$name" "$base/$file"
done
