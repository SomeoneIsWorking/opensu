# The one refusal for a missing build dependency: the maintainer's route (tools.check builds in its
# podman build image, which has every one) and the distribution packages for a build by hand.
#
# opensu_missing_package(<why> FEDORA <packages> DEBIAN <packages> [ARCH <packages>]
#                        [OTHERWISE <alternative>])
function(opensu_missing_package why)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "FEDORA;DEBIAN;ARCH;OTHERWISE" "")
    string(CONCAT by_hand "`sudo dnf install ${arg_FEDORA}` (Fedora), "
        "`sudo apt install ${arg_DEBIAN}` (Debian/Ubuntu)")
    if(arg_ARCH)
        string(APPEND by_hand ", `sudo pacman -S ${arg_ARCH}` (Arch)")
    endif()
    if(arg_OTHERWISE)
        string(APPEND by_hand ", or ${arg_OTHERWISE}")
    endif()
    message(FATAL_ERROR "${why}. Maintainers build with `uv run --frozen python -m tools.check "
        "build`, which runs in a podman build image that has it (docs/guide/building.md). "
        "Building by hand, install it: ${by_hand}.")
endfunction()
