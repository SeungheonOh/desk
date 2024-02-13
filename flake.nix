{
  description = "Desk";

  outputs = { self, nixpkgs }:
    let pkgs = import nixpkgs { system = "x86_64-linux"; };
    in {
    devShell.x86_64-linux =
      pkgs.mkShell {
        buildInputs = with pkgs; [
          meson
          ninja
          gf
          pkgconfig
          cairo
          wayland
          wayland-protocols
          mesa
          udev
          pixman
          libxkbcommon
          cglm
          vulkan-tools
          glslang
          libdrm
          xorg.libX11
          xorg.libxcb
          xorg.xcbutil
          xorg.xcbutilimage
          xorg.xcbutilkeysyms
          xorg.xcbutilwm
          hwdata
          libpng
          libseat
          libdisplay-info
          libliftoff
          xwayland
          cairo
          ffmpeg
          libinput
          cglm
        ];
      };
  };
}
