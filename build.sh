#/bin/sh
if test -e "build";then
    echo "build dir already exists; rm -rf build and re-run"
    rm -rf build_nvmf
fi

if [ ! -d "build" ] ; then
    mkdir build
fi


if [ $# -lt 1 ]; then
  echo "use default march native, other option(native/znver1/ivybridge/haswell/broadwell/skylake/cascadelake)"
  march="native"
else
  echo "use march $1"
  march=$1
fi


cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug # -DCOVERALLS=ON -DBUILD_ARCH=${march}
make -j4
make bolt_test
