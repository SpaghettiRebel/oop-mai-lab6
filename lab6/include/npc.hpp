#pragma once
#include <string>

class CombatVisitor;

class NPCBase {
public:
    NPCBase(std::string name, double x, double y) noexcept;
    virtual ~NPCBase();

    std::string name() const noexcept;
    double x() const noexcept;
    double y() const noexcept;
    bool alive() const noexcept;

    void markDead() noexcept;

    virtual std::string type() const noexcept = 0;

    virtual void accept(CombatVisitor &v) = 0;

private:
    struct Impl;
    Impl* pimpl;
};
