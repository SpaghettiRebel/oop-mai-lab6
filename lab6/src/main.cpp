#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <cctype>
#include <vector>

#include "dungeon.hpp"
#include "factory.hpp"
#include "observer.hpp"
#include "npc.hpp"


struct ConsoleLogger : public IObserver {
    void onDeath(const DeathEvent &ev) override {
        std::cout << "[LOG] " << ev.killer << " убил " << ev.victim << " в точке (" << ev.x << "," << ev.y << ")\n";
    }
};


struct FileLogger : public IObserver {
    explicit FileLogger(std::string filename = "log.txt") : fn(std::move(filename)) {}
    void onDeath(const DeathEvent &ev) override {
        std::ofstream f(fn, std::ios::app);
        if (!f) {
            return;
        }

        f << ev.killer << " убил " << ev.victim << " в точке (" << ev.x << "," << ev.y << ")\n";
    }
private:
    std::string fn;
};

static void print_help() {
    std::cout <<
    "Команды редактора:\n"
    "  help                         - показать это окно справки\n"
    "  add                          - добавить NPC (интерактивно)\n"
    "  add <класс> <имя> <x> <y>    - быстрое добавление в одну строку (например add Orc Bob 10 20)\n"
    "  list                         - вывод всех NPC\n"
    "  save <имя файла>             - сохранение всех NPC в файл\n"
    "  load <имя файла>             - загрузка NPC из файла (все расставленные юниты будут удалены)\n"
    "  combat <дальность>           - запуск боя с указанной дальностью атаки для всех NPC (double)\n"
    "  clear                        - удалить всех NPC\n"
    "  exit                         - закрыть\n";
}

