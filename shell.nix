{ pkgs ? import <nixpkgs> { } }:

pkgs.stdenv.mkDerivation {
  name = "linux-kernel-build";
  nativeBuildInputs = with pkgs; [
    gcc
    cmake
    gnumake
    pkg-config
    binutils
    wayland-scanner
    gobject-introspection
  ];
  buildInputs = with pkgs; [
    elfutils
    ncurses
    openssl
    zlib

    xorg.libX11
    xorg.libXrandr
    xorg.libXinerama
    xorg.libXcursor
    xorg.libXi
    wayland
    libxkbcommon

    glfw
    gtk2
  ];
}
