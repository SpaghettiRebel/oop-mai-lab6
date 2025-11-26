#pragma once
#include "npc.hpp"


class Orc final : public NPCBase {
public:
    using NPCBase::NPCBase;
    std::string type() const noexcept override;
    void accept(CombatVisitor &v) override;
};


class Bear final : public NPCBase {
public:
    using NPCBase::NPCBase;
    std::string type() const noexcept override;
    void accept(CombatVisitor &v) override;
};


class Squirrel final : public NPCBase {
public:
    using NPCBase::NPCBase;
    std::string type() const noexcept override;
    void accept(CombatVisitor &v) override;
};
