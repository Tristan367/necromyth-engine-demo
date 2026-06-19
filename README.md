# Necromyth Engine Demo

Reference client and tech demo for [Necromyth Engine](https://github.com/Tristan367/necromyth-engine).

## Build

```bash
make              # Release build  -> build-Release/
make run          # Release build + run
make debug        # Debug build only -> build-Debug/
make debug run    # Debug build + run
```

Debug and Release use **separate** build directories so `make debug` then `make run` does not reconfigure your Debug tree as Release.

Requires a checkout of the engine (sibling directory or set `VCE_ROOT`):

```
Projects/
  necromyth-engine/
  necromyth-engine-demo/  # this repo
```

## Demo scene

Each import path is exercised with real assets from `~/3D Models`:

| Object | Import | Mesh | Texture |
|---|---|---|---|
| Susan | `.gltf` + `.bin` | `susan.gltf` | `uvTest.png` (sidecar) |
| Wolf torus | `.gltf` + `.bin` | `wolf-thing.gltf` | `wolf.jpg` (sidecar) |
| Sphere (GLB) | `.glb` embedded textures | `sphere.glb` | 2 primitives, 2 embedded PNGs |
| Sphere (glTF) | `.gltf` + sidecars | `sphere.gltf` | same mesh, external PNGs |
| Suzanne | `.glb` | `Suzanne.glb` | reuses uvTest |
| Torus | `.obj` | `Torus.obj` | **array layer 0** (`brick.png`) |
| Floor / alpha quads | procedural (`demo_meshes.hpp`) | — | tile / alphaTest textures |
| Spinning torus | `.obj` | `Torus.obj` | `dirt.png` tile |

**glTF vs GLB:** `.glb` packs mesh + optional textures into one binary — great for simple exports. `.gltf` + sidecar files (`.bin`, `.png`, `.jpg`) is the same format split across files; textures stay as separate images next to the `.gltf`.

## Controls

**Menu (default):** cursor visible. **Resume** or **Esc** enters fly mode (cursor hidden). **Esc** again opens the menu. **Quit** exits from the menu (window close and Ctrl+C still work).

Fly mode: WASD move, Space/C vertical, Shift sprint.

**Debug UI:** separate panel (FPS, shadow toggles). Usable while the menu is open. Uncap FPS with `ENGINE_PRESENT=mailbox`.

## Lighting and shadows

A single directional sun lives in the frame UBO (`Scene::directional_light()`). The textured mesh shader applies ambient + Lambert diffuse modulated by a **2048×2048** directional shadow map (camera-footprint ortho, dual cascade by default — see engine `README.md` / `shadow_utils.hpp`).

Slow rotation on several props is intentional so shadow quality is easy to inspect while flying the camera.

## License

[MIT](LICENSE) — same terms as [Necromyth Engine](https://github.com/Tristan367/necromyth-engine). Engine contributions: see [CONTRIBUTING.md](https://github.com/Tristan367/necromyth-engine/blob/master/CONTRIBUTING.md).
