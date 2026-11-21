#!/bin/sh
# build.sh <blender> <aeron_gltf_cook> <work folder>: makes the six glove
# models that the game ships in resources/assets/models/. It downloads the
# sources into <work folder>/inputs, bakes them with Blender and cooks them.
set -e
here=$(cd "$(dirname "$0")" && pwd)
blender=$1
cook=$2
work=$3
[ -n "$work" ] || { echo "usage: build.sh <blender> <aeron_gltf_cook> <work folder>" >&2; exit 1; }
"$here/fetch.sh" "$work/inputs"
"$blender" -b --python "$here/bake.py" -- "$work/inputs" "$work/baked" | grep -E "^(BAKED|TIP)|Error|Traceback"
out="$here/../../resources/assets/models"
mkdir -p "$out"
for side in left right; do
	for pose in neutral point grasp; do
		"$cook" "$work/baked/vr_hand_${side}_${pose}.gltf" --out "$out/vr_hand_${side}_${pose}.glb"
	done
done
