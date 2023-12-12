all:
	$(CXX) -DWLR_USE_UNSTABLE -shared -fPIC --no-gnu-unique main.cpp nstackLayout.cpp -o nstackLayoutPlugin.so -g `pkg-config --cflags pixman-1 libdrm hyprland` -std=c++2b
clean:
	rm ./nstackLayoutPlugin.so
