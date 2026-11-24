# A libretro core's Vulkan crashes in the macOS bundle

Status: fixed. Packaged app at 06ff9430; Ben 10 (PS2, LRPS2) on the Mac.

Observed: `opensu-retro` dies with SIGSEGV, KERN_INVALID_ADDRESS at a garbage pointer ("possible
pointer authentication failure"), stack MoltenVK `MVKInstance::getEntryPoint` ← MoltenVK
`vkGetInstanceProcAddr` ← the core's `vkGetInstanceProcAddr_libretro` ← its
`create_device2_vulkan` ← `retro::VulkanDevice::negotiate` (`src/retro/vulkan_device.cpp`, the
`create_device2` call) ← `Presenter::open`. MoltenVK had created the VkInstance just before
(1.1.357, `VK_KHR_surface`, `VK_EXT_metal_surface`).

Expected: as from the build tree at the same commit, with `VK_DRIVER_FILES` naming MoltenVK's ICD
manifest: the game runs at 60 fps.

What differs: the bundle carries the Vulkan loader and `libMoltenVK.dylib` side by side in
`Contents/Frameworks` (`tools/package/macos/bundle.py`, `add_moltenvk`), the ICD manifest in
`Resources/vulkan/icd.d`. The crash has a loader-made VkInstance read as an `MVKInstance`, so the
core's call reached MoltenVK's exported `vkGetInstanceProcAddr` instead of the loader's that
`negotiate` hands it.

Next, on the Mac: which library the runner's `vkGetInstanceProcAddr` binds to in the bundle
(`otool -L`, `DYLD_PRINT_BINDINGS=1`) and which Vulkan library the core opens itself
(`DYLD_PRINT_LIBRARIES=1`), against the build tree. The package smoke's fake core has no Vulkan
negotiation, so it cannot catch this; it needs one that negotiates.

## Cause and fix (2026-11-24)

LRPS2 opens Vulkan itself with `dlopen`, trying `<executable>/../Frameworks/libvulkan.dylib`, then
`../Frameworks/libMoltenVK.dylib`, then `libvulkan.dylib` and `libMoltenVK.dylib` on the search path
(the strings in `pcsx2_libretro.dylib`, in that order). The bundle carried the loader only as
`libvulkan.1.4.357.dylib`, so the core's first candidate was missing and it opened MoltenVK
directly. From the build tree there is no `../Frameworks`, and `libvulkan.dylib` is the loader.

`add_moltenvk` (`tools/package/macos/bundle.py`) links `Frameworks/libvulkan.dylib` to the loader,
the same file the runner already loaded, and `sign` skips links. The staged app runs Ben 10 with
the core's device on the M2.
