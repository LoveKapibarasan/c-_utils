#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <ctime>
#include <cstdlib>
#include <cmath>
#include <optional>

namespace fs = std::filesystem;

struct Entry {
    std::string date; // in format YYYY-MM-DD
    int level;
    std::string path;
};

const std::string dataFile = "entries.txt";

std::vector<Entry> loadEntries() {
    std::vector<Entry> entries;
    std::ifstream file(dataFile);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string date, levelStr, path;
        if (!std::getline(ss, date, ',')) continue;
        if (!std::getline(ss, levelStr, ',')) continue;
        if (!std::getline(ss, path)) continue;
        try {
            int level = std::stoi(levelStr);
            entries.push_back({date, level, path});
        } catch (...) {
            continue;
        }
    }
    return entries;
}

void saveEntries(const std::vector<Entry>& entries) {
    std::ofstream file(dataFile);
    for (const auto& entry : entries) {
        file << entry.date << "," << entry.level << "," << entry.path << "\n";
    }
}

std::string getTodayDate() {
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    char buf[11];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", now);
    return std::string(buf);
}

int daysBetween(const std::string& date1, const std::string& date2) {
    std::tm tm1 = {}, tm2 = {};
    std::istringstream ss1(date1);
    std::istringstream ss2(date2);
    ss1 >> std::get_time(&tm1, "%Y-%m-%d");
    ss2 >> std::get_time(&tm2, "%Y-%m-%d");
    std::time_t time1 = std::mktime(&tm1);
    std::time_t time2 = std::mktime(&tm2);
    return std::difftime(time2, time1) / (60 * 60 * 24);
}

void createEntry(const std::string& date, int level, const std::string& path) {
    auto entries = loadEntries();
    std::vector<std::string> targets;
    if (fs::is_directory(path)) {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (fs::is_regular_file(entry.path())) {
                targets.push_back(entry.path().string());
            }
        }
    } else {
        targets.push_back(path);
    }
    for (const auto& tpath : targets) {
        entries.push_back({date, level, tpath});
    }
    saveEntries(entries);
    std::cout << "Entry/entries created.\n";
}

void updateLevel(const std::string& path, std::optional<int> newLevelOpt) {
    auto entries = loadEntries();
    bool found = false;
    std::vector<std::string> targets;

    if (fs::is_directory(path)) {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (fs::is_regular_file(entry.path())) {
                targets.push_back(entry.path().string());
            }
        }
    } else {
        targets.push_back(path);
    }

    for (const auto& tpath : targets) {
        for (auto& e : entries) {
            if (e.path == tpath) {
                if (newLevelOpt.has_value()) {
                    e.level = newLevelOpt.value();
                } else {
                    e.level += 1;
                }
                found = true;
            }
        }
    }
    if (found) {
        saveEntries(entries);
        std::cout << "Level(s) updated.\n";
    } else {
        std::cerr << "Path(s) not found in entries.\n";
    }
}

void openInExplorer(const std::string& path) {
    if (fs::exists(path)) {
        std::string command = "xdg-open \"" + path + "\"";
        std::system(command.c_str());
    } else {
        std::cerr << "Path does not exist.\n";
    }
}

void checkAndOpen() {
    auto entries = loadEntries();
    std::string today = getTodayDate();
    std::vector<Entry> newEntries;
    for (const auto& e : entries) {
        int limit = std::pow(2, e.level);
        if (daysBetween(e.date, today) >= limit) {
            fs::path p(e.path);
            fs::path dir = p.parent_path();
            std::cout << "Opening folder: " << dir << "\n";
            openInExplorer(dir.string());
        }
        if (e.level <= 3) {
            newEntries.push_back(e);
        }
    }
    if (newEntries.size() != entries.size()) {
        saveEntries(newEntries);
        std::cout << "Entries with level > 3 were removed.\n";
    }
}

void printUsage() {
    std::cout << "Usage:\n"
              << "  create YYYY-MM-DD LEVEL /path/to/file_or_folder\n"
              << "  create LEVEL /path/to/file_or_folder   (date = today)\n"
              << "  create /path/to/file_or_folder         (date = today, level = 0)\n"
              << "  update /path/to/file_or_folder NEW_LEVEL\n"
              << "  update /path/to/file_or_folder         (increment level by 1)\n"
              << "  open /path/to/file\n"
              << "  check\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string cmd = argv[1];
    if (cmd == "create" && (argc == 5 || argc == 4 || argc == 3)) {
        std::string date, path;
        int level;
        if (argc == 5) {
            date = argv[2];
            level = std::stoi(argv[3]);
            path = argv[4];
        } else if (argc == 4) {
            date = getTodayDate();
            level = std::stoi(argv[2]);
            path = argv[3];
        } else { // argc == 3
            date = getTodayDate();
            level = 0;
            path = argv[2];
        }
        createEntry(date, level, path);
    } else if (cmd == "update" && (argc == 3 || argc == 4)) {
        std::string path = argv[2];
        if (argc == 4) {
            int newLevel = std::stoi(argv[3]);
            updateLevel(path, newLevel);
        } else {
            updateLevel(path, std::nullopt);
        }
    } else if (cmd == "open" && argc == 3) {
        openInExplorer(argv[2]);
    } else if (cmd == "check") {
        checkAndOpen();
    } else {
        printUsage();
        return 1;
    }

    return 0;
}
