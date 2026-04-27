{
  description = "N-stack layout plugin for Hyprland";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    hyprland = {
      url = "git+https://github.com/hyprwm/Hyprland?submodules=1&ref=refs/pull/13817/head";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
    nixpkgs,
    hyprland,
  }:
    let
      systems = [
        "aarch64-linux"
        "x86_64-linux"
      ];

      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          hyprlandPkg = hyprland.packages.${system}.hyprland;
          hyprNStack = pkgs.stdenv.mkDerivation {
            pname = "hyprNStack";
            version = "0-unstable-${self.shortRev or "dirty"}";

            src = self;

            strictDeps = true;
            nativeBuildInputs = [
              pkgs.pkg-config
            ];
            buildInputs = [
              hyprlandPkg
            ] ++ hyprlandPkg.buildInputs;

            dontStrip = true;

            installPhase = ''
              runHook preInstall

              install -Dm755 nstackLayoutPlugin.so \
                "$out/lib/libhyprNStack.so"

              runHook postInstall
            '';

            meta = {
              description = "N-stack layout plugin for Hyprland";
              homepage = "https://github.com/zakk4223/hyprNStack";
              license = pkgs.lib.licenses.bsd3;
              platforms = hyprlandPkg.meta.platforms;
            };
          };
        in {
          default = hyprNStack;
          hyprNStack = hyprNStack;
        });

      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          hyprlandPkg = hyprland.packages.${system}.hyprland;
        in {
          default = pkgs.mkShell {
            nativeBuildInputs = [
              pkgs.pkg-config
            ];
            buildInputs = [
              hyprlandPkg
            ] ++ hyprlandPkg.buildInputs;
          };
        });
    };
}
