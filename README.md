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

| Key | Action |
|-----|--------|
| Tab | Toggle character mode (third-person character controller) |
| Esc | Menu / resume fly mode (cursor visible/hidden) |
| F3 | Toggle Jolt debug wireframes — physics bodies (green), hitboxes (yellow), character capsule (red) |
| Left-click | Raycast from camera center against all physics bodies; prints hitbox name on character hits |
| E | Swap primary/secondary animation (per-bone split swaps upper/lower body clip assignment) |
| WASD | Move (fly mode or character mode) |
| Space/C | Up/down (fly mode), jump (character mode) |
| Shift | Sprint |
| Quit | Menu → Quit, window close, or Ctrl+C |

## Debug visualization (F3)

Press F3 to toggle Jolt physics debug wireframes:
- **Green**: all physics bodies (spheres, boxes, capsules, cylinders, tapered variants, trimesh terrain)
- **Red**: character collision capsule
- **Yellow**: per-bone hitbox spheres (defined in `<model>.json`)

Wireframe rendering runs through a standalone Vulkan line-list pipeline (`DebugLineRenderer` in `debug_renderer.hpp`). No depth test — lines always render on top.

## Animation split

The animation test model (Icosphere with 11 bones, 2 clips) demonstrates per-bone animation assignment:
- Joints 0-1 (root+spine) play primary clip
- Joints 2-10 (chest+head+arms) play secondary clip
- Press E to swap which clip is primary vs secondary
- Hitboxes (11 spheres on every bone) confirm per-bone positions

System: `MeshInstance::secondary_joints` pointer → `compute_joint_matrices_split()` in engine. Zero overhead when null.

## Hitbox system

Per-skeleton collision attachments defined in `<model_name>.json`:
- **Body collider**: physics interactions (ground, rigidbodies, character push)
- **Hitboxes**: sensor bodies on specific bones for raycast detection (headshots, limb damage)
- References bones by name or joint index
- F3 shows hitboxes as yellow wireframes
- Left-click raycasts against hitbox layer, prints name on hit
- Falls back to body collider raycast when no hitboxes configured

New Jolt layer (`kHitbox`) — sensor-only, no collision response.

## Lighting and shadows

A single directional sun lives in the frame UBO (`Scene::directional_light()`). The textured mesh shader applies ambient + Lambert diffuse modulated by a **2048×2048** directional shadow map (camera-footprint ortho, dual cascade by default — see engine `README.md` / `shadow_utils.hpp`).

Slow rotation on several props is intentional so shadow quality is easy to inspect while flying the camera.

## License

[MIT](LICENSE) — same terms as [Necromyth Engine](https://github.com/Tristan367/necromyth-engine). Engine contributions: see [CONTRIBUTING.md](https://github.com/Tristan367/necromyth-engine/blob/master/CONTRIBUTING.md).
