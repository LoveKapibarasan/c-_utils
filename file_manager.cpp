#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <ctime>
#include <cstdlib>
#include <cmath>

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
        std::stringstream ss(line);
        std::string date, levelStr, path;
        std::getline(ss, date, ',');
        std::getline(ss, levelStr, ',');
        std::getline(ss, path);
        entries.push_back({date, std::stoi(levelStr), path});
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
    entries.push_back({date, level, path});
    saveEntries(entries);
    std::cout << "Entry created.\n";
}

void updateLevel(const std::string& path) {
    auto entries = loadEntries();
    bool found = false;
    for (auto& e : entries) {
        if (e.path == path) {
            e.level = 0;
            found = true;
            break;
        }
    }
    if (found) {
        saveEntries(entries);
        std::cout << "Level updated.\n";
    } else {
        std::cerr << "Path not found.\n";
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
    for (const auto& e : entries) {
        int limit = std::pow(2, e.level);
        if (daysBetween(e.date, today) >= limit) {
            std::cout << "Opening: " << e.path << "\n";
            openInExplorer(e.path);
        }
    }
}

void printUsage() {
    std::cout << "Usage:\n"
              << "  create YYYY-MM-DD LEVEL /path/to/file\n"
              << "  update /path/to/file NEW_LEVEL\n"
              << "  open /path/to/file\n"
              << "  check\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string cmd = argv[1];
    if (cmd == "create" && argc == 5) {
        createEntry(argv[2], std::stoi(argv[3]), argv[4]);
    } else if (cmd == "update" && argc == 4) {
        updateLevel(argv[2], std::stoi(argv[3]));
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
