#include "nerith/memory/memory.h"

#include <stdexcept>

namespace nerith {

Memory::Memory(std::size_t size) : data_(size, 0) {}

std::uint8_t Memory::read8(std::size_t address) const {
    return data_.at(address);
}

std::uint32_t Memory::read32(std::size_t address) const {
    const std::uint32_t b0 = read8(address);       // octet de poids fort
    const std::uint32_t b1 = read8(address + 1);
    const std::uint32_t b2 = read8(address + 2);
    const std::uint32_t b3 = read8(address + 3);   // octet de poids faible
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

void Memory::write8(std::size_t address, std::uint8_t value) {
    data_.at(address) = value;
}

void Memory::write32(std::size_t address, std::uint32_t value) {
    // On verifie d'abord que les 4 octets tiennent dans la memoire,
    // pour ne jamais ecrire un mot a moitie.
    if (address > data_.size() || data_.size() - address < 4) {
        throw std::out_of_range("write32 : adresse hors limites");
    }
    write8(address,     static_cast<std::uint8_t>((value >> 24) & 0xFF));
    write8(address + 1, static_cast<std::uint8_t>((value >> 16) & 0xFF));
    write8(address + 2, static_cast<std::uint8_t>((value >> 8) & 0xFF));
    write8(address + 3, static_cast<std::uint8_t>(value & 0xFF));
}

void Memory::load(std::size_t address, const std::vector<std::uint8_t>& bytes) {
    if (address > data_.size() || data_.size() - address < bytes.size()) {
        throw std::out_of_range("load : le programme ne tient pas dans la memoire");
    }
    for (std::size_t i = 0; i < bytes.size(); i++) {
        data_[address + i] = bytes[i];   // verifie plus haut : pas besoin de .at()
    }
}

std::size_t Memory::size() const {
    return data_.size();
}

}  // namespace nerith
