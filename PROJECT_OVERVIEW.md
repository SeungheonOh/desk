# Desk: Wayland Window Manager with 3D Desktop Effects

## Project Summary

**Desk** is a custom Wayland compositor/window manager written in C that provides a 3D-animated desktop environment. It combines wlroots (a Wayland library) with OpenGL/GLSL shaders to create smooth, physics-based animations for window movement, rotation, and scaling. The project is a minimal window manager implementation focused on visual effects and interactive window manipulation.

## Technology Stack

### Core Dependencies
- **wlroots**: Wayland/Libinput libraries for display server functionality
- **Wayland**: Core protocol for client-server graphics communication
- **OpenGL (ES 3.0)**: GPU-accelerated rendering with custom shaders
- **Cairo**: 2D graphics rendering for UI elements
- **CGLM**: C Linear Math library for vector/matrix operations
- **FFmpeg**: Video processing (referenced in build but specific usage unclear)
- **Meson**: Build system
- **Ninja**: Build acceleration

### Development Environment
- C language (no C++ or higher-level languages)
- GLSL shaders for visual effects
- Event-driven architecture using Wayland listeners

## Architecture Overview

### Core Components

#### 1. **Server (server.c/h)**
The main application server managing the entire display environment.

**Key Structures:**
- `DeskServer`: Main server state containing:
  - Display, backend, renderer, allocator (Wayland/graphics)
  - XDG shell support for client windows
  - Input handling (keyboard, mouse, cursor)
  - Output management (monitor/display)
  - Animation timer for physics updates
  - Focus tracking and window grab state
  - Super key state for window movement mode

**Key Features:**
- Spring physics-based animation system with velocity and dampening
- Smooth interpolation of window position and rotation
- Animation loop at ~60fps (16ms timer updates)
- Window management with focus handling

#### 2. **Views (view.c/h)**
Represents individual windows/clients in the compositor.

**View Structure Fields:**
- Position: `x, y` (current), `target_x, target_y` (animation target)
- Rotation: `rot` (current), `target_rot` (animation target) in radians
- Velocity: `vel_x, vel_y, rot_vel` for smooth movement
- Scale: `scale` for window sizing
- Animation: `dampening` factor (0.0-1.0, affects smoothness)
- Fade: `fadeIn` for opacity transitions
- Listeners: map, unmap, destroy, move, resize, maximize, fullscreen events

**Capabilities:**
- Smooth spring-based physics animations
- Event-driven lifecycle management
- Focus/keyboard input handling

#### 3. **Outputs (output.c/h)**
Manages display rendering for each physical monitor.

**Output Structure:**
- Per-monitor shader programs for different buffer types
- Render pass management with wlroots
- Three shader programs: window, external window, cursor
- Texture management for UI and screen effects

**Rendering Pipeline:**
1. Initialize shaders on first frame (GL context setup)
2. Clear framebuffer with light gray background
3. Enable blending for transparency
4. Render all views with transformations
5. Render cursor last
6. Present to output

#### 4. **Keyboard Input (keyboard.c/h)**
Manages keyboard input devices.

**Features:**
- Per-keyboard listener for key and modifier events
- Integration with Wayland seat for focus management

#### 5. **Shader System (shader.c/h)**
Abstraction for compiling and managing GLSL shaders.

**Methods:**
- `newShader()`: Compile vertex + fragment shader pair
- `useShader()`: Make shader active for rendering
- `setInt/setFloat/set4fv()`: Uniform variable setting
- `reloadShader()`: Hot-reload for development

### Supporting Components

#### **Events (events.h)**
Macro-based event system for Wayland listener attachment.

**Key Macros:**
- `LISTNER()`: Declares listener function signatures
- `HANDLE()`: Implements listener wrapper with wl_container_of pattern
- `ATTACH()`: Binds listener to signal

This pattern abstracts Wayland's callback mechanism for cleaner code.

#### **Macros & Utils (macro.h, aux.h)**
- `ASSERT/ASSERTN`: Debug assertions with formatted messages
- `GL_CHECK()`: OpenGL error checking macro
- `LOG/DEBUG()`: wlroots logging macros
- `rotateAbout/dilateAbout()`: Geometric transformation helpers
- `DISTP()`: Euclidean distance calculation

## Interaction Model

### Window Movement
1. Super key + mouse button triggers grab mode
2. Cursor movement updates `target_x, target_y` while grabbed
3. Animation loop applies spring physics to smoothly animate windows
4. Release mouse to deactivate grab mode

### Animation System
**Spring Physics Implementation:**
```
Pseudocode:
for each frame (every 16ms):
  for each view:
    dx = target_x - current_x
    velocity_x += dx * stiffness (0.3)
    velocity_x *= dampening (0.35)  // friction
    position_x += velocity_x
    
    // Similar for rotation with angle wrapping
    
    if moved significantly:
      schedule output frame redraw
```

