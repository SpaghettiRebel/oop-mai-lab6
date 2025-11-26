// src/npc.cpp
#include "npc.hpp"
#include "combat_visitor.hpp" // <- обязательно: полное определение CombatVisitor
#include "npc_types.hpp"
#include <utility>

// Реализация PIMPL
struct NPCBase::Impl {
    std::string name;
    double x{0.0};
    double y{0.0};
    bool alive{true};

    Impl(std::string n, double xx, double yy) noexcept
        : name(std::move(n)), x(xx), y(yy) {}
};

NPCBase::NPCBase(std::string name, double x, double y) noexcept
    : pimpl(new Impl(std::move(name), x, y)) {}

NPCBase::~NPCBase() {
    delete pimpl;
}

// Геттеры — сигнатуры должны точно совпадать с заголовком
std::string NPCBase::name() const noexcept { return pimpl->name; }
double NPCBase::x() const noexcept { return pimpl->x; }
double NPCBase::y() const noexcept { return pimpl->y; }
bool NPCBase::alive() const noexcept { return pimpl->alive; }
void NPCBase::markDead() noexcept { pimpl->alive = false; }

// Реализации типов NPC — возвращают строковый тип и вызывают visitor.
// Тут CombatVisitor полностью известен благодаря include выше.

std::string Orc::type() const noexcept { return "Orc"; }
void Orc::accept(CombatVisitor &v) { v.visit(*this); }

std::string Bear::type() const noexcept { return "Bear"; }
void Bear::accept(CombatVisitor &v) { v.visit(*this); }

std::string Squirrel::type() const noexcept { return "Squirrel"; }
void Squirrel::accept(CombatVisitor &v) { v.visit(*this); }
