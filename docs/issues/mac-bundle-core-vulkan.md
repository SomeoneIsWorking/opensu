# A libretro core's Vulkan crashes in the macOS bundle

Status: open. Packaged app at 06ff9430; Ben 10 (PS2, LRPS2) on the Mac.

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