### Rendering Pipeline
1. **Per-frame callback** triggered by output
2. **Shader initialization** on first frame
3. **Clear background** to light gray
4. **Transform and render views** with model/view/projection matrices
5. **Apply window shaders** to draw client surface content
6. **Render cursor** with custom shader
7. **Present** to physical output

## GLSL Shader Programs

The project uses multiple shader pairs for different rendering contexts:

| Shader Pair | Purpose |
|-------------|---------|
| `vert.glsl` + `frag.glsl` | Standard window rendering |
| `vert.glsl` + `frag_external.glsl` | External buffer (e.g., Wayland video) |
| `cursor_vert.glsl` + `cursor_frag.glsl` | Cursor rendering |

**Vertex Shader Pattern:**
- Takes 3D position and 2D texture coordinates
- Applies model/view/projection transformations
- Passes texture coordinates to fragment shader

## Data Flow

```
Wayland Client
    ↓ (XDG Shell Surface)
View (tracks animation state)
    ↓ (render frame event)
Output (manages per-display rendering)
    ↓ (shader rendering)
OpenGL Pipeline (GPU)
    ↓
Display
```

## Key Design Patterns

1. **Listener-based Event System**: All Wayland events use listener callbacks with `wl_container_of` pattern
2. **Spring Physics**: Smooth animations via velocity-based state updates
3. **Per-output Rendering**: Each monitor has independent shader/texture state
4. **Macro Abstraction**: Event system and error checking hide boilerplate

## Configuration

**Shader Paths** (src/config.h):
```c
WINDOW_VERTEX_SHADER: "./src/shader/vert.glsl"
WINDOW_FRAGMENT_SHADER: "./src/shader/frag.glsl"
WINDOW_FRAGMENT_SHADER_EXTERNAL: "./src/shader/frag_external.glsl"
CURSOR_VERTEX_SHADER: "./src/shader/cursor_vert.glsl"
CURSOR_FRAGMENT_SHADER: "./src/shader/cursor_frag.glsl"
```

**Animation Parameters** (src/server.c):
- Stiffness: 0.3 (acceleration towards target)
- Dampening: 0.35 (friction/smoothing factor)
- Frame rate: 16ms (~62fps)
- Snap threshold: 0.5px for position, 0.01rad for rotation

## Building & Running

**Build System**: Meson + Ninja

```bash
# Configure
meson setup builddir

# Build
ninja -C builddir

# Run
./builddir/desk
```

**Dependencies Installation**: Available via Nix flake (`flake.nix`)

## File Structure

```
/src
  ├── desk.c              # Main entry point
  ├── server.{c,h}        # Main server loop and state
  ├── view.{c,h}          # Window abstraction
  ├── output.{c,h}        # Display/monitor rendering
  ├── keyboard.{c,h}      # Keyboard input handling
  ├── shader.{c,h}        # Shader compilation/management
  ├── window.h            # (Alternative window tracking?)
  ├── aux.{c,h}           # Geometry utilities
  ├── macro.h             # Debugging/assertion macros
  ├── events.h            # Event system macros
  ├── imports.h           # All external dependencies
  ├── config.h            # Configuration constants
  ├── events/
  │   └── monitor.c       # Monitor/output event handlers
  └── shader/
      ├── vert.glsl       # Standard vertex shader
      ├── frag.glsl       # Standard fragment shader
      ├── frag_external.glsl
      ├── cursor_vert.glsl
      ├── cursor_frag.glsl
      ├── vert_flat.glsl  # Alternative vertex shaders
      └── frag_flat.glsl
```

## Notable Implementation Details

### Physics-based Animation
- Uses spring physics: `v += dx * stiffness; v *= dampening; pos += v`
- Angle wrapping for rotation: prevents spinning the long way
- Adaptive snapping: snaps to target when velocity is low and distance is small
- Configurable dampening per view

### Event Handling Pattern
```c
// Macro-generated event wrapper
HANDLE(map, void, View) {
    View *view = wl_container_of(listener, view, map);
    // ... handle map event
}
```

### Rendering Context
- Uses wlroots `wlr_render_pass` for abstraction
- GL commands execute within render pass
- Per-output texture management
- Screen capture texture for cursor effects

## Potential Future Extensions

Based on code structure:
1. Multi-monitor support (already designed for)
2. Window resize/maximize animations
3. Workspace management (foundation present)
4. Gesture input (Wayland libinput ready)
5. Improved cursor effects (screen texture prepared)
6. Configuration file loading (framework for parameters)

## Development Notes

- Built with `-O2` optimization and debugoptimized build type
- Uses unstable wlroots API (`-DWLR_USE_UNSTABLE`)
- Extensive error checking with GL_CHECK macros
- Debug logging via wlroots logger
- No external configuration files (hardcoded for now)
