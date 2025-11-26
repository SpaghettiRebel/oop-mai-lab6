#pragma once
#include <vector>
#include <memory>
#include <string>


class NPCBase;
class EventManager;


class Dungeon {
public:
    explicit Dungeon();
    ~Dungeon();

    bool addNPC(std::unique_ptr<NPCBase> npc);
    bool loadFromFile(const std::string &fname);
    bool saveToFile(const std::string &fname) const;
    void clear() noexcept;

    void printAll() const;

    EventManager& events() noexcept;

    void runCombat(double range);

private:
    struct Impl;
    Impl* pimpl_;
};

