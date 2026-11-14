#include "nerith/ppc/disassembler.h"

#include <iomanip>
#include <sstream>

#include "nerith/ppc/decoder.h"

namespace nerith::ppc {

std::string disassemble_range(const Memory& memory, std::size_t address, std::size_t count) {
    std::ostringstream out;
    for (std::size_t i = 0; i < count; i++) {
        const std::size_t current = address + i * 4;
        const std::uint32_t word = memory.read32(current);
        out << std::hex << std::setfill('0')
            << std::setw(8) << current << "  "
            << std::setw(8) << word << "  "
            << to_text(decode(word)) << "\n";
    }
    return out.str();
}

}  // namespace nerith::ppc
