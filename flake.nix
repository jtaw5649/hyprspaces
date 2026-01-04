{
  description = "hyprspaces devshell";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
    in {
      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.mkShell {
            packages = [
              pkgs.gcc_latest
              pkgs.llvmPackages_21.clang
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
              pkgs.git
              pkgs.gdb
              pkgs.hyprland
              pkgs.pixman
              pkgs.python3
              pkgs.wayland
              pkgs.wayland-protocols
            ];
          };
        }
      );
    };
}
