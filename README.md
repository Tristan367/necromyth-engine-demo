# Vulkan C++ App

Demo and game client for [Vulkan-C-Engine](../Vulkan-C-Engine).

## Build

```bash
make              # Release build  -> build-Release/
make run          # Release build + run
make debug        # Debug build only -> build-Debug/
make debug run    # Debug build + run
```

Debug and Release use **separate** build directories so `make debug` then `make run` does not reconfigure your Debug tree as Release.

Requires sibling checkout:

```
Projects/
  Vulkan-C-Engine/
  Vulkan-C-App/
```

## Demo scene

Each import path is exercised with real assets from `~/3D Models`:

| Object | Import | Mesh | Texture |
|---|---|---|---|
| Viking room | `.obj` (engine asset) | `viking_room.obj` | `viking_room.png` |
| Susan | `.gltf` + `.bin` | `susan.gltf` | `uvTest.png` (sidecar) |
| Wolf torus | `.gltf` + `.bin` | `wolf-thing.gltf` | `wolf.jpg` (sidecar) |
| Sphere (GLB) | `.glb` embedded textures | `sphere.glb` | 2 primitives, 2 embedded PNGs |
| Sphere (glTF) | `.gltf` + sidecars | `sphere.gltf` | same mesh, external PNGs |
| Suzanne | `.glb` | `Suzanne.glb` | reuses uvTest |
| Torus | `.obj` | `Torus.obj` | **array layer 0** (`brick.png`) |
| Small room | `.obj` | viking mesh | `dirt.png` tile (spinning) |

**glTF vs GLB:** `.glb` packs mesh + optional textures into one binary — great for simple exports. `.gltf` + sidecar files (`.bin`, `.png`, `.jpg`) is the same format split across files; textures stay as separate images next to the `.gltf`.

## Controls

Fly camera: click in the window to look (cursor hidden), Esc to release, WASD move, Space/C vertical, Shift sprint. Esc again (when not captured) quits.

**Debug UI:** ImGui overlay (FPS, present mode, shadow filter/focus toggles). Focus the debug window to use widgets; fly camera pauses keyboard capture while ImGui has focus. Uncap FPS with `ENGINE_PRESENT=mailbox`.

## Lighting and shadows

A single directional sun lives in the frame UBO (`Scene::directional_light()`). The textured mesh shader applies ambient + Lambert diffuse modulated by a **2048×2048** directional shadow map (view-frustum-fitted ortho from the engine — see Vulkan-C-Engine `README.md` / `shadow_utils.hpp`).

Slow rotation on several props is intentional so shadow quality is easy to inspect while flying the camera.
