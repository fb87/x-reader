{
  description = "x-reader simulator development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forEachSystem = nixpkgs.lib.genAttrs systems;
    in {
      packages = forEachSystem (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in {
          simulator = pkgs.stdenv.mkDerivation {
            pname = "x-reader-simulator";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.pkg-config ];
            buildInputs = [ pkgs.SDL2 pkgs.lua5_4 pkgs.zlib ];
            cmakeFlags = [ "-G" "Ninja" ];
          };
          default = self.packages.${system}.simulator;
        });

      apps = forEachSystem (system: {
        simulator = {
          type = "app";
          program = "${self.packages.${system}.simulator}/bin/x-reader-sim";
        };
        default = self.apps.${system}.simulator;
      });

      devShells = forEachSystem (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in {
          simulator = pkgs.mkShell {
            packages = [
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
              pkgs.clang-tools
              pkgs.findutils
              pkgs.unzip
              pkgs.stylua
              (pkgs.python3.withPackages (pythonPackages: [ pythonPackages.pillow ]))
            ];
            buildInputs = [ pkgs.SDL2 pkgs.lua5_4 pkgs.zlib ];
          };
          quality = pkgs.mkShell {
            packages = [ pkgs.clang-tools pkgs.findutils pkgs.stylua ];
          };
          default = self.devShells.${system}.simulator;
        });

      formatter = forEachSystem (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in pkgs.writeShellApplication {
          name = "format-x-reader";
          runtimeInputs = [ pkgs.clang-tools pkgs.findutils pkgs.stylua ];
          text = ''
            exec "$PWD/tools/format" "$@"
          '';
        });

      checks = forEachSystem (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in {
          simulator = self.packages.${system}.simulator;
          format = pkgs.runCommand "x-reader-format-check" {
            src = self;
            nativeBuildInputs = [ pkgs.clang-tools pkgs.findutils pkgs.stylua ];
          } ''
            cd "$src"
            bash tools/format --check
            touch "$out"
          '';
        });
    };
}
