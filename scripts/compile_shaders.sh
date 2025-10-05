compile ()
{
	if [ "$#" -eq 1 ]; then
		glslc "$1.vert" -o "$1.vert.spv"
		glslc "$1.frag" -o "$1.frag.spv"
	else
		for arg in "$@"; do
			glslc "$arg" -o "$arg.spv"
		done
	fi
}

echo "Compiling Shaders"

cd "assets/shaders"
rm *.spv
compile "quad";
compile "circle";
compile "instance.vert" "text.frag";
compile "common.vert" "path.frag";
compile "grid.vert" "grid_line.frag";
cd ..
cd ..

echo "Done Compiling Shaders"
