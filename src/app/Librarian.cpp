#include "app/Librarian.hpp"

#include <unordered_set>

namespace taureon::app {

LibrarianCapacity LibrarianCapacity::known(const std::size_t slots) noexcept {
    return {LibrarianCapacityKind::known, slots};
}

LibrarianCapacity LibrarianCapacity::unknown() noexcept {
    return {LibrarianCapacityKind::unknown, 0};
}

LibrarianCapacity LibrarianCapacity::unavailable() noexcept {
    return {LibrarianCapacityKind::unavailable, 0};
}

bool librarian_snapshot_is_well_formed(const LibrarianSnapshot& snapshot) {
    std::unordered_set<std::string> collection_ids;
    for (const auto& collection : snapshot.collections) {
        if (collection.stable_id.empty() || !collection_ids.insert(collection.stable_id).second) {
            return false;
        }
        std::unordered_set<std::string> bank_ids;
        for (const auto& bank : collection.banks) {
            if (bank.stable_id.empty() || !bank_ids.insert(bank.stable_id).second) return false;
            if (bank.capacity.kind == LibrarianCapacityKind::known &&
                bank.slots.size() > bank.capacity.known_slots) return false;
            std::unordered_set<std::string> slot_ids;
            for (const auto& slot : bank.slots) {
                if (slot.stable_id.empty() || !slot_ids.insert(slot.stable_id).second) return false;
                if (slot.semantic_object && slot.semantic_object->stable_id.empty()) return false;
            }
        }
    }
    return snapshot.semantic_support_available || snapshot.collections.empty();
}

} // namespace taureon::app
