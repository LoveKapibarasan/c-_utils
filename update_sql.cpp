#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

// Pattern definition for evaluation lines
const std::regex EVAL_PATTERN(R"(^\*\*解析\s*0\s*([△○])?\s*(?:候補1)?\s*時間.*?評価値\s*([+-]?\d+|[+-]?詰(?:\s*\d+)?))");

// Forward declarations
json loadSettings();
std::optional<json> findSetting(const std::string& filename, const json& settings);
std::string readFileWithEncoding(const std::string& filepath);
std::tuple<std::string, bool, std::optional<bool>, int, int> extractGameInformationFor24(
    const std::string& content, const std::string& player);
std::tuple<std::string, bool, std::optional<bool>, std::string, std::string, std::string, std::string> 
    extractGameInformationForWars(const std::string& content, const std::string& player);
std::optional<bool> gameResult(const std::string& content);
int sanitizeEval(const std::string& cp);
std::pair<double, double> processLines(const std::string& content);

// Load settings from JSON file
json loadSettings() {
    std::ifstream file("setting.json");
    if (!file.is_open()) {
        throw std::runtime_error("Could not open setting.json");
    }
    json settings;
    file >> settings;
    return settings;
}

// Find setting that matches a filename
std::optional<json> findSetting(const std::string& filename, const json& settings) {
    for (const auto& entry : settings) {
        std::regex pattern(entry["pattern"].get<std::string>());
        if (std::regex_match(filename, pattern)) {
            return entry;
        }
    }
    return std::nullopt;
}

// Read file with multiple encoding attempts
std::string readFileWithEncoding(const std::string& filepath) {
    std::vector<std::string> encodings = {"utf-8", "shift_jis", "cp932"};
    
    for (const auto& encoding : encodings) {
        try {
            std::ifstream file(filepath);
            if (file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                return content;
            }
        } catch (...) {
            continue;
        }
    }
    throw std::runtime_error("Could not decode " + filepath + " with tried encodings.");
}

// Main update SQL function
void updateSql(const std::string& inputFilePath) {
    json settings = loadSettings();
    std::string filename = fs::path(inputFilePath).filename().string();
    
    auto matchedSetting = findSetting(filename, settings);
    if (!matchedSetting) {
        throw std::runtime_error("Setting for file not found in setting.json");
    }

    std::string content = readFileWithEncoding(inputFilePath);
    
    std::string newLine;
    
    if ((*matchedSetting)["name"] == "24") {
        std::cout << "mode is 24 at " << inputFilePath << std::endl;
        auto [mode, teban, result, senteRating, goteRating] = 
            extractGameInformationFor24(content, (*matchedSetting)["player"]);
        auto [ave0, ave1] = processLines(content);
        
        std::string tebanStr = teban ? "true" : "false";
        std::string resultStr = result.has_value() ? 
            (result.value() ? "true" : "false") : "NULL";
        
        newLine = "('" + mode + "', " + tebanStr + ", " + resultStr + ", " +
                  std::to_string(static_cast<int>(std::round(ave0))) + ", " +
                  std::to_string(static_cast<int>(std::round(ave1))) + ", " +
                  std::to_string(senteRating) + ", " + std::to_string(goteRating) + ", NULL)\n";
    }
    else if ((*matchedSetting)["name"] == "wars") {
        std::cout << "mode is wars at " << inputFilePath << std::endl;
        auto [mode, teban, result, senpou, gopou, skakoi, gkakoi] = 
            extractGameInformationForWars(content, (*matchedSetting)["player"]);
        auto [ave0, ave1] = processLines(content);
        
        std::string tebanStr = teban ? "true" : "false";
        std::string resultStr = result.has_value() ? 
            (result.value() ? "true" : "false") : "NULL";
        
        newLine = "('" + mode + "', " + tebanStr + ", " + resultStr + ", " +
                  std::to_string(static_cast<int>(std::round(ave0))) + ", " +
                  std::to_string(static_cast<int>(std::round(ave1))) + ", " +
                  "'" + senpou + "', '" + gopou + "', '" + skakoi + "', '" + gkakoi + "')\n";
    }
    else {
        throw std::runtime_error("Error: name not found");
    }

    std::string sqlPath = (*matchedSetting)["sql_file_path"];
    
    // Read SQL file content
    std::string sqlContent;
    if (fs::exists(sqlPath)) {
        std::ifstream sqlFile(sqlPath);
        sqlContent = std::string((std::istreambuf_iterator<char>(sqlFile)),
                                std::istreambuf_iterator<char>());
    } else {
        throw std::runtime_error("Error: sql file not found");
    }

    // Remove trailing whitespace and semicolon, add comma
    sqlContent.erase(sqlContent.find_last_not_of(" \t\n\r") + 1);
    if (!sqlContent.empty() && sqlContent.back() == ';') {
        sqlContent.pop_back();
        sqlContent += ",\n";
    }

    // Write back to SQL file
    std::ofstream sqlFile(sqlPath);
    sqlFile << sqlContent << newLine << ";";
}

