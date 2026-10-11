{
  description = "x-reader simulator development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
    esp-idf = {
      url = "git+https://github.com/espressif/esp-idf.git?rev=30aaf64524299d3bde422ca9a2848090d1bc5d0f&submodules=1";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, esp-idf }:
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
          luaExtraComponents = pkgs.fetchFromGitHub {
            owner = "espressif";
            repo = "idf-extra-components";
            rev = "4eccbc4d911623c44f0492b3e1a2421f2b19ab96";
            sha256 = "0arirca4dsdaas8sd24p1354kcds5gxqivfxf1kn1bgs871dgk9z";
          };
          luaUpstream = pkgs.fetchFromGitHub {
            owner = "lua";
            repo = "lua";
            rev = "a5522f06d2679b8f18534fd6a9968f7eb539dc31";
            sha256 = "1fkllmk9hi1z40b1yz7vsj6sz2p3kzb3izpf783md9srgwixhy00";
          };
          luaComponent = pkgs.runCommand "espressif-lua-component" {} ''
            mkdir -p "$out"
            mkdir -p "$out/lua"
            cp "${luaExtraComponents}/lua/CMakeLists.txt" "$out/lua/"
            cp "${luaExtraComponents}/lua/idf_component.yml" "$out/lua/"
            cp -r "${luaExtraComponents}/lua/port" "$out/lua/"
            mkdir -p "$out/lua/lua"
            cp -rL "${luaUpstream}/." "$out/lua/lua/"
          '';
          isLinux = builtins.elem system [ "x86_64-linux" "aarch64-linux" ];
          toolchain = if system == "aarch64-linux" then {
            url = "https://github.com/espressif/crosstool-NG/releases/download/esp-14.2.0_20251107/xtensa-esp-elf-14.2.0_20251107-aarch64-linux-gnu.tar.xz";
            sha256 = "571f1d3d4aa46f75d86f4c0f5c6c492fac6849de8345dae919875fa087c59591";
          } else {
            url = "https://github.com/espressif/crosstool-NG/releases/download/esp-14.2.0_20251107/xtensa-esp-elf-14.2.0_20251107-x86_64-linux-gnu.tar.xz";
            sha256 = "b0065b3b28d2b5d3bf4868f2fda6bc95d6081025583d1c17b286884bead0305d";
          };
          xtensa = pkgs.stdenv.mkDerivation {
            pname = "xtensa-esp-elf";
            version = "14.2.0_20251107";
            src = pkgs.fetchurl toolchain;
            nativeBuildInputs = [ pkgs.autoPatchelfHook ];
            buildInputs = [ pkgs.stdenv.cc.cc.lib pkgs.zlib ];
            sourceRoot = "xtensa-esp-elf";
            installPhase = ''
              mkdir -p "$out"
              cp -r ./* "$out/"
            '';
          };
          pythonPackages = pkgs.python313Packages;
          espIdfMonitor = pythonPackages.buildPythonPackage {
            pname = "esp-idf-monitor";
            version = "1.5.0";
            src = pkgs.fetchurl {
              url = "https://files.pythonhosted.org/packages/source/e/esp-idf-monitor/esp-idf-monitor-1.5.0.tar.gz";
              sha256 = "1r22386zxl5gpp2g3kdrmgqjxfml93b6bn5fzm20vh8aiywzav4h";
            };
            pyproject = true;
            build-system = [ pythonPackages.setuptools ];
            dependencies = with pythonPackages; [
              construct
              pyelftools
              pyserial
              rich
            ];
            dontCheckRuntimeDeps = true;
            doCheck = false;
          };
          espIdfKconfig = pythonPackages.buildPythonPackage {
            pname = "esp-idf-kconfig";
            version = "2.5.4";
            src = pkgs.fetchurl {
              url = "https://files.pythonhosted.org/packages/d5/67/65dea4c47a1c395b33558ddf6e175970a949b0278c9279494ef3c927d78c/esp_idf_kconfig-2.5.4.tar.gz";
              sha256 = "0ijlypfchryi5wiid2qpqwdg7bpri2rkqgbplkljv243591zfhfx";
            };
            pyproject = true;
            build-system = [ pythonPackages.setuptools ];
            doCheck = false;
          };
          idfComponentManager = pythonPackages.buildPythonPackage {
            pname = "idf-component-manager";
            version = "2.2.0";
            src = pkgs.fetchurl {
              url = "https://files.pythonhosted.org/packages/source/i/idf-component-manager/idf_component_manager-2.2.0.tar.gz";
              sha256 = "0x1bwj2bw5wad2nych7xrjapxb2f4h727zhbxjszsc5zy3lb33hj";
            };
            pyproject = true;
            build-system = [ pythonPackages.setuptools ];
            dependencies = with pythonPackages; [
              click
              colorama
              packaging
              pydantic
              pydantic-settings
              requests
              requests-file
              requests-toolbelt
              rich
              pyyaml
              ruamel-yaml
              semantic-version
              tqdm
            ];
            dontCheckRuntimeDeps = true;
            doCheck = false;
          };
          firmwarePython = pkgs.python3.withPackages (_: [
            espIdfMonitor
            espIdfKconfig
            idfComponentManager
            pythonPackages.setuptools
            pythonPackages.psutil
            pythonPackages.tree-sitter
            pythonPackages."esp-idf-size"
            pythonPackages."freertos-gdb"
            pythonPackages.truststore
            pythonPackages.jsonref
            pythonPackages.click
            pythonPackages.colorama
            pythonPackages.construct
            pythonPackages.cryptography
            pythonPackages.pydantic
            pythonPackages.pydantic-settings
            pythonPackages.pyelftools
            pythonPackages.pyparsing
            pythonPackages.pyserial
            pythonPackages.requests-file
            pythonPackages.requests-toolbelt
            pythonPackages.rich
            pythonPackages.ruamel-yaml
            pythonPackages.semantic-version
            pythonPackages.tqdm
          ]);
          idfPyScript = pkgs.writeText "x-reader-idf.py" ''
            import os
            import sys
            from pathlib import Path

            path = Path(os.environ["IDF_PATH"]) / "tools" / "idf.py"
            sys.path.insert(0, str(path.parent))
            source = path.read_text()
            source = source.replace(
                "checks_output = None if SHELL_COMPLETE_RUN else check_environment()",
                "checks_output = []",
            )
            source = source.replace(
                "if name.endswith('_ext'):\n                extensions.append((name, import_module(name)))",
                "if name in ('core_ext', 'serial_ext'):\n                extensions.append((name, import_module(name)))",
            )
            namespace = {"__file__": str(path), "__name__": "__main__"}
            exec(compile(source, str(path), "exec"), namespace)
          '';
          idfPy = pkgs.writeShellScriptBin "idf.py" ''
            exec ${firmwarePython}/bin/python ${idfPyScript} "$@"
          '';
          firmwareTools = pkgs.lib.optionals isLinux [ xtensa ];
        in {
          firmware = pkgs.mkShell {
            packages = with pkgs; [
              bash
              cmake
              esptool
              git
              gnumake
              idfPy
              ninja
              firmwarePython
              unzip
            ] ++ firmwareTools;
            shellHook = ''
              export IDF_PATH="${esp-idf}"
              export ESP_IDF_PATH="$IDF_PATH"
              export IDF_TARGET="''${XREADER_IDF_TARGET:-esp32}"
              export IDF_COMPONENT_MANAGER=0
              export XREADER_LUA_COMPONENT_DIR="${luaComponent}"
              export PATH="$PATH:$IDF_PATH/tools"
              export IDF_PYTHON_CHECK_CONSTRAINTS=0
              echo "x-reader firmware shell"
              echo "ESP-IDF: $IDF_PATH"
              echo "Target: $IDF_TARGET"
              echo "Xtensa: $(command -v xtensa-esp-elf-gcc || true)"
            '';
          };
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
