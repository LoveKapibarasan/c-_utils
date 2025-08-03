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
#include <cstdlib>
#include "json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// Pattern definition for evaluation lines (supports both old and new formats)
const std::regex EVAL_PATTERN(R"(^\*(?:\*解析\s*0\s*(?:[△○])?\s*(?:候補1)?\s*時間.*?評価値\s*([+-]?\d+|[+-]?詰(?:\s*\d+)?)|#評価値=([+-]?\d+)))");

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
void processDirectory(const std::string& dirPath);

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

// Read file with encoding conversion using iconv command
std::string readFileWithEncoding(const std::string& filepath) {
    // First try to read as UTF-8
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    // Check if content contains Japanese characters that might be in Shift_JIS
    // If the file contains bytes that don't form valid UTF-8, try Shift_JIS conversion
    bool needsConversion = false;
    for (unsigned char c : content) {
        if (c >= 0x80 && c <= 0xFF) {
            needsConversion = true;
            break;
        }
    }
    
    if (needsConversion) {
        // Use iconv command to convert from Shift_JIS to UTF-8
        std::string tempFile = "/tmp/temp_convert.txt";
        std::string command = "iconv -f shift_jis -t utf-8 \"" + filepath + "\" > \"" + tempFile + "\" 2>/dev/null";
        
        if (std::system(command.c_str()) == 0) {
            std::ifstream convertedFile(tempFile);
            if (convertedFile.is_open()) {
                std::string convertedContent((std::istreambuf_iterator<char>(convertedFile)),
                                           std::istreambuf_iterator<char>());
                convertedFile.close();
                std::remove(tempFile.c_str());
                return convertedContent;
            }
        }
        std::remove(tempFile.c_str());
    }
    
    return content;
}

// Main update SQL function
void updateSql(const std::string& inputFilePath) {
    json settings = loadSettings();
    std::string filename = fs::path(inputFilePath).filename().string();
    
    std::cout << "=== Processing file: " << filename << " ===" << std::endl;
    std::cout << "Full path: " << inputFilePath << std::endl;
    
    auto matchedSetting = findSetting(filename, settings);
    if (!matchedSetting) {
        std::string errorMsg = "Setting for file not found in setting.json";
        std::cout << "ERROR: " << errorMsg << std::endl;
        throw std::runtime_error(errorMsg);
    }

    std::cout << "Matched setting: " << (*matchedSetting)["name"] << std::endl;
    std::cout << "Pattern: " << (*matchedSetting)["pattern"] << std::endl;
    std::cout << "Player: " << (*matchedSetting)["player"] << std::endl;

    std::string content = readFileWithEncoding(inputFilePath);
    std::cout << "File content loaded successfully (size: " << content.size() << " bytes)" << std::endl;
    
    std::string newLine;
    
    if ((*matchedSetting)["name"] == "24") {
        std::cout << "mode is 24 at " << inputFilePath << std::endl;
        auto [mode, teban, result, senteRating, goteRating] = 
            extractGameInformationFor24(content, (*matchedSetting)["player"]);
        std::cout << "Game info extracted - mode: " << mode << ", teban: " << (teban ? "sente" : "gote") 
                  << ", result: " << (result.has_value() ? (result.value() ? "win" : "lose") : "draw") << std::endl;
        std::cout << "Ratings - sente: " << senteRating << ", gote: " << goteRating << std::endl;
        
        auto [ave0, ave1] = processLines(content);
        std::cout << "Evaluation processing - ave0: " << ave0 << ", ave1: " << ave1 << std::endl;
        
        std::string tebanStr = teban ? "true" : "false";
        std::string resultStr = result.has_value() ? 
            (result.value() ? "true" : "false") : "NULL";
        
        newLine = "('" + mode + "', " + tebanStr + ", " + resultStr + ", " +
                  std::to_string(static_cast<int>(std::round(ave0))) + ", " +
                  std::to_string(static_cast<int>(std::round(ave1))) + ", " +
                  std::to_string(senteRating) + ", " + std::to_string(goteRating) + ", '" + filename + "')\n";
    }
    else if ((*matchedSetting)["name"] == "wars") {
        std::cout << "mode is wars at " << inputFilePath << std::endl;
        auto [mode, teban, result, senpou, gopou, skakoi, gkakoi] = 
            extractGameInformationForWars(content, (*matchedSetting)["player"]);
        std::cout << "Game info extracted - mode: " << mode << ", teban: " << (teban ? "sente" : "gote") 
                  << ", result: " << (result.has_value() ? (result.value() ? "win" : "lose") : "draw") << std::endl;
        std::cout << "Strategies - sente: " << senpou << ", gote: " << gopou << std::endl;
        std::cout << "Castles - sente: " << skakoi << ", gote: " << gkakoi << std::endl;
        
        auto [ave0, ave1] = processLines(content);
        std::cout << "Evaluation processing - ave0: " << ave0 << ", ave1: " << ave1 << std::endl;
        
        std::string tebanStr = teban ? "true" : "false";
        std::string resultStr = result.has_value() ? 
            (result.value() ? "true" : "false") : "NULL";
        
        newLine = "('" + mode + "', " + tebanStr + ", " + resultStr + ", " +
                  std::to_string(static_cast<int>(std::round(ave0))) + ", " +
                  std::to_string(static_cast<int>(std::round(ave1))) + ", " +
                  "'" + senpou + "', '" + gopou + "', '" + skakoi + "', '" + gkakoi + "', '" + filename + "')\n";
    }
    else {
        std::string errorMsg = "Error: Unknown mode name: " + (*matchedSetting)["name"].get<std::string>();
        std::cout << "ERROR: " << errorMsg << std::endl;
        throw std::runtime_error(errorMsg);
    }

    std::cout << "Generated SQL line: " << newLine << std::endl;

    std::string sqlPath = (*matchedSetting)["sql_file_path"];
    std::cout << "Target SQL file: " << sqlPath << std::endl;
    
    // Read SQL file content
    std::string sqlContent;
    if (fs::exists(sqlPath)) {
        std::ifstream sqlFile(sqlPath);
        sqlContent = std::string((std::istreambuf_iterator<char>(sqlFile)),
                                std::istreambuf_iterator<char>());
        std::cout << "SQL file loaded successfully (size: " << sqlContent.size() << " bytes)" << std::endl;
    } else {
        std::string errorMsg = "Error: sql file not found: " + sqlPath;
        std::cout << "ERROR: " << errorMsg << std::endl;
        throw std::runtime_error(errorMsg);
    }

    // Remove trailing whitespace and semicolon, add comma
    sqlContent.erase(sqlContent.find_last_not_of(" \t\n\r") + 1);
    if (!sqlContent.empty() && sqlContent.back() == ';') {
        sqlContent.pop_back();
        sqlContent += ",\n";
        std::cout << "Removed trailing semicolon and added comma" << std::endl;
    }

    // Write back to SQL file
    std::ofstream sqlFile(sqlPath);
    if (!sqlFile.is_open()) {
        std::string errorMsg = "Failed to open SQL file for writing: " + sqlPath;
        std::cout << "ERROR: " << errorMsg << std::endl;
        throw std::runtime_error(errorMsg);
    }
    
    sqlFile << sqlContent << newLine << ";";
    sqlFile.close();
    std::cout << "SQL file updated successfully" << std::endl;
    std::cout << "Added entry: " << newLine.substr(0, newLine.length()-1) << std::endl;
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
    int lineCount = 0;
    int matchCount = 0;
    
    std::cout << "--- Evaluation Processing ---" << std::endl;
    
    while (std::getline(stream, line)) {
        lineCount++;
        std::smatch match;
        if (std::regex_match(line, match, EVAL_PATTERN)) {
            matchCount++;
            // Old format uses match[1], new format uses match[2]
            std::string rawEval = match[1].matched ? match[1].str() : match[2].str();
            int evalValue = sanitizeEval(rawEval);
            evalList.push_back(evalValue);
            
            if (matchCount <= 5 || matchCount % 20 == 0) {
                std::cout << "Line " << lineCount << ": " << rawEval << " -> " << evalValue << std::endl;
            }
        }
    }
    
    std::cout << "Total lines processed: " << lineCount << std::endl;
    std::cout << "Evaluation lines found: " << matchCount << std::endl;
    std::cout << "Evaluation values collected: " << evalList.size() << std::endl;

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

    std::cout << "Differences calculated - diffs0: " << diffs0.size() << ", diffs1: " << diffs1.size() << std::endl;

    // Calculate averages
    double ave0 = diffs0.empty() ? 0.0 : 
                  static_cast<double>(std::accumulate(diffs0.begin(), diffs0.end(), 0)) / diffs0.size();
    double ave1 = diffs1.empty() ? 0.0 : 
                  static_cast<double>(std::accumulate(diffs1.begin(), diffs1.end(), 0)) / diffs1.size();

    std::cout << "Final averages - ave0: " << ave0 << ", ave1: " << ave1 << std::endl;
    std::cout << "--- End Evaluation Processing ---" << std::endl;

    return {ave0, ave1};
}

