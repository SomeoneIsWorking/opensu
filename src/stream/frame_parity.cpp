#include "frame_parity.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <utility>

namespace opensu::stream {

namespace {

/// Blocks stay short so a key frame fits the field's 255 symbols and rebuilding stays cheap.
constexpr std::size_t blockParts = 64;
constexpr unsigned fieldPolynomial = 0x11D;
constexpr std::size_t fieldOrder = 255;

struct Field {
    std::array<std::uint8_t, 2 * fieldOrder> exp{};
    std::array<std::uint8_t, fieldOrder + 1> log{};
};

constexpr Field makeField() {
    Field field;
    unsigned value = 1;
    for (std::size_t i = 0; i < fieldOrder; ++i) {
        field.exp.at(i) = static_cast<std::uint8_t>(value);
        field.exp.at(i + fieldOrder) = static_cast<std::uint8_t>(value);
        field.log.at(value) = static_cast<std::uint8_t>(i);
        value <<= 1U;
        if (value > fieldOrder) {
            value ^= fieldPolynomial;
        }
    }
    return field;
}

constexpr Field field = makeField();

std::uint8_t multiply(std::uint8_t left, std::uint8_t right) {
    if (left == 0 || right == 0) {
        return 0;
    }
    return field.exp.at(std::size_t{field.log.at(left)} + field.log.at(right));
}

std::uint8_t inverse(std::uint8_t value) {
    return field.exp.at(fieldOrder - field.log.at(value));
}

/// The Cauchy matrix's entry for parity `row` of a block of `parts`, any square of which inverts.
std::uint8_t coefficient(std::size_t row, std::size_t column, std::size_t parts) {
    return inverse(static_cast<std::uint8_t>((parts + row) ^ column));
}

/// Adds `source` times `factor` into `target`, as far as `source` goes.
void addScaled(std::string& target, std::string_view source, std::uint8_t factor) {
    std::array<std::uint8_t, fieldOrder + 1> times{};
    for (std::size_t value = 0; value < times.size(); ++value) {
        times.at(value) = multiply(static_cast<std::uint8_t>(value), factor);
    }
    const std::size_t length = std::min(target.size(), source.size());
    for (std::size_t i = 0; i < length; ++i) {
        const auto byte = static_cast<std::uint8_t>(source[i]);
        target[i] = static_cast<char>(static_cast<std::uint8_t>(target[i]) ^ times[byte]);
    }
}

using Matrix = std::vector<std::vector<std::uint8_t>>;

/// The inverse of `matrix`, which a Cauchy matrix's square always has.
Matrix inverted(Matrix matrix) {
    const std::size_t size = matrix.size();
    Matrix result(size, std::vector<std::uint8_t>(size, 0));
    for (std::size_t i = 0; i < size; ++i) {
        result[i][i] = 1;
    }
    for (std::size_t column = 0; column < size; ++column) {
        std::size_t pivot = column;
        while (matrix[pivot][column] == 0) {
            ++pivot;
        }
        std::swap(matrix[pivot], matrix[column]);
        std::swap(result[pivot], result[column]);
        const std::uint8_t scale = inverse(matrix[column][column]);
        for (std::size_t i = 0; i < size; ++i) {
            matrix[column][i] = multiply(matrix[column][i], scale);
            result[column][i] = multiply(result[column][i], scale);
        }
        for (std::size_t row = 0; row < size; ++row) {
            const std::uint8_t factor = matrix[row][column];
            if (row == column || factor == 0) {
                continue;
            }
            for (std::size_t i = 0; i < size; ++i) {
                matrix[row][i] ^= multiply(matrix[column][i], factor);
                result[row][i] ^= multiply(result[column][i], factor);
            }
        }
    }
    return result;
}

std::size_t blockParity(std::size_t parts) {
    return static_cast<std::size_t>(std::ceil(static_cast<double>(parts) * parityShare));
}

/// Rebuilds the block of `count` parts from `first`, whose parity starts at `parityAt`.
void restoreBlock(std::map<std::uint16_t, std::string>& parts,
                  const std::map<std::uint16_t, std::string>& parity, std::size_t first,
                  std::size_t count, std::size_t parityAt) {
    std::vector<std::size_t> missing;
    for (std::size_t i = 0; i < count; ++i) {
        if (!parts.contains(static_cast<std::uint16_t>(first + i))) {
            missing.push_back(i);
        }
    }
    std::vector<std::size_t> rows;
    for (std::size_t row = 0; row < blockParity(count) && rows.size() < missing.size(); ++row) {
        if (parity.contains(static_cast<std::uint16_t>(parityAt + row))) {
            rows.push_back(row);
        }
    }
    if (missing.empty() || rows.size() < missing.size()) {
        return;
    }
    Matrix matrix;
    std::vector<std::string> known;
    for (const std::size_t row : rows) {
        std::vector<std::uint8_t> line;
        line.reserve(missing.size());
        for (const std::size_t column : missing) {
            line.push_back(coefficient(row, column, count));
        }
        matrix.push_back(std::move(line));
        std::string left = parity.at(static_cast<std::uint16_t>(parityAt + row));
        for (std::size_t i = 0; i < count; ++i) {
            if (const auto part = parts.find(static_cast<std::uint16_t>(first + i));
                part != parts.end()) {
                addScaled(left, part->second, coefficient(row, i, count));
            }
        }
        known.push_back(std::move(left));
    }
    const Matrix solved = inverted(std::move(matrix));
    for (std::size_t i = 0; i < missing.size(); ++i) {
        std::string part(known.front().size(), '\0');
        for (std::size_t j = 0; j < known.size(); ++j) {
            addScaled(part, known[j], solved[i][j]);
        }
        parts[static_cast<std::uint16_t>(first + missing[i])] = std::move(part);
    }
}

} // namespace

std::size_t parityFor(std::size_t parts) {
    const std::size_t whole = parts / blockParts;
    return whole * blockParity(blockParts) + blockParity(parts % blockParts);
}

std::vector<std::string> parityOf(std::span<const std::string> parts) {
    std::vector<std::string> parity;
    parity.reserve(parityFor(parts.size()));
    for (std::size_t first = 0; first < parts.size(); first += blockParts) {
        const std::span<const std::string> block =
            parts.subspan(first, std::min(blockParts, parts.size() - first));
        const std::size_t length = std::ranges::max(block, {}, [](const std::string& part) {
                                       return part.size();
                                   }).size();
        for (std::size_t row = 0; row < blockParity(block.size()); ++row) {
            std::string packet(length, '\0');
            for (std::size_t i = 0; i < block.size(); ++i) {
                addScaled(packet, block[i], coefficient(row, i, block.size()));
            }
            parity.push_back(std::move(packet));
        }
    }
    return parity;
}

void restoreParts(std::map<std::uint16_t, std::string>& parts,
                  const std::map<std::uint16_t, std::string>& parity, std::size_t expected,
                  std::size_t lastBytes) {
    std::size_t parityAt = 0;
    for (std::size_t first = 0; first < expected; first += blockParts) {
        const std::size_t count = std::min(blockParts, expected - first);
        restoreBlock(parts, parity, first, count, parityAt);
        parityAt += blockParity(count);
    }
    if (const auto last = parts.find(static_cast<std::uint16_t>(expected - 1));
        last != parts.end() && last->second.size() > lastBytes) {
        last->second.resize(lastBytes);
    }
}

} // namespace opensu::stream
