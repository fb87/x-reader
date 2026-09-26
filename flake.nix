{
  description = "XReader ESP-IDF development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    esp-idf = {
      url = "git+https://github.com/espressif/esp-idf.git?rev=30aaf64524299d3bde422ca9a2848090d1bc5d0f&submodules=1";
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
      devShells = for_each_system (pkgs:
        let
          is_linux = builtins.elem pkgs.system [ "x86_64-linux" "aarch64-linux" ];
          xtensa_archive = if pkgs.system == "aarch64-linux" then {
            url = "https://github.com/espressif/crosstool-NG/releases/download/esp-14.2.0_20251107/xtensa-esp-elf-14.2.0_20251107-aarch64-linux-gnu.tar.xz";
            sha256 = "571f1d3d4aa46f75d86f4c0f5c6c492fac6849de8345dae919875fa087c59591";
          } else {
            url = "https://github.com/espressif/crosstool-NG/releases/download/esp-14.2.0_20251107/xtensa-esp-elf-14.2.0_20251107-x86_64-linux-gnu.tar.xz";
            sha256 = "b0065b3b28d2b5d3bf4868f2fda6bc95d6081025583d1c17b286884bead0305d";
          };
          ulp_archive = if pkgs.system == "aarch64-linux" then {
            url = "https://github.com/espressif/binutils-gdb/releases/download/esp32ulp-elf-2.38_20240113/esp32ulp-elf-2.38_20240113-linux-arm64.tar.gz";
            sha256 = "ecce0788ce1000e5c669c5adaf2fd5bf7f9bf96dcdbd3555d1d9ce4dcb311038";
          } else {
            url = "https://github.com/espressif/binutils-gdb/releases/download/esp32ulp-elf-2.38_20240113/esp32ulp-elf-2.38_20240113-linux-amd64.tar.gz";
            sha256 = "d13a808365b78465fa6591636dfbbb9604d9d15a397c3d9cd22626d54828ac2c";
          };

          xtensa_esp_elf = pkgs.stdenv.mkDerivation {
            pname = "xtensa-esp-elf";
            version = "14.2.0_20251107";
            src = pkgs.fetchurl {
              inherit (xtensa_archive) url sha256;
            };
            nativeBuildInputs = [ pkgs.autoPatchelfHook ];
            buildInputs = [ pkgs.stdenv.cc.cc.lib pkgs.zlib ];
            sourceRoot = "xtensa-esp-elf";
            installPhase = ''
              mkdir -p "$out"
              cp -r ./* "$out/"
            '';
          };

          esp32ulp_elf = pkgs.stdenv.mkDerivation {
            pname = "esp32ulp-elf";
            version = "2.38_20240113";
            src = pkgs.fetchurl {
              inherit (ulp_archive) url sha256;
            };
            sourceRoot = "esp32ulp-elf";
            installPhase = ''
              mkdir -p "$out"
              cp -r ./* "$out/"
            '';
          };

          esp_rom_elfs = pkgs.stdenv.mkDerivation {
            pname = "esp-rom-elfs";
            version = "20241011";
            src = pkgs.fetchurl {
              url = "https://github.com/espressif/esp-rom-elfs/releases/download/20241011/esp-rom-elfs-20241011.tar.gz";
              sha256 = "921f000164a421c7628fbfee55b173384aafaa51883adc65cd27bf9b0af9e9a9";
            };
            sourceRoot = ".";
            installPhase = ''
              mkdir -p "$out"
              cp -r ./* "$out/"
            '';
          };

          toolchain_packages = pkgs.lib.optionals is_linux [
            xtensa_esp_elf
            esp32ulp_elf
            esp_rom_elfs
          ];
        in {
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
              python3Packages.pip
              unzip
            ] ++ toolchain_packages;

            ESP_IDF_PATH = "${esp-idf}";
            ESP_ROM_ELF_DIR = if is_linux then "${esp_rom_elfs}" else "";

            shellHook = ''
              export PATH="$ESP_IDF_PATH/tools:$PATH"
              export IDF_TARGET="esp32"
              export IDF_PYTHON_CHECK_CONSTRAINTS=0
              export IDF_PYTHON_ENV_PATH="''${XREADER_IDF_PYTHON_ENV_PATH:-$PWD/.nix/idf-python}"

              if [ ! -x "$IDF_PYTHON_ENV_PATH/bin/python" ]; then
                python -m venv "$IDF_PYTHON_ENV_PATH"
              fi

              if [ ! -f "$IDF_PYTHON_ENV_PATH/.xreader-core-installed" ]; then
                "$IDF_PYTHON_ENV_PATH/bin/python" -m pip install \
                  --disable-pip-version-check \
                  --quiet \
                  --requirement "$ESP_IDF_PATH/tools/requirements/requirements.core.txt"
                touch "$IDF_PYTHON_ENV_PATH/.xreader-core-installed"
              fi

              export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"
              echo "xreader development shell"
              echo "ESP-IDF source: $ESP_IDF_PATH"
              echo "Target: $IDF_TARGET"
              echo "Xtensa toolchain: $(command -v xtensa-esp-elf-gcc || true)"
            '';
          };
        });
    };
}
