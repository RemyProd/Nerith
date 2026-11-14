#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nerith {

// Memoire de l'emulateur : un tableau d'octets de taille fixe.
// Tous les acces sont verifies : une adresse invalide leve std::out_of_range.
class Memory {
public:
    explicit Memory(std::size_t size);

    std::uint8_t read8(std::size_t address) const;
    std::uint32_t read32(std::size_t address) const;   // big-endian, comme la Xbox 360

    void write8(std::size_t address, std::uint8_t value);
    void write32(std::size_t address, std::uint32_t value);   // big-endian

    // Copie des octets a partir d'une adresse (pour charger un programme).
    // Tout ou rien : si ca ne tient pas, rien n'est copie.
    void load(std::size_t address, const std::vector<std::uint8_t>& bytes);

    std::size_t size() const;

private:
    std::vector<std::uint8_t> data_;
};

}  // namespace nerith
