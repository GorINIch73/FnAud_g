/**
 * @file test_data_generator.cpp
 * @brief Utility for generating test data (bank export simulation)
 * 
 * Generates TSV file with payment data for import testing
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <random>
#include <ctime>
#include <iomanip>
#include <cstring>

// Структуры данных
struct CounterpartyData {
    std::string name;
    std::string inn;
};

struct ContractData {
    std::string number;
    std::string date;
};

struct InvoiceData {
    std::string number;
    std::string date;
    std::string type;  // "акт", "накладная", "счет-фактура"
};

// Генератор случайных чисел
class RandomGenerator {
private:
    std::mt19937 gen_;
    std::uniform_int_distribution<int> dist_;
    
public:
    RandomGenerator(unsigned int seed = 0) {
        if (seed == 0) {
            seed = static_cast<unsigned int>(std::time(nullptr));
        }
        gen_.seed(seed);
    }
    
    int next(int min, int max) {
        std::uniform_int_distribution<int> d(min, max);
        return d(gen_);
    }
    
    double nextDouble(double min, double max) {
        std::uniform_real_distribution<double> d(min, max);
        return d(gen_);
    }
    
    const std::string& pick(const std::vector<std::string>& items) {
        return items[dist_(gen_) % items.size()];
    }
    
    template<typename T>
    const T& pick(const std::vector<T>& items) {
        return items[dist_(gen_) % items.size()];
    }
};

// Генератор данных
class TestDataGenerator {
private:
    RandomGenerator rng_;
    std::vector<CounterpartyData> counterparties_;
    std::vector<std::string> kosguCodes_;
    std::vector<std::string> operationTypes_;
    std::vector<std::string> contractPrefixes_;
    std::vector<std::string> invoiceTypes_;
    
    std::string generateDate(int year, int month, int day) {
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << day << "."
            << std::setfill('0') << std::setw(2) << month << "."
            << year;
        return oss.str();
    }
    
    std::string generateRandomDate(int startYear, int endYear) {
        int year = rng_.next(startYear, endYear);
        int month = rng_.next(1, 12);
        int day = rng_.next(1, 28);  // Упрощённо, без учёта дней в месяцах
        return generateDate(year, month, day);
    }
    
    std::string generateINN() {
        // Генерируем 10-значный ИНН
        std::ostringstream oss;
        for (int i = 0; i < 9; ++i) {
            oss << rng_.next(0, 9);
        }
        // Контрольная сумма (упрощённо)
        int sum = 0;
        std::string inn = oss.str();
        int weights[] = {2, 4, 10, 3, 5, 9, 4, 6, 8};
        for (int i = 0; i < 9; ++i) {
            sum += (inn[i] - '0') * weights[i];
        }
        int checkDigit = (sum % 11) % 10;
        oss << checkDigit;
        return oss.str();
    }
    
    std::string generateContractNumber() {
        std::string prefix = contractPrefixes_[rng_.next(0, contractPrefixes_.size() - 1)];
        int number = rng_.next(1, 9999);
        std::ostringstream oss;
        oss << prefix << "-" << std::setfill('0') << std::setw(4) << number;
        return oss.str();
    }
    
    std::string generateInvoiceNumber() {
        int number = rng_.next(1, 99999);
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(5) << number;
        return oss.str();
    }
    
public:
    TestDataGenerator(unsigned int seed = 0) : rng_(seed) {
        initCounterparties();
        initKosguCodes();
        initOperationTypes();
        initContractPrefixes();
        initInvoiceTypes();
    }
    
    void initCounterparties() {
        counterparties_ = {
            {"ООО \"ТехноСервис\"", "7701234567"},
            {"АО \"СтройМаш\"", "7702345678"},
            {"ООО \"Офисные решения\"", "7703456789"},
            {"ИП Петров А.В.", "770456789012"},
            {"ООО \"ЭнергоСбыт\"", "7705678901"},
            {"ЗАО \"ТрансЛогистик\"", "7706789012"},
            {"ООО \"МедТехника\"", "7707890123"},
            {"АО \"СвязьИнвест\"", "7708901234"},
            {"ООО \"Клининг Про\"", "7709012345"},
            {"ООО \"ПринтСервис\"", "7710123456"},
            {"ЗАО \"ДатаЦентр\"", "7711234567"},
            {"ООО \"ЮрКонсалт\"", "7712345678"},
            {"АО \"АльфаБанк\"", "7713456789"},
            {"ООО \"БетаСтрахование\"", "7714567890"},
            {"ООО \"ГаммаТрейд\"", "7715678901"},
            {"ИП Сидоров И.И.", "771678901234"},
            {"ООО \"ДельтаСофт\"", "7717890123"},
            {"ЗАО \"Эпсилон\"", "7718901234"},
            {"ООО \"ЖилКомСервис\"", "7719012345"},
            {"АО \"Зенит\"", "7720123456"},
            {"ООО \"Импульс\"", "7721234567"},
            {"ООО \"Йога-Центр\"", "7722345678"},
            {"ЗАО \"Карат\"", "7723456789"},
            {"ООО \"Лидер\"", "7724567890"},
            {"ООО \"Маяк\"", "7725678901"},
            {"АО \"Нева\"", "7726789012"},
            {"ООО \"Орион\"", "7727890123"},
            {"ЗАО \"Прогресс\"", "7728901234"},
            {"ООО \"Радуга\"", "7729012345"},
            {"ООО \"Спектр\"", "7730123456"}
        };
    }
    
    void initKosguCodes() {
        kosguCodes_ = {
            "100", "130", "180",  // Доходы
            "210", "220", "230", "240", "250", "260", "290",  // Расходы
            "310", "320", "340", "350",  // Увеличение стоимости
            "510", "520", "530", "550", "560",  // Финансовые активы
            "610", "620", "630", "650", "660",  // Уменьшение финансовых активов
            "710", "720", "730"  // Обязательства
        };
    }
    
    void initOperationTypes() {
        operationTypes_ = {
            "Оплата по договору",
            "Авансовый платеж",
            "Окончательный расчет",
            "Оплата услуг",
            "Оплата товаров",
            "Оплата работ",
            "Возврат средств",
            "Перечисление налога",
            "Оплата аренды",
            "Оплата связи",
            "Оплата коммунальных услуг",
            "Оплата транспорта",
            "Оплата рекламы",
            "Оплата консультаций",
            "Оплата подписки"
        };
    }
    
    void initContractPrefixes() {
        contractPrefixes_ = {
            "К", "Д", "ГК", "ДК", "КД", "Контракт"
        };
    }
    
    void initInvoiceTypes() {
        invoiceTypes_ = {
            "акт", "накладная", "счет-фактура", "УПД", "акт выполненных работ"
        };
    }
    
    std::string generateDescription(const ContractData& contract, 
                                     const InvoiceData& invoice,
                                     const std::string& kosguCode,
                                     const std::string& operationType) {
        std::ostringstream oss;
        
        // Формируем описание с использованием шаблонов из ТЗ
        int templateNum = rng_.next(0, 4);
        
        switch (templateNum) {
            case 0:
                // Шаблон с договором и КОСГУ
                oss << operationType << " по контракту №" << contract.number 
                    << " от " << contract.date << ". К" << kosguCode;
                break;
            case 1:
                // Шаблон с накладной и договором
                oss << operationType << ". " << invoice.type << " №" << invoice.number 
                    << " от " << invoice.date << ". Контракт №" << contract.number 
                    << " от " << contract.date << ". К" << kosguCode;
                break;
            case 2:
                // Шаблон с актом и КОСГУ
                oss << invoice.type << " №" << invoice.number << " от " << invoice.date 
                    << ". " << operationType << ". К" << kosguCode;
                break;
            case 3:
                // Полный шаблон
                oss << operationType << " по договору №" << contract.number 
                    << " от " << contract.date << ". Основание: " << invoice.type 
                    << " №" << invoice.number << " от " << invoice.date 
                    << ". КОСГУ К" << kosguCode;
                break;
        }
        
        return oss.str();
    }
    
    struct PaymentData {
        std::string docNumber;
        std::string date;
        std::string counterparty;
        std::string inn;
        double amount;
        std::string description;
    };
    
    PaymentData generatePayment(int paymentNumber) {
        PaymentData payment;
        
        // Номер платежного поручения
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(6) << paymentNumber;
        payment.docNumber = oss.str();
        
        // Дата
        payment.date = generateRandomDate(2023, 2025);
        
        // Контрагент
        const auto& cp = counterparties_[rng_.next(0, counterparties_.size() - 1)];
        payment.counterparty = cp.name;
        payment.inn = cp.inn;
        
        // Сумма (от 1000 до 500000 рублей)
        payment.amount = std::round(rng_.nextDouble(1000, 500000) * 100) / 100;
        
        // Описание
        ContractData contract;
        contract.number = generateContractNumber();
        contract.date = generateRandomDate(2022, 2024);
        
        InvoiceData invoice;
        invoice.type = invoiceTypes_[rng_.next(0, invoiceTypes_.size() - 1)];
        invoice.number = generateInvoiceNumber();
        invoice.date = generateRandomDate(2023, 2025);
        
        std::string kosgu = kosguCodes_[rng_.next(0, kosguCodes_.size() - 1)];
        std::string operationType = operationTypes_[rng_.next(0, operationTypes_.size() - 1)];
        
        payment.description = generateDescription(contract, invoice, kosgu, operationType);
        
        return payment;
    }
    
    bool generateTSV(const std::string& filename, int recordCount) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: cannot create file " << filename << std::endl;
            return false;
        }

        // Header
        file << "doc_number\tdate\trecipient\tinn\tamount\tdescription\n";

        // Generate records
        for (int i = 1; i <= recordCount; ++i) {
            PaymentData payment = generatePayment(i);

            file << payment.docNumber << "\t"
                 << payment.date << "\t"
                 << payment.counterparty << "\t"
                 << payment.inn << "\t"
                 << std::fixed << std::setprecision(2) << payment.amount << "\t"
                 << payment.description << "\n";

            // Progress
            if (i % 100 == 0 || i == recordCount) {
                std::cout << "\rRecords generated: " << i << "/" << recordCount << std::flush;
            }
        }

        file.close();
        std::cout << std::endl;

        return true;
    }
};

void printUsage(const char* programName) {
    std::cout << "Test Data Generator for Financial Audit\n"
              << "========================================\n\n"
              << "Usage:\n"
              << "  " << programName << " <record_count> [filename] [seed]\n\n"
              << "Arguments:\n"
              << "  record_count  - number of records to generate (1 to 100000)\n"
              << "  filename      - output TSV filename (default: bank_export.tsv)\n"
              << "  seed          - random generator seed (default: system time)\n\n"
              << "Examples:\n"
              << "  " << programName << " 1000                    - generate 1000 records\n"
              << "  " << programName << " 5000 test_data.tsv      - generate 5000 records to test_data.tsv\n"
              << "  " << programName << " 10000 data.tsv 42       - generate 10000 records with fixed seed\n\n"
              << "Output file format:\n"
              << "  TSV (Tab-Separated Values) with columns:\n"
              << "  - doc_number   : payment document number\n"
              << "  - date         : payment date (DD.MM.YYYY)\n"
              << "  - recipient    : counterparty name\n"
              << "  - inn          : counterparty tax ID (INN)\n"
              << "  - amount       : payment amount\n"
              << "  - description  : payment description (contains KOSGU, contract, invoice)\n\n"
              << "Description contains:\n"
              << "  - Contract number and date\n"
              << "  - Invoice/act number and date\n"
              << "  - KOSGU code (format KXXX)\n"
              << "  - Operation type\n";
}

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "  Test Data Generator v1.0\n";
    std::cout << "  for Financial Audit\n";
    std::cout << "========================================\n\n";

    // Parse command line arguments
    if (argc < 2 || std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0) {
        printUsage(argv[0]);
        return 0;
    }

    int recordCount = 0;
    std::string filename = "bank_export.tsv";
    unsigned int seed = 0;

    // Record count
    try {
        recordCount = std::stoi(argv[1]);
        if (recordCount < 1 || recordCount > 100000) {
            std::cerr << "Error: record count must be between 1 and 100000\n";
            return 1;
        }
    } catch (...) {
        std::cerr << "Error: invalid record count '" << argv[1] << "'\n";
        return 1;
    }

    // Filename (optional)
    if (argc >= 3) {
        filename = argv[2];
        // Add .tsv extension if not present
        size_t dotPos = filename.rfind('.');
        size_t slashPos = filename.rfind('/');
        size_t backslashPos = filename.rfind('\\');
        
        // Check if filename has an extension (dot after last path separator)
        // Note: npos is returned when separator not found (very large number)
        bool hasDot = (dotPos != std::string::npos);
        bool hasSlash = (slashPos != std::string::npos);
        bool hasBackslash = (backslashPos != std::string::npos);
        bool dotAfterPath = (!hasSlash || dotPos > slashPos) && (!hasBackslash || dotPos > backslashPos);
        bool hasExtension = hasDot && dotAfterPath && (dotPos < filename.length() - 1);
        
        if (!hasExtension) {
            filename += ".tsv";
        }
    }

    // Seed (optional)
    if (argc >= 4) {
        try {
            seed = static_cast<unsigned int>(std::stoul(argv[3]));
        } catch (...) {
            std::cerr << "Error: invalid seed '" << argv[3] << "'\n";
            return 1;
        }
    }

    std::cout << "Generation parameters:\n";
    std::cout << "  Record count: " << recordCount << "\n";
    std::cout << "  Filename: " << filename << "\n";
    std::cout << "  Seed: " << (seed == 0 ? "random (system time)" : std::to_string(seed)) << "\n\n";

    std::cout << "Generating data...\n";
    
    TestDataGenerator generator(seed);
    
    auto startTime = std::clock();

    if (generator.generateTSV(filename, recordCount)) {
        auto endTime = std::clock();
        double duration = static_cast<double>(endTime - startTime) / CLOCKS_PER_SEC;

        std::cout << "\nGeneration complete!\n";
        std::cout << "  File: " << filename << "\n";
        std::cout << "  Records: " << recordCount << "\n";
        std::cout << "  Duration: " << std::fixed << std::setprecision(2) << duration << " sec.\n";

        // Estimate file size
        std::ifstream fileSizeCheck(filename, std::ios::binary | std::ios::ate);
        if (fileSizeCheck.is_open()) {
            std::streamsize size = fileSizeCheck.tellg();
            std::cout << "  Size: " << std::fixed << std::setprecision(2)
                      << (size / 1024.0 / 1024.0) << " MB\n";
        }

        std::cout << "\nSample of first 5 records:\n";
        std::cout << "----------------------------------------\n";

        std::ifstream previewFile(filename);
        std::string line;
        int lineCount = 0;
        while (std::getline(previewFile, line) && lineCount < 6) {
            std::cout << line << "\n";
            lineCount++;
        }
        previewFile.close();

        std::cout << "----------------------------------------\n";
        std::cout << "\nFile is ready for import into Financial Audit!\n";

        return 0;
    } else {
        std::cerr << "\nGeneration error!\n";
        return 1;
    }
}
