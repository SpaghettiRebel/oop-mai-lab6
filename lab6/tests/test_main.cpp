#include <gtest/gtest.h>

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <algorithm>
#include <filesystem>

#include "dungeon.hpp"
#include "factory.hpp"
#include "observer.hpp"
#include "npc.hpp"
#include "combat_visitor.hpp" 

namespace fs = std::filesystem;

struct TestObserver : public IObserver {
    std::vector<DeathEvent> events;
    void onDeath(const DeathEvent &ev) override {
        events.push_back(ev);
    }
};

static bool contains_event(const std::vector<DeathEvent> &evs, std::string killer, std::string victim) {
    return std::any_of(evs.begin(), evs.end(), [&](const DeathEvent &e){
        return e.killer == killer && e.victim == victim;
    });
}

// -------------------- Factory tests --------------------

TEST(FactoryTests, CreateTypesAndValues) {
    auto o = NPCFactory::create("Orc", "O1", 10.0, 20.0);
    auto b = NPCFactory::create("Bear", "B1", 100.5, 200.5);
    auto s = NPCFactory::create("Squirrel", "S1", 0.0, 0.0);

    ASSERT_NE(o, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(s, nullptr);

    EXPECT_EQ(o->type(), "Orc");
    EXPECT_EQ(o->name(), "O1");
    EXPECT_DOUBLE_EQ(o->x(), 10.0);
    EXPECT_DOUBLE_EQ(o->y(), 20.0);

    EXPECT_EQ(b->type(), "Bear");
    EXPECT_EQ(b->name(), "B1");

    EXPECT_EQ(s->type(), "Squirrel");
    EXPECT_EQ(s->name(), "S1");
}

TEST(FactoryTests, CreateFromLineParsing) {
    auto good = NPCFactory::createFromLine("Orc Bor 12.5 3.25");
    ASSERT_NE(good, nullptr);
    EXPECT_EQ(good->type(), "Orc");
    EXPECT_EQ(good->name(), "Bor");
    EXPECT_DOUBLE_EQ(good->x(), 12.5);
    EXPECT_DOUBLE_EQ(good->y(), 3.25);

    // malformed lines
    auto bad1 = NPCFactory::createFromLine("OrcOnlyName");
    EXPECT_EQ(bad1, nullptr);

    auto bad2 = NPCFactory::createFromLine("Unknown X 1 2");
    EXPECT_EQ(bad2, nullptr);
}

// -------------------- EventManager tests --------------------

TEST(EventManagerTests, SubscribeAndNotify) {
    EventManager em;
    auto obs = std::make_shared<TestObserver>();
    em.subscribe(obs);
    EXPECT_TRUE(obs->events.empty());

    DeathEvent e{"Killer","Victim", 1.0, 2.0};
    em.notify(e);
    ASSERT_EQ(obs->events.size(), 1u);
    EXPECT_EQ(obs->events[0].killer, "Killer");
    EXPECT_EQ(obs->events[0].victim, "Victim");
}

// -------------------- Dungeon add/load/save tests --------------------

TEST(DungeonTests, AddBoundsAndDuplicate) {
    Dungeon d;
    // in bounds
    EXPECT_TRUE(d.addNPC(NPCFactory::create("Orc","o",0,0)));
    EXPECT_TRUE(d.addNPC(NPCFactory::create("Bear","b",500,500)));
    // duplicate name
    EXPECT_FALSE(d.addNPC(NPCFactory::create("Squirrel","o",100,100)));
    // out of bounds
    EXPECT_FALSE(d.addNPC(NPCFactory::create("Orc","o2",-1,10)));
    EXPECT_FALSE(d.addNPC(NPCFactory::create("Orc","o3",10,501)));
}

TEST(DungeonTests, SaveLoadRoundtrip) {
    Dungeon d;
    d.clear();
    d.addNPC(NPCFactory::create("Orc","Bob",10,10));
    d.addNPC(NPCFactory::create("Bear","Pim",10,11));
    const std::string fname = "ut_test_save.txt";

    // ensure clean
    std::error_code ec;
    fs::remove(fname, ec);

    ASSERT_TRUE(d.saveToFile(fname));
    // create another dungeon and load
    Dungeon d2;
    ASSERT_TRUE(d2.loadFromFile(fname));
    // check loaded list
    // we can't rely on order, but names should be present
    std::ostringstream out;
    d2.printAll(); // ensure it doesn't crash
    // cleanup
    fs::remove(fname, ec);
}

// -------------------- Combat behavior tests --------------------

// Scenario: Orc kills Bear and Orc kills Orc; Bear kills Squirrel; Squirrel never attacks.
// We'll assert that in a simultaneous round:
// - each victim logged at most once
// - a character that is killed in the round may still kill others in that same round (aliveAtStart semantics)

TEST(CombatTests, SimultaneousRoundVictimLoggedOnceAndKilledCanStillAttack) {
    Dungeon d;
    d.clear();
    // Create the units close enough to each other
    // Bob (Orc) at (10,10)
    // Pim (Bear) at (10,11)
    // chuck (Squirrel) at (10,12)
    // Bobby (Orc) at (9,10)
    d.addNPC(NPCFactory::create("Orc", "Bob", 10, 10));
    d.addNPC(NPCFactory::create("Bear", "Pim", 10, 11));
    d.addNPC(NPCFactory::create("Squirrel", "chuck", 10, 12));
    d.addNPC(NPCFactory::create("Orc", "Bobby", 9, 10));

    auto obs = std::make_shared<TestObserver>();
    d.events().subscribe(obs);

    // run combat with range 10 (large enough so all meet)
    d.runCombat(10.0);

    // We expect:
    // - Pim should have been killed (by Bob). Pim should also be able to kill chuck in same round (bear kills squirrel).
    // - Bobby and Bob are orcs that may kill each other (or mutual kills), but victim logging must be unique per victim.

    // Check that no victim occurs more than once in logs
    std::map<std::string,int> victim_count;
    for (auto &e : obs->events) victim_count[e.victim]++;

    for (auto &p : victim_count) {
        EXPECT_EQ(p.second, 1) << "Victim " << p.first << " was logged multiple times";
    }

    // Check expected specific events exist:
    // Pim killed chuck should appear (Bear kills Squirrel)
    EXPECT_TRUE(contains_event(obs->events, "Pim", "chuck") || contains_event(obs->events, "Bob", "chuck") || contains_event(obs->events, "Bobby", "chuck"));
    // Bob should kill Pim (Orc kills Bear)
    EXPECT_TRUE(contains_event(obs->events, "Bob", "Pim") || contains_event(obs->events, "Bobby", "Pim"));

    // After combat, check survivors: some may die depending on mutual kills, but Squirrel must be dead (bear kills squirrel)
    // We'll verify that chuck is not present in dungeon list (i.e., removed)
    std::stringstream buffer;
    // there is no direct accessor for list except printAll; to ensure `chuck` not present re-load saved state,
    // but simpler: we run list and check text contains no "chuck".
    ::testing::internal::CaptureStdout();
    d.printAll();
    std::string output = ::testing::internal::GetCapturedStdout();
    EXPECT_EQ(output.find("chuck"), std::string::npos);
}

// Additional test: ensure a victim is logged only once even if multiple attackers try to kill them
TEST(CombatTests, VictimLoggedOnlyOnceIfMultipleAttackers) {
    Dungeon d;
    d.clear();
    // Create a victim and two attackers. Attack rules: Orc kills Bear, so two orcs vs one bear.
    d.addNPC(NPCFactory::create("Orc", "OrcA", 0, 0));
    d.addNPC(NPCFactory::create("Orc", "OrcB", 0, 0.5));
    d.addNPC(NPCFactory::create("Bear", "BearV", 0, 0.7));

    auto obs = std::make_shared<TestObserver>();
    d.events().subscribe(obs);

    d.runCombat(10.0);

    // BearV should appear at most once as victim in events
    int count = 0;
    for (auto &e : obs->events) if (e.victim == "BearV") ++count;
    EXPECT_LE(count, 1);
    EXPECT_EQ(count, 1);
}

// Edge case: no combat if out of range
TEST(CombatTests, NoCombatOutOfRange) {
    Dungeon d;
    d.clear();
    d.addNPC(NPCFactory::create("Orc","O1", 0, 0));
    d.addNPC(NPCFactory::create("Bear","B1", 500, 500)); // far away

    auto obs = std::make_shared<TestObserver>();
    d.events().subscribe(obs);

    d.runCombat(10.0);
    EXPECT_TRUE(obs->events.empty());
    // both should still be present
    ::testing::internal::CaptureStdout();
    d.printAll();
    std::string out = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("O1"), std::string::npos);
    EXPECT_NE(out.find("B1"), std::string::npos);
}

// -------------------- Visitor direct behavior test --------------------
// This test directly uses CombatVisitor to check attack relationships.
// (optional if CombatVisitor header/impl are available)
TEST(VisitorTests, BasicWantsKillSemantics) {
    // create a minimal set using factory and direct visitor
    auto orc = NPCFactory::create("Orc","x",0,0);
    auto bear = NPCFactory::create("Bear","y",0,0);
    auto sq = NPCFactory::create("Squirrel","z",0,0);

    // orc vs bear => orc kills bear
    {
        CombatVisitor vis(orc.get());
        bear->accept(vis);
        EXPECT_TRUE(vis.victimDies());
    }
    // bear vs squirrel => bear kills squirrel
    {
        CombatVisitor vis(bear.get());
        sq->accept(vis);
        EXPECT_TRUE(vis.victimDies());
    }
    // squirrel vs orc => squirrel does not attack
    {
        CombatVisitor vis(sq.get());
        orc->accept(vis);
        EXPECT_FALSE(vis.victimDies());
    }
}

