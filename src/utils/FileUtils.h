/**
 * @file FileUtils.h
 * @brief Утилиты для работы с файлами
 */

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace FinancialAudit {

class FileUtils {
public:
    static bool fileExists(const std::string& path) {
        std::ifstream f(path);
        return f.good();
    }
    
    static bool readFile(const std::string& path, std::string& content) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
        
        return true;
    }
    
    static bool writeFile(const std::string& path, const std::string& content) {
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        
        file << content;
        return true;
    }
    
    static std::string getFileName(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos) {
            return path.substr(pos + 1);
        }
        return path;
    }
    
    static std::string getFileExtension(const std::string& path) {
        size_t pos = path.find_last_of('.');
        if (pos != std::string::npos) {
            return path.substr(pos);
        }
        return "";
    }
    
    static std::vector<std::string> readLines(const std::string& path) {
        std::vector<std::string> lines;
        std::ifstream file(path);
        
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                lines.push_back(line);
            }
        }
        
        return lines;
    }
};

} // namespace FinancialAudit
