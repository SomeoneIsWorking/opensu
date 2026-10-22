# Frame generation behaviour specification (LSFG 3 shaders from the player's Lossless.dll)

Status: implemented by the Gamescope fork's `CLosslessSynthesizer` (`src/FrameGen/`), which runs
the SPIR-V modules of the player's DLL on Gamescope's device; proof in
`docs/issues/adaptive-lsfg-vk.md`, "Lossless generator". It describes what Lossless
Scaling 3.2.2 (Steam build 19655272) does with the frame-generation shaders in its `Lossless.dll`,
so that our MIT generator behind the Gamescope fork's `IFrameSynthesizer::Generate(previous,
current, t, output)` (`docs/issues/adaptive-lsfg-vk.md`, "Decision") can run the same shaders from
the player's own install. Upscaling, LSFG 1/2 and LSFI are out of scope.

Clean-room note: written from the player's install only. `Lossless.dll` was read with Ghidra 12.0.4
(project `scratch/lsfg-re/ghidra`, our earlier RE of the same file) and wrestool, the shaders with
vkd3d-compiler 1.17 and spirv-dis, and `LosslessScaling.dll` with ilspycmd 11.1. No lsfg-vk source
was read. `scratch/lsfg-re/` contains only our own Ghidra and ILSpy output; nothing in it came from
lsfg-vk. The adaptive-lsfg-vk card mentions lsfg-vk's structures in places; this spec does not use
them. Work files are in gitignored `scratch/lsfg-spec-re/` (`res/` raw resources, `asm/` vkd3d
disassembly, `spvdis/` SPIR-V disassembly, `out/` Ghidra output and tables, `scripts/`).

Citations: `FUN_1800xxxxx` is a function in `Lossless.dll` at that image address (image base
0x180000000). `DAT_1800xxxxx` is a constant there. "Resource N" is an `RT_RCDATA` resource with
integer id N. "Object +0xNN" is a field of Lossless Scaling's LSFG3 object, given only to tie a
fact to the code. **Inferred** marks anything not read directly from code or data.

## 1. Inputs, settings and what the generator owns

The generator owns: the shader modules, every intermediate image, the per-level uniform buffers,
the samplers, the order of dispatches, and the two-slot history of real frames. Its inputs are one
new real frame at a time, plus a timestamp `t` in (0, 1) for each frame to generate. Its output is
an image at the input's size, holding the frame at time `t` between the previous and the newest
real frame.

| Setting | Values | Source in Lossless Scaling | Effect |
|---|---|---|---|
| Size mode | 0 = BALANCED, 1 = PERFORMANCE | .NET `Profile.LSFGSizeEnum`, `Profile.LSFGSize`; `ApplySettings` arg 10 (`frameGenerationSize`) → `DAT_180050554` → object +0x08 (`FUN_180010f70` @ 0x180011d1d) | Shader set and channel counts (§5) |
| Flow scale | 25..100 % in steps of 5, default 100 | `Profile.LSFGFlowScale`, slider min 25 max 100 tick 5 (`UI.Pages/ProfilePage.xaml`); `ApplySettings` arg 14 → `DAT_1800505b0`; optional override `[lsfg] flow_scale` in `config.ini` (integer percent); object +0x38 = percent / 100 (`DAT_1800449e0` = 100.0) | Resolution of motion estimation (§3) |
| UI detection | 0 off, non-zero on; default 1 | `config.ini` `[lsfg] ui_detection_rate` (shipped as 1; 1 when the key is absent), `FUN_180010f70` → object +0x1c | Adds the UI-mask passes (§4.2) and binds the mask in two passes |
| UI threshold | float, default 0.5 | `config.ini` `[lsfg] ui_detection_threshold` (shipped 0.5; default `DAT_18004492c` = 0.5) → object +0x20 | Written to `UiThreshold` (§2.3) |
| UI debug | 0 / non-zero, default 0 | `config.ini` `[lsfg] ui_detection_debug` → object +0x24 | Output becomes the UI mask instead of the frame |
| Iteration delta | integer, default 0 | `config.ini` `[lsfg] iteration_delta` (not in the shipped file) → object +0x3c | Number of levels of the second and third flow (`L2 = 3 + delta`) |
| HDR | bool | `Profile.HdrSupport`; `ApplySettings` arg 22 → object +0x409 of the scaler | Image format of the frame copies (§3) and the `HdrSupport` field |
| Real-frame snap | 0.05 | `config.ini` `[lsfg] real_timestamp_tolerance` (`FUN_180014c90`) | `t` within this of 0 or 1 returns a real frame (§4.3) |

Frame generation runs only when `ApplySettings` arg 9 (`frameGenerationType`, .NET
`Profile.FrameGenerationEnum`: Off, LSFG3, LSFG2, LSFG1, LSFI, LSFI1) is 1 = LSFG3; that branch
allocates the 0x11f8-byte LSFG3 object (`FUN_180010f70` @ 0x180011cbd). Fixed/adaptive mode,
multiplier and target only drive pacing (`docs/issues/adaptive-lsfg-vk.md`, "Lossless Scaling's
Adaptive mode") and do not reach the shaders.

## 2. Shader storage and extraction

### 2.1 Where the shaders are

- `Lossless.dll` holds 300 `RT_RCDATA` (type 10) resources, ids 101..400, language 1033 (wrestool
  listing, `scratch/lsfg-spec-re/out/resources.txt`). Its only other resources are VERSIONINFO
  (type 16) and a manifest (type 24).
- Ids 101..302 are DXBC (magic `DXBC`, chunks RDEF/ISGN/OSGN/SHEX/STAT, plus SFI0 on the
  min-precision ones), compute shader model 5.0, creator "Microsoft (R) HLSL Shader Compiler 10.1".
- Ids 303..400 are SPIR-V 1.0 modules (magic 0x07230203, generator word 0x000e0000), every one a
  `GLCompute` entry point named `main`, `OpSource HLSL 620`, with debug names kept.
- Lossless Scaling itself only ever loads DXBC: `FUN_180028730` calls `FindResourceW(module, id,
  RT_RCDATA)`, `LoadResource`, `SizeofResource`, `LockResource`, then
  `ID3D11Device::CreateComputeShader` (vtable +0x90) with the bytes. It imports no Vulkan; the
  SPIR-V resources are not referenced by any code path found (no `FindResourceW` caller passes
  303..400).

### 2.2 Which ids are LSFG 3

`FUN_18001fe10` (logs "LSFG3 shaders init success") loads exactly these, by size mode:

| Role (our name, §4) | Balanced id | Performance id | SPIR-V FP16 | SPIR-V FP32 |
|---|---|---|---|---|
| Copy | 254 | 254 | 303 | 352 |
| LumaPyramid | 255 | 255 | 304 | 353 |
| Compose | 256 | 256 | 305 | 354 |
| FlowEntry (G1, H1) | 257 | 280 | 306 / 329 | 355 / 378 |
| FlowBEntry2 (H6) | 258 | 281 | 307 / 330 | 356 / 379 |
| FlowA filters (G2, G3, G4) | 259, 260, 261 | 282, 283, 284 | 308-310 / 331-333 | 357-359 / 380-382 |
| FlowA resolve (G5) | 262 | 285 | 311 / 334 | 360 / 383 |
| FlowB filters (H2, H3, H4) | 263, 264, 265 | 286, 287, 288 | 312-314 / 335-337 | 361-363 / 384-386 |
| FlowB resolve (H5) | 266 | 289 | 315 / 338 | 364 / 387 |
| Feature stages (I2..I5) | 267, 268, 269, 270 | 290, 291, 292, 293 | 316-319 / 339-342 | 365-368 / 388-391 |
| FlowC filters (H7, H8, H9) | 271, 272, 273 | 294, 295, 296 | 320-322 / 343-345 | 369-371 / 392-394 |
| FlowC resolve (H10) | 274 | 297 | 323 / 346 | 372 / 395 |
| UI stages (U0..U3) | 275, 276, 277, 278 | 298, 299, 300, 301 | 324-327 / 347-350 | 373-376 / 396-399 |
| UI mask (U4), only with UI detection | 279 | 302 | 328 / 351 | 377 / 400 |

The SPIR-V columns are the DXBC id plus 49 (FP16 set) and plus 98 (FP32 set), balanced before
performance. That mapping is **inferred from interfaces**: for all 98 pairs the resource names
(`Sampler`, `Sampler1`, `InputN`, `OutputN`, `OutputMipN`), counts, constant-buffer presence and
thread-group size are identical (`scripts/pairs.py`: 98 pairs, 0 mismatches). Equivalence of the
code itself was not proven. Ids 101..253 are LSFG 1/2, LSFI and the upscalers (`FUN_1800176b0`,
`FUN_18001d610`, `FUN_18001e240`, `FUN_18001cc20`, `FUN_180026f30`, `FUN_1800279c0`,
`FUN_180028240`) and are not used.

### 2.3 Interface of every LSFG 3 shader

Every DXBC shader declares the same constant buffer `cb` (register b0), 32 bytes, or 48 for the UI
mask shader; the CPU always fills a 48-byte block (`FUN_18001e890`, `CreateBuffer` ByteWidth 0x30).
The SPIR-V modules keep `cb` only where the shader reads a field of it (Copy, LumaPyramid, Compose,
the flow entry and resolve shaders, the UI mask); the filter, feature and UI stages declare no
binding 0. Layout from the DXBC RDEF chunk and identical `OpMemberDecorate Offset`s in the SPIR-V:

| Offset | Field | Type | Meaning and value written |
|---|---|---|---|
| 0x00 | `InputOffset` | uint2 | Origin of the frame inside the captured texture. Read only by Copy (254), which loads input texel `id + InputOffset` (its disassembly). Set to object +0x3b0/+0x3b4 of the scaler (`FUN_180010f70` @ 0x180011d01, 0x180011cfb); **inferred** to be the crop origin. Ours: (0, 0) |
| 0x08 | `FirstIter` | uint | 1 when this level is the coarsest flow level (`l == L - 1`), else 0 |
| 0x0c | `FirstIterS` | uint | 1 when `l == L2 - 1`, else 0 |
| 0x10 | `AdvancedColorKind` | uint | Object arg 8 (scaler +0x40c; 0 when capture API is 1, otherwise an argument of `FUN_180010f70`). LumaPyramid applies a 1/2.2 power when it equals 1 (disassembly of 255). **Inferred**: the display's DXGI advanced colour kind. Ours: 0 for 8-bit SDR |
| 0x14 | `HdrSupport` | uint | HDR setting (0/1). LumaPyramid scales by 0.1333 then applies 1/2.2 when set. Ours: 0 |
| 0x18 | `ResolutionInvScale` | float | `1.0 / flowScale` (`DAT_18004493c` = 1.0 over object +0x38), after the clamp in §3 |
| 0x1c | `Timestamp` | float | `t` of the frame being generated; 0 at creation, rewritten for every level before each generated frame (`FUN_180021940`) |
| 0x20 | `UiThreshold` | float | UI threshold (object +0x20); only shader 279/302 declares it |
| 0x24..0x2f | — | — | zero |

Fields used per shader (RDEF "used" flag): Copy uses `InputOffset`; LumaPyramid uses
`AdvancedColorKind`, `HdrSupport`, `ResolutionInvScale`; Compose uses `ResolutionInvScale`,
`Timestamp`; 257/262/266 and 280/285/289 use `FirstIter`, `Timestamp`; 258/281 `FirstIterS`,
`Timestamp`; 274/297 `FirstIterS`; 279/302 `UiThreshold`; all others no field.

Thread groups (DXBC `dcl_thread_group` = SPIR-V `LocalSize`): Copy 8x8, LumaPyramid 32x32, Compose
16x16, UI mask 32x32, every other shader 8x8 (`out/table.txt`).

Interface of each SPIR-V module, balanced / performance where they differ (`out/table.txt`; the
same counts as the src/dst columns of section 4). "cb" is the number of members (7, or 8 with
`UiThreshold`), samplers are `Sampler` then `Sampler1`, inputs `Input1..`, outputs `Output1..`
except LumaPyramid (`Output0..6`) and UI mask (`OutputMip0..5`):

| Shader | cb | Samplers | Inputs | Outputs | Output format |
|---|---|---|---|---|---|
| Copy | 7 | 0 | 1 | 1 | Rgba32f |
| LumaPyramid | 7 | 1 | 1 | 7 | R8 |
| Compose | 7 | 2 | 5 | 1 | Unknown |
| FlowEntry (G1, H1) | 7 | 2 | 9 / 5 | 3 | Rgba8 |
| FlowBEntry2 (H6) | 7 | 2 | 10 / 6 | 2 / 1 | Rgba8 |
| FlowA/B filter 1 (G2, H2) | - | 1 | 3 | 4 / 2 | Rgba8 |
| FlowA/B filters 2, 3 | - | 1 | 4 / 2 | 4 / 2 | Rgba8 |
| FlowA/B resolve (G5, H5) | 7 | 2 | 6 / 4 | 1 | Rgba16f |
| Feature I2, I3 | - | 1 | 1 / 1, 2 / 1 | 2 / 1 | Rgba8 |
| Feature I4, I5 | - | 1 | 2 / 1, 4 / 2 | 4 / 2 | Rgba8 |
| FlowC filters (H7..H9) | - | 1 | 2 / 1 | 2 / 1 | Rgba8 |
| FlowC resolve (H10) | 7 | 2 | 3 / 2 | 1 | Rgba16f |
| UI U0 | - | 1 | 12 / 6 | 2 | Rgba8 |
| UI U1..U3 | - | 1 | 2 | 2 | Rgba8 |
| UI mask (U4) | 8 | 1 | 2 | 6 | R8 |

A shader with one sampler gets the pass's s0 only; the s1 = `Sc` of the pass tables matters only to
the two-sampler shaders. U0 (one sampler) therefore samples with `S1`.

Bindings, all in descriptor set 0 in the SPIR-V modules (DXC's register shifts, observed):

| D3D register | SPIR-V binding | Vulkan descriptor type |
|---|---|---|
| b0 `cb` | 0 | `UNIFORM_BUFFER` (Block, `Uniform` storage class) |
| s0 `Sampler`, s1 `Sampler1` | 16, 17 | `SAMPLER` |
| t0..t11 `Input1..Input12` | 32..43 | `SAMPLED_IMAGE` (separate image and sampler) |
| u0..u6 `Output1..`, `Output0..6`, `OutputMip0..5` | 48..54 | `STORAGE_IMAGE` |

Storage image formats declared in the SPIR-V: `Rgba8` for every RGBA8 intermediate, `Rgba16f` for
the three flow fields, `R8` for the luma pyramid and UI mask, `Unknown` for Compose's output
(needs `shaderStorageImageWriteWithoutFormat`), and `Rgba32f` for Copy's output (303/352). The last
does not match the RGBA8 or RGBA16F image Lossless Scaling writes it to on D3D11 (§3); with the
embedded SPIR-V the copy target must be `R32G32B32A32_SFLOAT`, and our generator makes it so (it
needs linear filtering of that format, which LumaPyramid and Compose sample). Compose samples the
copies through a sampler, so their format is otherwise free.

Capabilities: the FP16 set (306..351) declares `Float16` (needs `shaderFloat16`, Vulkan 1.2 or
`VK_KHR_shader_float16_int8`; no 16-bit integers or 16-bit storage); 303..305 and the FP32 set do
not. Compose's `Unknown` output needs `shaderStorageImageWriteWithoutFormat`, the R8 outputs
`shaderStorageImageExtendedFormats`, and binding a missing input as a null descriptor
(`VK_EXT_robustness2` `nullDescriptor`, which Gamescope's device enables) gives D3D11's zero reads. `StorageImageExtendedFormats`
(R8 storage) appears in the luma and UI-mask modules. All 98 modules pass `spirv-val --target-env
vulkan1.2`; a sample also passes vulkan1.0. The DXBC versions of 257..302 carry the
`enableMinimumPrecision` global flag (min16float); 254..256 do not.

### 2.4 Extraction at run time

1. Resolve the player's `Lossless.dll` (see the card's "Lossless.dll for openSU"). Read it in place,
   never copy it.
2. Parse the PE resource directory: root → type 10 → name id → language 1033 → data entry (RVA,
   size). Map the RVA through the section table to a file offset. This is the standard PE layout;
   no Windows API is needed.
3. For each id in §2.2, take the bytes, check the magic (`DXBC` for 254..302, 0x07230203 for
   303..400), and check the interface (§7).
4. Preferred: create `VkShaderModule`s straight from 303..351 (when `shaderFloat16` is available)
   or 352..400. No translation is needed. Descriptor set layouts follow the bindings above.
5. Fallback, for a DLL without the SPIR-V resources: translate the DXBC.

### 2.5 DXBC to SPIR-V translation, if needed

| Tool | Licence | Result on these shaders |
|---|---|---|
| vkd3d-shader (`libvkd3d-shader`, `vkd3d-compiler -x dxbc-tpf -b spirv-binary`) | LGPL-2.1-or-later | Run on all 49 LSFG 3 DXBC resources: 49 translate, 49 pass `spirv-val --target-env vulkan1.1`. It warns "Unhandled global flags 0x21/0x31" (min precision is dropped, so FP32). Its default bindings are sequential; the library's `vkd3d_shader_interface_info` sets them. Linking it dynamically keeps our code MIT; it is LGPL, so linking statically into the BSD fork is not an option. |
| DXVK's DXBC compiler | zlib | Not tried. It is internal to DXVK, not a library. |
| SPIRV-Tools (`spirv-val`, `spirv-dis`) | Apache-2.0 | Validation and inspection only. |
| SPIRV-Reflect | Apache-2.0 | Optional, for the interface check in §7. |

Tested with vkd3d-compiler 1.17 in a Fedora 44 container (`scratch/lsfg-spec-re/spv/`).

## 3. Images, sizes and formats

Notation: `W x H` is the real frame's size. `fw = floor(flowScale * W)`, `fh = floor(flowScale * H)`.
For a level `l`: `a(l) = fw div 2^l` (integer division), `h(l) = ceil(a(l) / 2)`, `q(l) = ceil(h(l) / 2)`,
and the same for heights. `div` is integer division; `ceil` acts on floats (`FUN_18001e890`;
`DAT_180044980` = 2.0, `DAT_18004492c` = 0.5).

- Levels of the first flow: `L = 7` when `flowScale * H > 512` (`DAT_1800449f4`), else 6. This
  uses the flow scale before the clamp below.
- Levels of the second and third flow: `L2 = 3 + iterationDelta` (object +0x18 = +0x3c + 3).
  It must not exceed 6, the size of those view arrays (object layout; **inferred** limit).
- Clamp: if `q(0) div 32 == 0` for the width or the height (`2^DAT_180044990`, 2^5), the flow scale
  becomes `ceil(max(128 / W, 128 / H) * 100) / 100` (`2^DAT_180044998` = 128) and `fw`, `fh` are
  recomputed. The rounding is to whole percent.

Every image is a 2D texture with one mip level, one layer, no multisampling, usable as both
sampled and storage (`BindFlags` 0x88), default usage. "Mips" below are separate images. Formats
are DXGI 28 = `R8G8B8A8_UNORM`, 61 = `R8_UNORM`, 10 = `R16G16B16A16_FLOAT`.

| Our name | Count | Size | Format | Object fields (SRV / UAV) |
|---|---|---|---|---|
| `FrameCopy[s]` | 2 (`s` = slot) | W x H | RGBA8_UNORM, or RGBA16_FLOAT when HDR | +0x2e8 / +0x2d8 |
| `Output[o]` | 2 | W x H | as `FrameCopy` | +0x1118 / +0x1108 |
| `Luma[i]` | 7 (i = 0..6) | `a(i)` x ... | R8_UNORM | +0x330 / +0x2f8 |
| `Feature[k][c][l]` | 3 x 4 x L | `q(l)` | RGBA8_UNORM | +0x668 / +0x368, index `l + 8c + 32k` |
| `Half[c][l]` | nH x L (4 balanced, 2 performance) | `h(l)` | RGBA8_UNORM | +0xa68 / +0x968, index `l + 8c` |
| `Quarter[c][l]` | nQ x L (8 balanced, 5 performance) | `q(l)` | RGBA8_UNORM | +0xd68 / +0xb68, index `l + 8c` |
| `FlowA[l]` | L | `q(l)` | RGBA16_FLOAT | +0xfa8 / +0xf68 |
| `FlowB[l]` | L2 | `q(l)` | RGBA16_FLOAT | +0x1018 / +0xfe8 |
| `FlowC[l]` | L2 | `q(l)` | RGBA16_FLOAT | +0x1078 / +0x1048 |
| `UiMask[i]` | 6, only with UI detection | `q(0) div 2^i` | R8_UNORM | +0x10d8 / +0x10a8 |
| `LevelCB[l]` | L | 48 bytes | uniform | data +0x258, buffer +0x298 |

`Feature` is allocated with 4 channels in both modes; performance uses channels 0 and 1. Slot and
ring counters (object +0x40 `s`, +0x44 `k`, +0x48 `o`) start at 0 and are reset by the release
function `FUN_180024670`.

Worked sizes (from the formulas above):

| Input, flow scale | fw x fh | L | q(0) | q(L-1) |
|---|---|---|---|---|
| 1920x1080, 1.0 | 1920x1080 | 7 | 480x270 | 8x4 |
| 1920x1080, 0.5 | 960x540 | 7 | 240x135 | 4x2 |
| 2560x1440, 0.25 | 640x360 | 6 | 160x90 | 5x3 |
| 320x240, 0.25 | clamped to 0.54: 172x129 | 6 | 43x33 | 2x1 |

Memory at 1920x1080, flow 1.0, balanced, SDR: about 60 MB (two copies and two outputs 33 MB,
`Half` 11 MB, `Feature` 8 MB, `Quarter` 5.5 MB, flows 4 MB, luma 2.8 MB). **Inferred** arithmetic,
not measured.

## 4. The pipeline

Per real frame: Ingest (§4.1), then UI mask (§4.2) when enabled. Per generated frame: Generate
(§4.3). Order in the capture loop `FUN_180014c90`: `FUN_180020260` (Ingest, call @ 0x180015749) before
the capture frame is released, then the object's virtual slot 1 (`FUN_180020d90`, UI mask), then
`FUN_180021940` (Generate) once per output timestamp (call @ 0x1800158bb). Real frames are shown
through Generate too: it returns a `FrameCopy` when `t` is near 0 or 1.

Notation in the tables: `p = (k + 2) mod 3` is the previous real frame's feature slot, `k` the
newest; `src` is the shader's SRV slots in order (t0, t1, ...), `dst` its UAV slots (u0, ...).
Every slot not listed is null; on D3D11 a null SRV reads zero, so a Vulkan port binds a zero image
there (or uses `VK_EXT_robustness2` null descriptors). Dispatch sizes are group counts `(x, y, 1)`.
`S0` = trilinear, border (0,0,0,0) (object +0x60); `S1` = trilinear, border (1,1,1,1) (+0x68); `Sc`
= trilinear, clamp (+0x70) (§6).

### 4.1 Ingest, once per real frame (`FUN_180020260`)

1. `s = (s - 1) & 1` (toggle), then:

| Pass | Shader (bal / perf) | cb | Samplers s0, s1 | src | dst | Dispatch |
|---|---|---|---|---|---|---|
| I0 Copy | 254 | LevelCB[0] | S0, Sc | the captured frame | `FrameCopy[s]` | ceil(W/8), ceil(H/8) |
| I1 LumaPyramid | 255 | LevelCB[0] | S0, Sc | `FrameCopy[s]` | `Luma[0..6]` | ceil(fw/64), ceil(fh/64) (`DAT_1800448f4` = 1/64) |

2. `k = (k + 1) mod 3`.
3. For `l` from `L - 1` down to 0, with `LevelCB[l]` bound and samplers unchanged (S0, Sc):

| Pass | Shader | src (balanced) | dst (balanced) | src (performance) | dst (performance) | Dispatch |
|---|---|---|---|---|---|---|
| I2 | 267 / 290 | `Luma[min(l, 6)]` | `Half[0..1][l]` | `Luma[min(l, 6)]` | `Half[0][l]` | ceil(h(l)/8) |
| I3 | 268 / 291 | `Half[0..1][l]` | `Half[2..3][l]` | `Half[0][l]` | `Half[1][l]` | ceil(h(l)/8) |
| I4 | 269 / 292 | `Half[2..3][l]` | `Quarter[0..3][l]` | `Half[1][l]` | `Quarter[0..1][l]` | ceil(q(l)/8) |
| I5 | 270 / 293 | `Quarter[0..3][l]` | `Feature[k][0..3][l]` | `Quarter[0..1][l]` | `Feature[k][0..1][l]` | ceil(q(l)/8) |

Result: `FrameCopy[s]` holds the new frame and `Feature[k][*][*]` its feature pyramid; the previous
frame is `FrameCopy[1 - s]` and `Feature[p]`. Dispatches per real frame: `2 + 4L`.

### 4.2 UI mask, once per real frame when UI detection is on (`FUN_180020d90`)

All at level 0, `LevelCB[0]` bound. Same bindings in both modes except U0.

| Pass | Shader | Samplers | src | dst | Dispatch |
|---|---|---|---|---|---|
| U0 | 275 / 298 | S1, Sc | balanced: `Feature[(k+1) mod 3][0..3][0]`, `Feature[p][0..3][0]`, `Feature[k][0..3][0]` (12); performance: the same with channels 0..1 (6) | `Quarter[0..1][0]` | ceil(q(0)/8) |
| U1 | 276 / 299 | S0, Sc | `Quarter[0..1][0]` | `Quarter[2..3][0]` | ceil(q(0)/8) |
| U2 | 277 / 300 | S0, Sc | `Quarter[2..3][0]` | `Quarter[0..1][0]` | ceil(q(0)/8) |
| U3 | 278 / 301 | S0, Sc | `Quarter[0..1][0]` | `Quarter[2..3][0]` | ceil(q(0)/8) |
| U4 | 279 / 302 | S0, Sc | `Quarter[2..3][0]` | `UiMask[0..5]` | ceil(q(0)/32) (`DAT_1800448f8` = 1/32) |

U0 reads the three most recent real frames, so the mask is meaningful from the third real frame on
(**inferred**).

### 4.3 Generate, once per output frame (`FUN_180021940(object, &out, t, tolerance)`)

- If `t <= tolerance`: output `FrameCopy[1 - s]` (the previous real frame); nothing is dispatched.
- If `t >= 1 - tolerance`: output `FrameCopy[s]` (the newest real frame).
- Otherwise run the passes below, then output `Output[o]`, or `UiMask[0]` when UI debug is set.
  Lossless Scaling uses tolerance = `real_timestamp_tolerance` (0.05).

First set `Sc` on s1 for the whole sequence. Then for `l` from `L - 1` down to 0: write `t` to
`LevelCB[l].Timestamp` and bind `LevelCB[l]`.

First flow, every level:

| Pass | Shader | s0 | src (balanced) | src (performance) | dst (bal / perf) | Dispatch |
|---|---|---|---|---|---|---|
| G1 | 257 / 280 | S1 | `Feature[p][0..3][l]`, `Feature[k][0..3][l]`, t8 = `FlowA[l+1]` if `l < L-1` | `Feature[p][0..1][l]`, `Feature[k][0..1][l]`, t4 = `FlowA[l+1]` if `l < L-1` | `Quarter[0..2][l]` (both) | ceil(q(l)/8) |
| G2 | 259 / 282 | S0 | `Quarter[0..2][l]` | `Quarter[0..2][l]` | `Quarter[4..7][l]` / `Quarter[3..4][l]` | ceil(q(l)/8) |
| G3 | 260 / 283 | S0 | `Quarter[4..7][l]` | `Quarter[3..4][l]` | `Quarter[0..3][l]` / `Quarter[0..1][l]` | ceil(q(l)/8) |
| G4 | 261 / 284 | S0 | `Quarter[0..3][l]` | `Quarter[0..1][l]` | `Quarter[4..7][l]` / `Quarter[3..4][l]` | ceil(q(l)/8) |
| G5 | 262 / 285 | S0 | `Quarter[4..7][l]`, t4 = `FlowA[l+1]` if `l < L-1`, t5 = `UiMask[min(l, 5)]` if UI on | `Quarter[3..4][l]`, t2 = `FlowA[l+1]` if `l < L-1`, t3 = `UiMask[min(l, 5)]` if UI on | `FlowA[l]` | ceil(q(l)/8) |

Second and third flow, only when `l < L2`:

| Pass | Shader | s0 | src (balanced) | src (performance) | dst (bal / perf) | Dispatch |
|---|---|---|---|---|---|---|
| H1 | 257 / 280 | S1 | as G1 but t8 = `FlowB[l+1]` if `l < L2-1` | as G1 but t4 = `FlowB[l+1]` if `l < L2-1` | `Quarter[0..2][l]` | ceil(q(l)/8) |
| H2 | 263 / 286 | S0 | `Quarter[0..2][l]` | `Quarter[0..2][l]` | `Quarter[4..7][l]` / `Quarter[3..4][l]` | ceil(q(l)/8) |
| H3 | 264 / 287 | S0 | `Quarter[4..7][l]` | `Quarter[3..4][l]` | `Quarter[0..3][l]` / `Quarter[0..1][l]` | ceil(q(l)/8) |
| H4 | 265 / 288 | S0 | `Quarter[0..3][l]` | `Quarter[0..1][l]` | `Quarter[4..7][l]` / `Quarter[3..4][l]` | ceil(q(l)/8) |
| H5 | 266 / 289 | S0 | `Quarter[4..7][l]`, t4 = `FlowB[l+1]` if `l < L2-1`, t5 = `UiMask[min(l, 5)]` if UI on | `Quarter[3..4][l]`, t2 = `FlowB[l+1]` if `l < L2-1`, t3 = `UiMask[min(l, 5)]` if UI on | `FlowB[l]` | ceil(q(l)/8) |
| H6 | 258 / 281 | S1 | `Feature[p][0..3][l]`, `Feature[k][0..3][l]`, t8 = `FlowA[l+1]` (always), t9 = `FlowB[l+1]` if `l < L2-1` | `Feature[p][0..1][l]`, `Feature[k][0..1][l]`, t4 = `FlowA[l+1]`, t5 = `FlowB[l+1]` if `l < L2-1` | `Quarter[0..1][l]` / `Quarter[0][l]` | ceil(q(l)/8) |
| H7 | 271 / 294 | S0 | `Quarter[0..1][l]` | `Quarter[0][l]` | `Quarter[2..3][l]` / `Quarter[1][l]` | ceil(q(l)/8) |
| H8 | 272 / 295 | S0 | `Quarter[2..3][l]` | `Quarter[1][l]` | `Quarter[0..1][l]` / `Quarter[0][l]` | ceil(q(l)/8) |
| H9 | 273 / 296 | S0 | `Quarter[0..1][l]` | `Quarter[0][l]` | `Quarter[2..3][l]` / `Quarter[1][l]` | ceil(q(l)/8) |
| H10 | 274 / 297 | S0 | `Quarter[2..3][l]`, t2 = `FlowC[l+1]` if `l < L2-1` | `Quarter[1][l]`, t1 = `FlowC[l+1]` if `l < L2-1` | `FlowC[l]` | ceil(q(l)/8) |

H6 binds `FlowA[l+1]` without a level check; it always exists because `l + 1 <= L2 <= 6 < L` with
the default delta. With a large `iteration_delta` it can fall past `L - 1`, where D3D11 sees a null
view (zeros).

After the loop (all levels done), `o = (o - 1) & 1`, then:

| Pass | Shader | cb | Samplers | src | dst | Dispatch |
|---|---|---|---|---|---|---|
| F Compose | 256 | LevelCB[0] (Timestamp = t) | S0, Sc | `FrameCopy[1-s]`, `FrameCopy[s]`, `FlowA[0]`, `FlowB[0]`, `FlowC[0]` | `Output[o]` | ceil(W/16), ceil(H/16) (`DAT_180044904` = 1/16) |

Dispatches per generated frame: `5L + 10 L2 + 1` (66 at 1080p with the defaults: L = 7, L2 = 3).

Roles, **inferred** from the names, the inputs and the disassembly, and not needed to run the
shaders: I1 makes a flow-resolution luma pyramid (Rec.601 weights, read by sampling the copy at
`ResolutionInvScale`); I2..I5 turn each luma level into four RGBA8 feature maps at quarter
resolution, stored per real frame in the 3-slot ring; G estimates the motion field `FlowA` coarse
to fine using both frames' features and `t`; H refines two further fields (`FlowB`, `FlowC`) over
the finest `L2` levels; Compose warps and blends the two real frames with the three fields at `t`.
The UI mask marks static overlay pixels so flow ignores them.

### 4.4 Uniform data per generated frame

Lossless Scaling writes `t` into each level's 48-byte block, releases the old buffer and creates
a new dynamic constant buffer from the block for every level and every generated frame
(`FUN_180021940`, `CreateBuffer` ByteWidth 0x30, Usage 2, BindFlags 4, CPUAccess 0x10000). Only
`Timestamp` changes. On Vulkan, give each (generated frame, level) its own 48-byte slice, for
example one buffer with dynamic offsets (`UNIFORM_BUFFER_DYNAMIC` matches the shaders' Uniform
block), so frames recorded together do not share a block. Pad each slice to
`minUniformBufferOffsetAlignment`.

## 5. Size modes and flow scale

| | Balanced (0) | Performance (1) |
|---|---|---|
| Shader ids | 257..279 | 280..302 |
| Feature channels per level | 4 | 2 |
| `Half` images per level | 4 | 2 |
| `Quarter` images per level | 8 | 5 |
| G/H filter stages | 4 in, 4 out | 2 in, 2 out |
| H6..H10 width | 2 images | 1 image |

Everything else is shared: Copy, LumaPyramid and Compose, the three flow fields, the level counts,
the dispatch sizes and the samplers (`FUN_18001e890` sets nH/nQ at object +0x0c/+0x10;
`FUN_18001fe10` picks the ids).

Flow scale changes only `fw` and `fh`, hence every image except `FrameCopy` and `Output`, the
number of levels `L` (via `flowScale * H > 512`), and `ResolutionInvScale`. It does not change any
shader. Compose always runs at full `W x H`.

## 6. Samplers

Created in `FUN_18001e890` (two through `FUN_1800288f0`, one inline):

| Name | Object field | Filter | Address U/V/W | Border | LOD |
|---|---|---|---|---|---|
| S0 | +0x60 | 0x15 MIN_MAG_MIP_LINEAR | BORDER | (0,0,0,0) | 0..FLT_MAX, bias 0, aniso 1, compare NEVER |
| S1 | +0x68 | 0x15 | BORDER | (1,1,1,1) | same |
| Sc | +0x70 | 0x15 | CLAMP | (0,0,0,0) | same |

Vulkan: linear min/mag, linear mipmap mode, `CLAMP_TO_BORDER` with `FLOAT_TRANSPARENT_BLACK` (S0) or
`FLOAT_OPAQUE_WHITE` (S1), `CLAMP_TO_EDGE` (Sc), unnormalized coordinates off, max LOD
`VK_LOD_CLAMP_NONE`. The images have one mip level, so the mip filter does nothing.

## 7. Lifetimes, barriers and history

Lifetimes:
- Everything in §3 and §6 and the 49 pipelines live from setup until a setting in §1 or the frame
  size changes. Lossless Scaling then releases all (`FUN_180024670`, which also zeroes `s`, `k`,
  `o`) and creates them again (`FUN_18001e800` → `FUN_18001e890` → `FUN_18001fe10`). History does
  not survive a change.
- `FrameCopy[s]` lives across two real frames (newest, then previous). `Feature[k]` lives across
  three (newest, previous, oldest for U0). `UiMask` is rewritten once per real frame and read by
  every generated frame after it.
- `Half`, `Quarter` and the three flow fields are scratch. Each is fully rewritten before it is
  read within one Ingest, UI or Generate call, except that `FlowA/B/C[l+1]` are read at level `l`
  after being written at level `l+1` of the same Generate call.
- `Output[o]` alternates, so the presenter can still read the previous output while the next is
  written. Our generator has no `Output` ring: Compose writes straight into the presenter's image
  for that output, of which Gamescope keeps one per generated frame of the last three real frames.

Barriers: D3D11 orders dispatches implicitly; Lossless Scaling unbinds all slots after each
dispatch (null arrays at object +0x168 for SRVs and +0x78 for UAVs). On Vulkan, record all passes on
one queue in the order above and put, between every two consecutive dispatches, a compute-to-compute
memory dependency (src `SHADER_STORAGE_WRITE`, dst `SHADER_SAMPLED_READ | SHADER_STORAGE_READ |
SHADER_STORAGE_WRITE`). Almost every pass reads the previous pass's output, and the `Quarter` set
is reused as read-after-write and write-after-read within a level, so a finer scheme gains little.
Keep the intermediate images in `GENERAL` layout for their whole life. Additionally:
- The captured frame: transition it to `SHADER_READ_ONLY_OPTIMAL` (or `GENERAL`) and take it from
  its producer's queue before I0. It is not used after I0, so it can be released after I0's
  dispatch completes.
- `FrameCopy`, `Feature`, `UiMask` across real frames, and `Output` to the presenter: a dependency
  from the last compute write to the consumer (Gamescope's composite) and back before the next
  write, plus queue-family ownership transfer if they differ.
- First use of each image: a transition from `UNDEFINED` to `GENERAL`, and clear the history images
  (`FrameCopy`, `Feature`) to zero. D3D11 leaves new textures undefined; **inferred** that they
  read as zero in practice. Our generator should only generate once two real frames are ingested
  (three with UI detection); this is our choice, not observed in Lossless Scaling.

## 8. Version, build and mismatch detection

Observed on the player's install:
- `Lossless.dll` VERSIONINFO FileVersion and ProductVersion 3.2.2.0; `LosslessScaling.dll`
  `AssemblyInformationalVersion` 3.2.2; Steam app 993090 `buildid` 19655272
  (`appmanifest_993090.acf`, default branch).
- `Lossless.dll` SHA-256 `626b196d799606cd4250b7b29e04228692ab70cf56a5d1bbb56d748c8219f0eb`,
  7,521,280 bytes. SHA-256 of resources 254..400 concatenated in id order:
  `89be04f96bba1f798bf911cb8c812ead7951b7f4e0d6c3f09f97783247ef87f6`. Per-resource sizes and
  hash prefixes: `scratch/lsfg-spec-re/out/hashes.txt` (e.g. 254: 1,216 bytes, 255: 6,012, 256:
  3,132).
- Each DXBC resource also carries its own 16-byte checksum at bytes 4..19.

How to detect a mismatched DLL (our design): do not require the hash, since a rebuild with the same
interface is fine. At load, for every id the chosen size mode and precision use (26 SPIR-V modules
of §2.2) require that the resource exists, is SPIR-V with one compute entry point, and that its
interface matches the table in §2.3 exactly: binding names, kinds and slots in set 0 and nothing
else, the `cb` member offsets 0x00..0x1c (0x20 for the UI mask shader), storage formats, local
size, and the `Float16` capability. For SPIR-V read `OpName`, `OpDecorate Binding`/`DescriptorSet`,
`OpMemberDecorate Offset`, `OpTypeImage` and `OpExecutionMode LocalSize`; for DXBC read the RDEF
chunk and `dcl_thread_group`. Any difference: refuse frame generation, show real frames only, and
log once the DLL's FileVersion, its size, the resource id and the first difference. Any difference in the
pass graph itself (§4) cannot be seen from the resources; it shows up as a changed interface or a
changed id set (e.g. a missing 303..400, or a new `FUN_18001fe10`-style table), which is why the
check covers every id.

Not observed, from our card only: Lossless Scaling's `lsfg-vk` Steam beta branch ships the shaders
as `lsfg-vk.dll` with different resource ids (citing Kenji-NX `docs/project-state.md` S010). That
DLL was not examined; this spec covers the default branch's `Lossless.dll` only.

## 9. Gaps

1. The SPIR-V modules (303..400) are assumed to compute the same thing as the DXBC Lossless Scaling
   runs. Interfaces match for all 98 pairs. The FP16 balanced and performance sets produce
   motion-interpolated frames on RADV (card, "Lossless generator"); no frame was compared with
   Lossless Scaling's own output, and the FP32 set has not run.
2. Copy's SPIR-V declares an `Rgba32f` output, unlike D3D11's RGBA8/RGBA16F target (§2.3); our
   copies are `R32G32B32A32_SFLOAT`.
3. `InputOffset` and `AdvancedColorKind` were traced to object fields, not to their UI origin.
   Ours are 0 for an uncropped 8-bit SDR frame. The HDR path (RGBA16F copies, `HdrSupport` = 1) is
   unexplored.
4. Startup with less than two (three) real frames of history is not handled specially in the
   code read (§7).
5. FP16 (`Float16`) against FP32 output differences are unknown; D3D11's min16float lets the driver
   choose, so either is within what Lossless Scaling ships.
6. Checked on one GPU (RX 6700 XT, RADV) against the two real frames only. A closer comparison is
   RenderDoc against Lossless Scaling under Wine/DXVK on the same input.
