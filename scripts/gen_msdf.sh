#!/bin/sh


# gen <dir> <file-name> <size>
gen(){
	_CUR_DIR=$(realpath .)
	if [ ! -e "./$1/$2.ttf" ]; then
		echo "File does not exist at $(realpath .)/$1/$2.ttf"
		return 1
	fi

	echo "[MsdfGenerator] Generating at $1 for file $2 at size $3"
	cd "./$1"

	_DIR="msdf-$2-$3"
	mkdir "./$_DIR" -p
	rm "./$_DIR/*" -rf

	msdf-atlas-gen \
		-font "$2.ttf" \
		-type mtsdf \
		-size $3 \
		-imageout "./$_DIR/$2.png" \
		-json "./$_DIR/$2.json" \
		-pxrange 8 \

		echo "[MsdfGenerator] Generated at $(realpath .)/$_DIR"

	cd $_CUR_DIR
}

TTF_FILE_DIR="assets/fonts/Roboto"
TTF_FILE_NAME="Roboto-Regular" # without extension

gen $TTF_FILE_DIR $TTF_FILE_NAME 32



