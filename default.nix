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
  # They come from the system install; point XILINX_VIVADO/XILINX_VITIS at the
  # 2024.1 install roots (see local.mk / README).
  shellHook = ''
    echo "FPGA-Configurator shell (Vivado/Vitis expected from the system install)"
  '';
}
