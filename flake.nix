{
  description = "Deterministic 2D crop-world simulation library";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      cropsim-viewer = pkgs.stdenv.mkDerivation {
        pname = "cropsim-viewer";
        version = "0.1.0";
        src = pkgs.lib.fileset.toSource {
          root = ./.;
          fileset = pkgs.lib.fileset.unions [
            ./CMakeLists.txt
            ./include
            ./src
          ];
        };

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
    in {
      packages.${system}.cropsim-viewer = cropsim-viewer;

      apps.${system}.cropsim-viewer = {
        type = "app";
        program = "${cropsim-viewer}/bin/cropsim_viewer";
        meta.description = "Interactive GPU viewer for crop simulation worlds";
      };

      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          cmake
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
