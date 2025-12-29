#pragma once

// Trida zajistujici, ze je objekt nekopirovatelny a nepresouvatelny
class NonCopyableNonMovable {
public:
    NonCopyableNonMovable() = default;

    NonCopyableNonMovable(const NonCopyableNonMovable&) = delete;
    NonCopyableNonMovable& operator=(const NonCopyableNonMovable&) = delete;

    NonCopyableNonMovable(NonCopyableNonMovable&&) = delete;
    NonCopyableNonMovable& operator=(NonCopyableNonMovable&&) = delete;
};
