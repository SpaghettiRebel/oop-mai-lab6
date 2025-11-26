#include "factory.hpp"
#include "npc.hpp"
#include "npc_types.hpp"
#include <sstream>

std::unique_ptr<NPCBase> NPCFactory::create(const std::string &type, const std::string &name, double x, double y) {
    if (type == "Orc") return std::make_unique<Orc>(name, x, y);
    if (type == "Bear") return std::make_unique<Bear>(name, x, y);
    if (type == "Squirrel") return std::make_unique<Squirrel>(name, x, y);
    return nullptr;
}

std::unique_ptr<NPCBase> NPCFactory::createFromLine(const std::string &line) {
    std::istringstream iss(line);
    std::string type, name;
    double x, y;
    if (!(iss >> type >> name >> x >> y)) return nullptr;
    return create(type, name, x, y);
}
