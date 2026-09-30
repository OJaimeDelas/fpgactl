{ pkgs ? import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/25.05.tar.gz") {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    gnumake
    python3
    rsync
    openssh
    unzip
    coreutils
    gnugrep
  ];
  # Vivado/Vitis/xsct are deliberately NOT provided by nix.
  # They come from the system install; point VIVADOPATH/VITISPATH at the
  # installs (see local.mk / README).
  # FPGACTL_NIX=1 tells the Makefile it is already inside this shell
  # (so it does not re-exec itself into nix-shell again).
  shellHook = ''
    export FPGACTL_NIX=1
  '';
}
