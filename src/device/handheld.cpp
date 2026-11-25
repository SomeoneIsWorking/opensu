#include "handheld.hpp"

#include <fstream>
#include <string>

namespace opensu::device {

bool rogAlly(const std::filesystem::path& dmi) {
    std::ifstream file{dmi / "product_name"};
    std::string name;
    std::getline(file, name);
    return name.starts_with("ROG Ally");
}

} // namespace opensu::device