// Extract game information for wars mode
std::tuple<std::string, bool, std::optional<bool>, std::string, std::string, std::string, std::string> 
extractGameInformationForWars(const std::string& content, const std::string& player) {
    std::string mode;
    if (content.find("10分") != std::string::npos) {
        mode = "10m";
    } else if (content.find("3分") != std::string::npos && content.find("スプリント") == std::string::npos) {
        mode = "3m";
    } else if (content.find("スプリント") != std::string::npos) {
        mode = "sp";
    } else if (content.find("10秒") != std::string::npos) {
        mode = "10s";
    } else {
        throw std::runtime_error("Error: mode not found");
    }

    // Player detection
    std::regex senteRegex(R"(先手[：:](.+))");
    std::regex goteRegex(R"(後手[：:](.+))");
    std::smatch senteMatch, goteMatch;

    if (!std::regex_search(content, senteMatch, senteRegex) || 
        !std::regex_search(content, goteMatch, goteRegex)) {
        throw std::runtime_error("Error: player names not found");
    }

    std::string sente = senteMatch[1].str();
    std::string gote = goteMatch[1].str();

    bool teban;
    if (sente.find(player) != std::string::npos) {
        teban = true;
    } else if (gote.find(player) != std::string::npos) {
        teban = false;
    } else {
        throw std::runtime_error("Error: player not found in game");
    }

    std::optional<bool> result = gameResult(content);
    if (content.find("勝者：▲") != std::string::npos) {
        result = true;
    } else if (content.find("勝者：△") != std::string::npos) {
        result = false;
    }

    // Extract strategies and castles
    std::regex sSenpouRegex(R"(先手の戦法：(.+))");
    std::regex gSenpouRegex(R"(後手の戦法：(.+))");
    std::regex sKakoiRegex(R"(先手の囲い：(.+))");
    std::regex gKakoiRegex(R"(後手の囲い：(.+))");

    std::smatch sSenpouMatch, gSenpouMatch, sKakoiMatch, gKakoiMatch;

    std::string sSenpou = std::regex_search(content, sSenpouMatch, sSenpouRegex) ? 
                         sSenpouMatch[1].str() : "";
    std::string gSenpou = std::regex_search(content, gSenpouMatch, gSenpouRegex) ? 
                         gSenpouMatch[1].str() : "";
    std::string sKakoi = std::regex_search(content, sKakoiMatch, sKakoiRegex) ? 
                        sKakoiMatch[1].str() : "";
    std::string gKakoi = std::regex_search(content, gKakoiMatch, gKakoiRegex) ? 
                        gKakoiMatch[1].str() : "";

    return {mode, teban, result, sSenpou, gSenpou, sKakoi, gKakoi};
}

// Extract game information for 24 mode
std::tuple<std::string, bool, std::optional<bool>, int, int> 
extractGameInformationFor24(const std::string& content, const std::string& player) {
    std::string mode;
    if (content.find("早指し") != std::string::npos && 
        content.find("早指し2") == std::string::npos && 
        content.find("早指し3") == std::string::npos) {
        mode = "hy";
    } else if (content.find("早指し2") != std::string::npos) {
        mode = "hy2";
    } else if (content.find("早指し3") != std::string::npos) {
        mode = "hy3";
    } else if (content.find("15分") != std::string::npos) {
        mode = "15";
    } else if (content.find("長考") != std::string::npos) {
        mode = "30";
    } else {
        throw std::runtime_error("Error: mode not found");
    }

    std::optional<bool> result = gameResult(content);

    std::regex senteRegex(R"(先手：(.+?)\((\d+)\))");
    std::regex goteRegex(R"(後手：(.+?)\((\d+)\))");
    std::smatch senteMatch, goteMatch;

    if (!std::regex_search(content, senteMatch, senteRegex) || 
        !std::regex_search(content, goteMatch, goteRegex)) {
        throw std::runtime_error("Error: player information not found");
    }

    std::string sentePlayer = senteMatch[1].str();
    int senteRating = std::stoi(senteMatch[2].str());
    std::string gotePlayer = goteMatch[1].str();
    int goteRating = std::stoi(goteMatch[2].str());

    bool teban;
    if (sentePlayer.find(player) != std::string::npos) {
        teban = true;
    } else if (gotePlayer.find(player) != std::string::npos) {
        teban = false;
    } else {
        throw std::runtime_error("Error: player not found");
    }

    return {mode, teban, result, senteRating, goteRating};
}

// Determine game result
std::optional<bool> gameResult(const std::string& content) {
    if (content.find("先手の勝ち") != std::string::npos) {
        return true;
    } else if (content.find("後手の勝ち") != std::string::npos) {
        return false;
    }
    return std::nullopt;
}

// Sanitize evaluation value
int sanitizeEval(const std::string& cp) {
    if (cp.find("詰") != std::string::npos) {
        return cp[0] == '+' ? 3300 : -3300;
    }
    int value = std::stoi(cp);
    return std::max(std::min(value, 3300), -3300);
}

// Process lines to calculate average evaluation differences
std::pair<double, double> processLines(const std::string& content) {
    std::vector<int> evalList;
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_match(line, match, EVAL_PATTERN)) {
            std::string rawEval = match[2].str();
            int evalValue = sanitizeEval(rawEval);
            evalList.push_back(evalValue);
        }
    }

    // Calculate differences
    std::vector<int> diffs0, diffs1;
    for (size_t i = 1; i < evalList.size(); ++i) {
        int diff = evalList[i] - evalList[i-1];
        if (i % 2 == 0) {
            diffs0.push_back(diff);
        } else {
            diffs1.push_back(diff);
        }
    }

    // Calculate averages
    double ave0 = diffs0.empty() ? 0.0 : 
                  static_cast<double>(std::accumulate(diffs0.begin(), diffs0.end(), 0)) / diffs0.size();
    double ave1 = diffs1.empty() ? 0.0 : 
                  static_cast<double>(std::accumulate(diffs1.begin(), diffs1.end(), 0)) / diffs1.size();

    return {ave0, ave1};
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file_path>" << std::endl;
        return 1;
    }

    try {
        updateSql(argv[1]);
        std::cout << "SQL update completed successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}