#include "dungeon.hpp"
#include "factory.hpp"
#include "observer.hpp"
#include "combat_visitor.hpp"
#include "npc.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>

struct Dungeon::Impl {
    std::vector<std::unique_ptr<NPCBase>> npcs;
    EventManager events;
};

Dungeon::Dungeon() : pimpl_(new Impl()) {}
Dungeon::~Dungeon() { delete pimpl_; }

bool Dungeon::addNPC(std::unique_ptr<NPCBase> npc) {
    if (!npc) return false;
    if (npc->x() < 0 || npc->x() > 500 || npc->y() < 0 || npc->y() > 500) return false;
    auto it = std::find_if(pimpl_->npcs.begin(), pimpl_->npcs.end(),
                           [&](auto &p){ return p->name() == npc->name(); });
    if (it != pimpl_->npcs.end()) return false;
    pimpl_->npcs.push_back(std::move(npc));
    return true;
}

bool Dungeon::loadFromFile(const std::string &fname) {
    std::ifstream f(fname);
    if (!f) return false;
    std::string line;
    std::vector<std::unique_ptr<NPCBase>> newlist;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto npc = NPCFactory::createFromLine(line);
        if (!npc) continue;
        if (npc->x() < 0 || npc->x() > 500 || npc->y() < 0 || npc->y() > 500) continue;
        bool dup = std::any_of(newlist.begin(), newlist.end(), [&](auto &p){ return p->name() == npc->name(); });
        if (dup) continue;
        newlist.push_back(std::move(npc));
    }
    pimpl_->npcs = std::move(newlist);
    return true;
}

bool Dungeon::saveToFile(const std::string &fname) const {
    std::ofstream f(fname);
    if (!f) return false;
    for (auto &p : pimpl_->npcs) {
        f << p->type() << " " << p->name() << " " << p->x() << " " << p->y() << "\n";
    }
    return true;
}

void Dungeon::clear() noexcept {
    pimpl_->npcs.clear();
}

void Dungeon::printAll() const {
    std::cout << "--- NPCs (" << pimpl_->npcs.size() << ") ---\n";
    for (auto &p : pimpl_->npcs) {
        std::cout << p->type() << " " << p->name() << " (" << p->x() << "," << p->y() << ")";
        if (!p->alive()) std::cout << " [dead]";
        std::cout << "\n";
    }
}

EventManager& Dungeon::events() noexcept {
    return pimpl_->events;
}

void Dungeon::runCombat(double range) {
    if (range < 0.0) return;
    const double r2 = range * range;

    auto &npcs = pimpl_->npcs;
    size_t n = npcs.size();
    if (n < 2) return;

    // snapshot who is alive at start of this combat pass
    std::vector<char> aliveAtStart(n, 0);
    for (size_t i = 0; i < n; ++i) aliveAtStart[i] = npcs[i]->alive() ? 1 : 0;

    // willDie marks victims (may be set multiple times but we only log once per victim)
    std::vector<char> willDie(n, 0);

    // store first killer for each victim (empty => not yet killed/logged)
    std::vector<std::string> killerOf(n);

    // events in this round (we will notify AFTER applying deaths)
    struct Ev { std::string killer; std::string victim; double x; double y; };
    std::vector<Ev> events;

    // evaluate all unordered pairs (i<j) using aliveAtStart snapshot
    for (size_t i = 0; i < n; ++i) {
        if (!aliveAtStart[i]) continue; // dead at start -> doesn't participate
        for (size_t j = i + 1; j < n; ++j) {
            if (!aliveAtStart[j]) continue;
            // distance check
            double dx = npcs[i]->x() - npcs[j]->x();
            double dy = npcs[i]->y() - npcs[j]->y();
            if (dx*dx + dy*dy > r2) continue;

            // i attacks j
            CombatVisitor cv_i(npcs[i].get());
            npcs[j]->accept(cv_i);
            if (cv_i.victimDies()) {
                // record willDie even if already marked; but only first killer is logged
                willDie[j] = 1;
                if (killerOf[j].empty()) {
                    killerOf[j] = npcs[i]->name();
                    events.push_back({npcs[i]->name(), npcs[j]->name(), npcs[j]->x(), npcs[j]->y()});
                }
            }
            if (cv_i.attackerDies()) {
                // j would kill i (in reaction)
                willDie[i] = 1;
                if (killerOf[i].empty()) {
                    killerOf[i] = npcs[j]->name();
                    events.push_back({npcs[j]->name(), npcs[i]->name(), npcs[i]->x(), npcs[i]->y()});
                }
            }

            // Note: we evaluate both directions implicitly because when later i/j
            // swap roles (we don't evaluate j attacking i separately here). If you want
            // both A vs B and B vs A to be considered as separate attacker roles, keep this loop
            // because cv_i.attackerDies() covers mutual kills as determined by visitor.
        }
    }

    // apply deaths (mark dead) — but do it once per victim
    for (size_t idx = 0; idx < n; ++idx) {
        if (willDie[idx] && npcs[idx]->alive()) {
            npcs[idx]->markDead();
        }
    }

    // notify events (each victim logged only once, as collected)
    for (auto &ev : events) {
        pimpl_->events.notify({ev.killer, ev.victim, ev.x, ev.y});
    }

    // remove dead NPCs
    npcs.erase(std::remove_if(npcs.begin(), npcs.end(),
                [](const std::unique_ptr<NPCBase> &p){ return !p->alive(); }),
               npcs.end());
}
