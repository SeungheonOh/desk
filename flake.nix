{
  description = "Desk";

  outputs = { self, nixpkgs }:
    let pkgs = import nixpkgs { system = "x86_64-linux"; };
    in {
    devShell.x86_64-linux =
      pkgs.mkShell {
        buildInputs = [
          pkgs.cowsay
          pkgs.meson
          pkgs.ninja
          pkgs.cmake
          pkgs.pkgconfig
          pkgs.cairo
          pkgs.wayland
          pkgs.wayland-protocols
          pkgs.mesa
          pkgs.udev
          pkgs.pixman
          pkgs.libxkbcommon
          pkgs.cglm
          pkgs.vulkan-tools
          pkgs.glslang
          pkgs.libdrm
          pkgs.xorg.libX11
          pkgs.xorg.libxcb
          pkgs.xorg.xcbutil
          pkgs.xorg.xcbutilimage
          pkgs.xorg.xcbutilkeysyms
          pkgs.xorg.xcbutilwm
          pkgs.hwdata
          pkgs.libpng
          pkgs.libseat
          pkgs.libdisplay-info
          pkgs.libliftoff
          pkgs.xwayland
          pkgs.ffmpeg
          pkgs.libinput
        ];
      };
  };
}
