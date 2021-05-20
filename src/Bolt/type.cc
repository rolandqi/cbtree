#include "type.h"

// struct Item {
//   const char *pointer_ = nullptr;
//   uint32_t length_ = 0;
//   Item() = default;
//   Item(const char *p, uint32_t sz) : pointer_(p), length_(sz) {}
//   Item(const Item &item) {
//     memmove(this->pointer_, item.pointer_, item.length_);
//     this->length_ = item.length_;
//   }
//   bool operator==(const Item &other) const;
//   bool operator!=(const Item &other) const;
//   bool operator<(const Item &other) const;

//   void reset() {
//     pointer_ = nullptr;
//     length_ = 0;
//   }
//   bool empty() const;

//   // make a Item based on the given data ptr;
//   static Item make_item(const char *p) {
//     if (p == nullptr || *p == 0) {
//       return {};
//     }
//     return { p, strlen(p) };
//   }
//   Item *clone() {
//     Item *ptr = new Item(*this);
//     return ptr;
//   }
// };

// namespace std {
// template <> struct hash<Item> {
//   std::size_t operator()(Item const &item) const noexcept {
//     return hash<size_t>()(reinterpret_cast<size_t>(item.pointer_)) ^
//            hash<size_t>()(item.length_);
//   }
// };
// }

//================
// TODO(roland): some mempool object to reduce copy

// bool Item::operator==(const Item &other) const {
//   if (this == &other) {
//     return true;
//   }
//   if (length_ != other.length_) {
//     return false;
//   }
//   for (size_t i = 0; i < length_; i++) {
//     if (pointer_[i] != other.pointer_[i]) {
//       return false;
//     }
//   }
//   return true;
// }

// bool Item::operator!=(const Item &other) const {
//   return !(this->operator==(other));
// }

// bool Item::operator<(const Item &other) const {
//   size_t i = 0;
//   size_t j = 0;
//   while (i < length_ && j < other.length_) {
//     if (pointer_[i] == other.pointer_[j]) {
//       i++;
//       j++;
//       continue;
//     }
//     return pointer_[i] < other.pointer_[j];
//   }
//   return i == length_ && j != other.length_;
// }

// void Item::reset() {
//   pointer_ = nullptr;
//   length_ = 0;
// }

// bool Item::empty() const { return length_ == 0; }
