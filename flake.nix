{
  description = "XReader ESP-IDF development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    esp-idf = {
      url = "github:espressif/esp-idf/v5.5.2";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, esp-idf }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      for_each_system = f:
        nixpkgs.lib.genAttrs systems (system: f (import nixpkgs {
          inherit system;
        }));
    in {
      devShells = for_each_system (pkgs: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            bash
            clang
            clang-tools
            cmake
            esptool
            git
            gnumake
            ninja
            python3
            unzip
          ];

          ESP_IDF_PATH = "${esp-idf}";

          shellHook = ''
            export PATH="$ESP_IDF_PATH/tools:$PATH"
            export IDF_TARGET="esp32"
            echo "xreader development shell"
            echo "ESP-IDF source: $ESP_IDF_PATH"
            echo "Target: $IDF_TARGET"
            echo "Note: install or provide the ESP-IDF Python environment and Xtensa toolchain before building."
          '';
        };
      });
    };
}