static std::vector<std::string> split_ws(const std::string &s) {
    std::istringstream iss(s);
    std::vector<std::string> t;
    std::string w;
    while (iss >> w) t.push_back(w);
    return t;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Dungeon d;
    d.events().subscribe(std::make_shared<ConsoleLogger>());
    d.events().subscribe(std::make_shared<FileLogger>("log.txt"));

    std::cout << "Balagur Fate 3 — редактор подземелий\n";
    print_help();

    auto trim = [](std::string s) -> std::string {
        auto l = s.find_first_not_of(" \t\r\n");
        if (l == std::string::npos) return {};
        auto r = s.find_last_not_of(" \t\r\n");
        return s.substr(l, r - l + 1);
    };
    auto to_lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };

    auto read_line = [&](const std::string &prompt)->std::string {
        std::string s;
        while (true) {
            std::cout << prompt;
            if (!std::getline(std::cin, s)) return std::string(); // EOF signal -> cancel
            s = trim(s);
            if (s.empty()) {
                std::cout << "Пустой ввод — попытайтесь ещё раз или введите 'cancel'/'q' для отмены\n";
                continue;
            }
            if (s == "cancel" || s == "q") return std::string();
            return s;
        }
    };

    auto read_double = [&](const std::string &prompt, double lo, double hi, double &out)->bool {
        while (true) {
            std::string s = read_line(prompt);
            if (s.empty()) return false; // canceled or EOF
            std::istringstream iss(s);
            double v; char extra;
            if (!(iss >> v) || (iss >> extra)) {
                std::cout << "Некорректное число — попытайтесь ещё раз или введите 'cancel'/'q' для отмены\n";
                continue;
            }
            if (v < lo || v > hi) {
                std::cout << "Число не входит в отрезок [" << lo << "," << hi << "]. Попытайтесь ещё раз или введите 'cancel'/'q' для отмены\n";
                continue;
            }
            out = v;
            return true;
        }
    };

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        line = trim(line);
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd == "help") {
            print_help();

        } else if (cmd == "add") {
            // First check if the user provided inline args: "add Orc Bob 1 2"
            std::vector<std::string> rest;
            std::string token;
            while (iss >> token) rest.push_back(token);

            std::string type, name;
            double x = 0.0, y = 0.0;
            bool canceled = false;
            bool done = false;

            if (rest.size() >= 4) {
                // try interpret as: type name x y
                type = rest[0]; name = rest[1];
                std::istringstream sx(rest[2]), sy(rest[3]);
                if (!(sx >> x) || !(sy >> y) || x < 0 || x > 500 || y < 0 || y > 500) {
                    std::cout << "Invalid inline parameters. Falling back to interactive mode.\n";
                } else {
                    // normalize type
                    std::string low = to_lower(type);
                    if (low == "orc") type = "Orc";
                    else if (low == "bear") type = "Bear";
                    else if (low == "squirrel") type = "Squirrel";
                    else { std::cout << "Unknown type in inline args. Falling back to interactive.\n"; }
                    if (type == "Orc" || type == "Bear" || type == "Squirrel") {
                        auto npc = NPCFactory::create(type, name, x, y);
                        if (npc && d.addNPC(std::move(npc))) {
                            std::cout << "Added " << type << " '" << name << "' at (" << x << "," << y << ")\n";
                            done = true;
                        } else {
                            std::cout << "Failed to add NPC (duplicate name or coords). Enter interactive mode.\n";
                        }
                    }
                }
            }

            if (done) continue;

            std::string tline = read_line("Класс (Orc|Bear|Squirrel) (или 'cancel'/'q'): ");
            if (tline.empty()) { std::cout << "Отмена добавления\n"; continue; }

            auto toks = split_ws(tline);
            if (toks.size() >= 4) {
                type = toks[0]; name = toks[1];
                std::istringstream sx(toks[2]), sy(toks[3]);
                if (!(sx >> x) || !(sy >> y)) { std::cout << "Invalid numbers in single-line input. Switching to step mode.\n"; }
                else {
                    std::string low = to_lower(type);
                    if (low == "orc") type = "Orc";
                    else if (low == "bear") type = "Bear";
                    else if (low == "squirrel") type = "Squirrel";
                    else { std::cout << "Неизвестный класс NPC. Отмена добавления\n"; continue; }
                    auto npc = NPCFactory::create(type, name, x, y);
                    if (npc && d.addNPC(std::move(npc))) {
                        std::cout << "Добавлен " << type << " '" << name << "' в точке (" << x << "," << y << ")\n";
                        continue;
                    } else {
                        std::cout << "Не удалось добавить NPC (duplicate name or coords). Aborting add.\n";
                        continue;
                    }
                }
            }

            type = tline;
            std::string low = to_lower(type);
            if (low == "orc") type = "Orc";
            else if (low == "bear") type = "Bear";
            else if (low == "squirrel") type = "Squirrel";
            else { std::cout << "Неизвестный класс NPC. Отмена добавления\n"; continue; }

            std::string name_in = read_line("Имя (уникальное) (или 'cancel'/'q'): ");
            if (name_in.empty()) { 
                std::cout << "Добавление отменено\n"; 
                continue; 
            }
            name = name_in;

            if (!read_double("x (0..500) (or 'cancel'/'q'): ", 0.0, 500.0, x)) { 
                std::cout << "Добавление отменено\n"; 
                continue; 
            }
            if (!read_double("y (0..500) (or 'cancel'/'q'): ", 0.0, 500.0, y)) { 
                std::cout << "Добавление отменено\n"; 
                continue; 
            }

            auto npc = NPCFactory::create(type, name, x, y);
            if (!npc) { 
                std::cout << "Ошибка создания. Добавление отменено\n"; 
                continue; 
            }
            if (!d.addNPC(std::move(npc))) {
                std::cout << "Ошибка добавления NPC: введено повторяющееся имя или недопустимые координаты\n";
            } else {
                std::cout << "Добавлен NPC " << type << " '" << name << "' в точку (" << x << "," << y << ")\n";
            }

        } else if (cmd == "list") {
            d.printAll();

        } else if (cmd == "save") {
            std::string fname;
            if (!(iss >> fname)) {
                std::cout << "Использование: save <имя файла>\n";
                continue;
            }
            if (d.saveToFile(fname)) std::cout << "Сохранено в '" << fname << "'\n";
            else std::cout << "Не удалось сохранить в файл '" << fname << "'\n";

        } else if (cmd == "load") {
            std::string fname;
            if (!(iss >> fname)) {
                std::cout << "Использование: load <имя файла>\n";
                continue;
            }
            if (d.loadFromFile(fname)) std::cout << "Загрузка из файла '" << fname << "'\n";
            else std::cout << "Не удалось загрузить файл '" << fname << "'\n";

        } else if (cmd == "combat") {
            double R;
            if (!(iss >> R)) {
                std::cout << "Использование: combat <дальность>\n";
                continue;
            }
            if (R < 0.0) { std::cout << "Дальность атаки не может быть отрицательной\n"; continue; }
            std::cout << "Запуск сражения с дальностью атаки = " << R << " ...\n";
            d.runCombat(R);
            std::cout << "Сражение завершено\n";
            d.printAll();

        } else if (cmd == "clear") {
            d.clear();
            std::cout << "Все NPC удалены\n";

        } else if (cmd == "exit" || cmd == "quit") {
            std::cout << "Игра окончена\n";
            break;

        } else {
            std::cout << "Неизвестная команда. Введите 'help' для вывода справки.\n";
        }
    }

    return 0;
}
