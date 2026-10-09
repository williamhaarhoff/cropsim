{
  description = "Deterministic 2D crop-world simulation library";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      projectSource = pkgs.lib.fileset.toSource {
        root = ./.;
        fileset = pkgs.lib.fileset.unions [
          ./CMakeLists.txt
          ./include
          ./src
        ];
      };
      cropsim-viewer = pkgs.stdenv.mkDerivation {
        pname = "cropsim-viewer";
        version = "0.1.0";
        src = projectSource;

        nativeBuildInputs = with pkgs; [
          cmake
          ninja
          pkg-config
          shaderc
        ];
        buildInputs = with pkgs; [
          sdl3
          vulkan-headers
          vulkan-loader
          boost
          yaml-cpp
        ];

        cmakeFlags = [
          "-DCROPSIM_BUILD_TESTS=OFF"
          "-DCROPSIM_BUILD_VIEWER=ON"
        ];

        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp cropsim_viewer crop.vert.spv crop.frag.spv $out/bin/
          runHook postInstall
        '';
      };
      cpp-test = pkgs.stdenv.mkDerivation {
        pname = "cropsim-cpp-test";
        version = "0.1.0";
        src = pkgs.lib.fileset.toSource {
          root = ./.;
          fileset = pkgs.lib.fileset.unions [
            ./CMakeLists.txt
            ./include
            ./src
            ./tests
          ];
        };

        nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
        buildInputs = with pkgs; [ boost doctest yaml-cpp ];
        cmakeFlags = [
          "-DCROPSIM_BUILD_TESTS=ON"
          "-DCROPSIM_BUILD_BENCHMARKS=ON"
          "-DCROPSIM_BUILD_VIEWER=OFF"
        ];

        doCheck = true;
        checkPhase = ''
          runHook preCheck
          ctest --output-on-failure
          runHook postCheck
        '';

        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          printf '#!/bin/sh\necho "cropsim C++ tests passed"\n' > $out/bin/cpp-test
          chmod +x $out/bin/cpp-test
          runHook postInstall
        '';
      };
    in {
      packages.${system} = {
        inherit cropsim-viewer cpp-test;
      };

      checks.${system}.cpp-test = cpp-test;

      apps.${system} = {
        cropsim-viewer = {
          type = "app";
          program = "${cropsim-viewer}/bin/cropsim_viewer";
          meta.description = "Interactive GPU viewer for crop simulation worlds";
        };
        cpp-test = {
          type = "app";
          program = "${cpp-test}/bin/cpp-test";
          meta.description = "Build and run the cropsim C++ tests and benchmark";
        };
      };

      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          cmake
          boost
          doctest
          shaderc
          sdl3
          vulkan-headers
          vulkan-loader
          ninja
          pkg-config
          yaml-cpp
        ];
      };
    };
}
