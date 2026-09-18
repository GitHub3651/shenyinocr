#include "engines/ocr/vendor/paddle/include/config.h"

#include "engines/ocr/vendor/paddle/include/utility.h"

#include <QDir>
#include <QFileInfo>
#include <QString>

#include <map>
#include <sstream>
#include <stdexcept>

namespace {

std::map<std::string, std::string> loadConfig(const std::string &configPath)
{
    const std::vector<std::string> lines =
            PaddleOCR::Utility::ReadDict(configPath);
    std::map<std::string, std::string> config;
    for (const std::string &line : lines) {
        std::istringstream stream(line);
        std::string key;
        std::string value;
        if (!(stream >> key) || key[0] == '#') {
            continue;
        }
        if (!(stream >> value)) {
            throw std::runtime_error(
                        "Invalid OCR configuration line: " + line);
        }
        config[key] = value;
    }
    return config;
}

const std::string &requiredValue(
        const std::map<std::string, std::string> &config,
        const std::string &key)
{
    const auto found = config.find(key);
    if (found == config.end()) {
        throw std::runtime_error(
                    "Missing OCR configuration key: " + key);
    }
    return found->second;
}

std::string resolveConfigPath(
        const QDir &configDirectory,
        const std::string &configuredPath)
{
    const QString path = QString::fromStdString(configuredPath);
    return QDir::cleanPath(
                QDir::isAbsolutePath(path)
                ? path
                : configDirectory.absoluteFilePath(path)).toStdString();
}

} // namespace

namespace PaddleOCR {

OCRConfig::OCRConfig(const std::string &configFile)
{
    const std::map<std::string, std::string> config = loadConfig(configFile);
    const QDir configDirectory =
            QFileInfo(QString::fromStdString(configFile)).absoluteDir();

    cpuMathLibraryNumThreads = std::stoi(
                requiredValue(config, "cpu_math_library_num_threads"));
    useMkldnn = std::stoi(requiredValue(config, "use_mkldnn")) != 0;
    maxSideLen = std::stoi(requiredValue(config, "max_side_len"));
    detDbThresh = std::stof(requiredValue(config, "det_db_thresh"));
    detDbBoxThresh = std::stof(
                requiredValue(config, "det_db_box_thresh"));
    detDbUnclipRatio = std::stof(
                requiredValue(config, "det_db_unclip_ratio"));

    detModelDir = resolveConfigPath(
                configDirectory,
                requiredValue(config, "det_model_dir"));
    recModelDir = resolveConfigPath(
                configDirectory,
                requiredValue(config, "rec_model_dir"));
    charListFile = resolveConfigPath(
                configDirectory,
                requiredValue(config, "char_list_file"));
}

} // namespace PaddleOCR
