# Adaptive frame generation through lsfg-vk

Status: open, research first.

The player: "Another request I have is adaptive LSFG-VK". lsfg-vk is the Vulkan layer that runs
Lossless Scaling's frame generation on Linux; the player has a fork (`SomeoneIsWorking/lsfg-vk`).
It needs the player's own `Lossless.dll` from their Lossless Scaling install on Steam, which is
never copied or shipped.

Adaptive means a target frame rate instead of a fixed multiplier: the generated-frame count follows
the game's real rate so output holds the target (Lossless Scaling's Adaptive mode).

To find out: whether lsfg-vk's current release supports adaptive mode or only fixed multipliers;
how it is configured per game (its config file, environment) and whether openSU can set it per game
through the layered settings; how it sits with Gamescope (the layer runs in the game's Vulkan
instance, inside our nested Gamescope); and what the fork already changes.
