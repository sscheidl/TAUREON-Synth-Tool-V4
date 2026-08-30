#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace taureon::app {

enum class LibrarianCapacityKind { known, unknown, unavailable };

struct LibrarianCapacity {
    LibrarianCapacityKind kind{LibrarianCapacityKind::unavailable};
    std::size_t known_slots{};

    [[nodiscard]] static LibrarianCapacity known(std::size_t slots) noexcept;
    [[nodiscard]] static LibrarianCapacity unknown() noexcept;
    [[nodiscard]] static LibrarianCapacity unavailable() noexcept;
};

struct LibrarianOperationAvailability {
    bool readable{};
    bool writable{};
    bool renameable{};
    bool deletable{};
    std::string unavailable_reason;
};

struct LibrarianSemanticObject {
    std::string stable_id;
    std::string display_name;
    std::string semantic_kind;
};

struct LibrarianSlot {
    std::string stable_id;
    std::string display_label;
    bool read_only{};
    std::optional<LibrarianSemanticObject> semantic_object;
    std::string unavailable_reason;
};

struct LibrarianBank {
    std::string stable_id;
    std::string display_name;
    LibrarianCapacity capacity;
    std::vector<LibrarianSlot> slots;
    LibrarianOperationAvailability operations;
};

struct LibrarianCollection {
    std::string stable_id;
    std::string display_name;
    std::vector<LibrarianBank> banks;
};

struct LibrarianSnapshot {
    bool semantic_support_available{};
    std::string unavailable_reason;
    std::vector<LibrarianCollection> collections;
};

class ILibrarianProvider {
public:
    virtual ~ILibrarianProvider() = default;
    [[nodiscard]] virtual LibrarianSnapshot snapshot() const = 0;
};

[[nodiscard]] bool librarian_snapshot_is_well_formed(const LibrarianSnapshot& snapshot);

} // namespace taureon::app
