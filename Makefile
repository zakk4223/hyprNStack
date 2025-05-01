.PHONY: check_libs install uninstall clean

PACKAGES = `pkg-config --cflags pixman-1 libdrm hyprland wayland-server libinput aquamarine xkbcommon`
COMPILE_CMD = $(CXX) -DWLR_USE_UNSTABLE -shared -fPIC --no-gnu-unique main.cpp nstackLayout.cpp -o nstackLayoutPlugin.so -g $(PACKAGES) -std=c++2b

build: check_libs
	$(COMPILE_CMD)

bypass_checks:
	@echo "Bypassing library checks..."
	$(COMPILE_CMD)

check_libs:
	@echo "Checking for required libraries..."
	@pkg-config --exists pixman-1 || (echo "Error: pixman-1 not found" && exit 1)
	@pkg-config --exists libdrm || (echo "Error: libdrm not found" && exit 1)
	@pkg-config --exists hyprland || (echo "Error: hyprland not found" && exit 1)
	@pkg-config --exists wayland-server || (echo "Error: wayland-server not found" && exit 1)
	@pkg-config --exists libinput || (echo "Error: libinput not found" && exit 1)
	@pkg-config --exists aquamarine || (echo "Error: aquamarine not found" && exit 1)
	@pkg-config --exists xkbcommon || (echo "Error: xkbcommon not found" && exit 1)
	@echo "All required libraries found."

install: build
	@echo "This will install the nstackLayoutPlugin.so file to the ~/.config/hypr/plugins directory."
	@echo "Are you sure you want to continue? (y/n)"
	@read confirm
	@if [ "$$confirm" != "y" ]; then
		@echo "Installation cancelled."
		@exit 1
	fi
	mkdir -p ~/.config/hypr/plugins
	cp ./nstackLayoutPlugin.so ~/.config/hypr/plugins/
	@echo "Installation complete. You should probably restart hyprland now."

uninstall:
	@echo "This will uninstall the nstackLayoutPlugin.so file from the ~/.config/hypr/plugins directory."
	@echo "Are you sure you want to continue? (y/n)"
	@read confirm
	@if [ "$$confirm" != "y" ]; then
		@echo "Uninstallation cancelled."
		@exit 1
	fi
	rm ~/.config/hypr/plugins/nstackLayoutPlugin.so
	@echo "Uninstallation complete. You should probably restart hyprland now. (nstackLayoutPlugin.so removed)"
	@echo "Remember to set your layout back to either dwindle or master."

clean:
	rm ./nstackLayoutPlugin.so