// Function to process all kif files in a directory recursively
void processDirectory(const std::string& dirPath) {
    std::cout << "=== Processing directory: " << dirPath << " ===" << std::endl;
    
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        throw std::runtime_error("Directory does not exist or is not a directory: " + dirPath);
    }
    
    std::vector<std::string> kifFiles;
    
    // Recursively find all .kif files
    for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".kif") {
            kifFiles.push_back(entry.path().string());
        }
    }
    
    std::cout << "Found " << kifFiles.size() << " .kif files" << std::endl;
    
    if (kifFiles.empty()) {
        std::cout << "No .kif files found in directory: " << dirPath << std::endl;
        return;
    }
    
    // Sort files for consistent processing order
    std::sort(kifFiles.begin(), kifFiles.end());
    
    int successCount = 0;
    int errorCount = 0;
    
    for (size_t i = 0; i < kifFiles.size(); ++i) {
        const std::string& kifFile = kifFiles[i];
        std::cout << "\n[" << (i + 1) << "/" << kifFiles.size() << "] Processing: " 
                  << fs::path(kifFile).filename().string() << std::endl;
        
        try {
            updateSql(kifFile);
            successCount++;
            std::cout << "✓ Successfully processed" << std::endl;
        } catch (const std::exception& e) {
            errorCount++;
            std::cout << "✗ Error processing file: " << e.what() << std::endl;
            // Continue processing other files instead of stopping
        }
    }
    
    std::cout << "\n=== Processing Summary ===" << std::endl;
    std::cout << "Total files found: " << kifFiles.size() << std::endl;
    std::cout << "Successfully processed: " << successCount << std::endl;
    std::cout << "Errors encountered: " << errorCount << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file_path_or_directory>" << std::endl;
        std::cerr << "  - If a file is specified, processes that single .kif file" << std::endl;
        std::cerr << "  - If a directory is specified, recursively processes all .kif files in that directory" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    
    try {
        if (fs::is_directory(inputPath)) {
            // Process directory recursively
            processDirectory(inputPath);
            std::cout << "Directory processing completed successfully." << std::endl;
        } else if (fs::is_regular_file(inputPath)) {
            // Process single file
            updateSql(inputPath);
            std::cout << "SQL update completed successfully." << std::endl;
        } else {
            throw std::runtime_error("Input path is neither a file nor a directory: " + inputPath);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}