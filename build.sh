#!/bin/bash
#!/bin/bash

clone() {
  if [ ! -d $1 ]; then git clone $2 $1; fi
}
download() {
  if [ ! -d ext ]; then mkdir -p ext; fi
  clone ext/glfw  https://github.com/glfw/glfw.git
  clone ext/imgui https://github.com/ocornut/imgui
  clone ext/Vulkan-Headers https://github.com/KhronosGroup/Vulkan-Headers.git
}
# cmake .. -DCMAKE_INSTALL_PREFIX=./binary \
build_glfw() {
  pwd=`pwd`
  cd ext/glfw
  mkdir build-mingw
  cd build-mingw
  # cmake .. -DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
  cmake .. \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=./binary \
    -DBUILD_SHARED_LIBS=ON
  make -j4
  # sudo make install
  cd $pwd
}

send() {
  password="$1"
  file="$2"
  ssh_target="$3"
  remote_path="$4"
  sshpass -p "${password}" scp ${file} ${ssh_target}:${remote_path} && echo "Success moving ${file} -> ${remote_path}"
}

build_mouse() {
  target=$1
  file="bin/${target}.exe"
  mainfunction=$1
  include="-Iext/glfw/build-mingw/binary/include"
  include+=" -Iext/Vulkan-Headers/include"
  lib="-Lbin/"

  add="${include} ${lib} ${bin}"
  runtime="-Wl,-rpath,./"
  x86_64-w64-mingw32-gcc -D${mainfunction}=main updater.c timeout.c input.c demo.c do_glfw.c do_imgui.c do_vulkan.c firefox_dom.c firefox_dom_rdp.c firefox_dom_cdp.c -o ${file} ${add} -lgdi32 -lglfw3 -lws2_32 -ladvapi32 -l:vulkan-1.lib ${runtime} && echo "Success ${file}"

  password="{password}"
  ssh_target="user@127.0.0.1"
  remote_path="C://Users/.../Desktop/exe"

  if [ -f build.hidden ]; then
    source build.hidden
  fi

  send $password $file $ssh_target $remote_path

  glfw_dll="bin/glfw3.dll"
  send $password $glfw_dll $ssh_target $remote_path
}
build_markdown() {
  file="bin/markdown"
  gcc -Dmarkdown=main markdown.c -o ${file} && echo "Success ${file}"
}
