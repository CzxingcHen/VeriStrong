{
  outputs = { nixpkgs, ... }:
  let
    pkgs = nixpkgs.legacyPackages.x86_64-linux;
    mkShell = pkgs.mkShell.override {
      # stdenv = pkgs.llvmPackages_15.libcxxStdenv;
      stdenv = pkgs.gcc12Stdenv;
    };
    gdb-wrapper = pkgs.writeShellApplication {
      name = "gdb";
      text = ''
        exec gdb -ex 'add-auto-load-safe-path ${pkgs.gcc12Stdenv.cc.cc.lib}' "$@"
      '';
    };
  in
  {
    devShell.x86_64-linux = mkShell {
      hardeningDisable = [ "fortify" ];

      packages = with pkgs; [
        meson
        ninja
        clang-tools
        cmake
        boost.dev
        argparse
        linuxPackages.perf
      ];

      shellHook = with pkgs; ''
        export BOOST_INCLUDEDIR="${boost.dev}/include"
        export BOOST_LIBRARYDIR="${boost}/lib"
        export AR="gcc-ar"
        export PATH="${gdb-wrapper}/bin:$PATH"
      '';
    };
  };
}
