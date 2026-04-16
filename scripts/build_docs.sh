#! /bin/zsh
echo "This assumes you have already built the project with documentation enabled.
			If not, please run build_debug.sh with BDOCS=ON first."
echo "> BDOCS=ON ./scripts/build_debug.sh"

echo "Building documentation..."
cmake --build build --target docs
echo "Done"
