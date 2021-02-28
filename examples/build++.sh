# This command requires glfw with include files and libraries
# NOTE: use -lgfw3 on macOS
# This build script should be improved, use cmake perhaps
export DVZ_EXAMPLE_FILE=$1
g++ $DVZ_EXAMPLE_FILE -I../include/ -I../external/cglm/include -I../build/_deps/glfw-src/include \
    -I../external/imgui/ -I../external/ -I$(qmake -query QT_INSTALL_HEADERS) \
    -L../build/ -L../build/_deps/glfw-build/src -lvulkan -lm -lglfw $(pkg-config --libs Qt5Gui) \
    -ldatoviz -fPIC -o datoviz_example

# NOTE: libdatoviz must be in the linker path before running the example (dynamic linking)
# LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../build ./datoviz_example
