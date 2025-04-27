#pragma once
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

// Implémentation simple pour remplacer nlohmann/json
class SimpleJson {
public:
    enum class Type { Null, Boolean, Number, String, Array, Object };

    class Value {
    private:
        Type type_           = Type::Null;
        bool bool_value_     = false;
        double number_value_ = 0.0;
        std::string string_value_;
        std::vector<std::shared_ptr<Value>> array_values_;
        std::map<std::string, std::shared_ptr<Value>> object_values_;

    public:
        Value() : type_(Type::Null) {}
        explicit Value(bool val) : type_(Type::Boolean), bool_value_(val) {}
        explicit Value(int val) : type_(Type::Number), number_value_(val) {}
        explicit Value(float val) : type_(Type::Number), number_value_(val) {}
        explicit Value(double val) : type_(Type::Number), number_value_(val) {}
        explicit Value(const std::string& val) : type_(Type::String), string_value_(val) {}

        Type getType() const {
            return type_;
        }
        bool isNull() const {
            return type_ == Type::Null;
        }
        bool isBool() const {
            return type_ == Type::Boolean;
        }
        bool isNumber() const {
            return type_ == Type::Number;
        }
        bool isString() const {
            return type_ == Type::String;
        }
        bool isArray() const {
            return type_ == Type::Array;
        }
        bool isObject() const {
            return type_ == Type::Object;
        }

        bool getBool() const {
            return bool_value_;
        }
        int getInt() const {
            return static_cast<int>(number_value_);
        }
        float getFloat() const {
            return static_cast<float>(number_value_);
        }
        double getDouble() const {
            return number_value_;
        }
        const std::string& getString() const {
            return string_value_;
        }

        bool contains(const std::string& key) const {
            if (type_ != Type::Object)
                return false;
            return object_values_.find(key) != object_values_.end();
        }

        Value& operator[](const std::string& key) {
            if (type_ != Type::Object) {
                type_ = Type::Object;
                object_values_.clear();
            }
            if (object_values_.find(key) == object_values_.end()) {
                object_values_[key] = std::make_shared<Value>();
            }
            return *object_values_[key];
        }

        const Value& operator[](const std::string& key) const {
            static Value null_value;
            if (type_ != Type::Object || !contains(key)) {
                return null_value;
            }
            return *object_values_.at(key);
        }

        Value& operator[](size_t index) {
            static Value null_value;
            if (type_ != Type::Array || index >= array_values_.size()) {
                return null_value;
            }
            return *array_values_[index];
        }

        bool is_array() const {
            return type_ == Type::Array;
        }

        size_t size() const {
            if (type_ == Type::Array)
                return array_values_.size();
            if (type_ == Type::Object)
                return object_values_.size();
            return 0;
        }

        // Itérateur simple pour les tableaux
        class ArrayIterator {
            const std::vector<std::shared_ptr<Value>>& array_;
            size_t index_;

        public:
            ArrayIterator(const std::vector<std::shared_ptr<Value>>& arr, size_t idx) : array_(arr), index_(idx) {}
            bool operator!=(const ArrayIterator& other) const {
                return index_ != other.index_;
            }
            void operator++() {
                ++index_;
            }
            const Value& operator*() const {
                return *array_[index_];
            }
        };

        ArrayIterator begin() const {
            return ArrayIterator(array_values_, 0);
        }
        ArrayIterator end() const {
            return ArrayIterator(array_values_, array_values_.size());
        }

        // Parse simple JSON
        static std::shared_ptr<Value> parse(std::istream& input) {
            char c;
            while (input >> c) {
                if (c == '{') {
                    auto obj   = std::make_shared<Value>();
                    obj->type_ = Type::Object;
                    parseObject(input, *obj);
                    return obj;
                } else if (c == '[') {
                    auto arr   = std::make_shared<Value>();
                    arr->type_ = Type::Array;
                    parseArray(input, *arr);
                    return arr;
                }
            }
            return std::make_shared<Value>();
        }

    private:
        static void skipWhitespace(std::istream& input) {
            while (input.peek() == ' ' || input.peek() == '\n' || input.peek() == '\r' || input.peek() == '\t') {
                input.get();
            }
        }

        static void parseObject(std::istream& input, Value& obj) {
            skipWhitespace(input);
            if (input.peek() == '}') {
                input.get();
                return;
            }

            while (true) {
                skipWhitespace(input);
                char quote;
                input >> quote;  // "

                std::string key;
                char c;
                while (input.get(c) && c != '"') {
                    key += c;
                }

                skipWhitespace(input);
                input >> c;  // :

                skipWhitespace(input);
                obj.object_values_[key] = parseValue(input);

                skipWhitespace(input);
                input >> c;  // , ou }
                if (c == '}')
                    break;
            }
        }

        static void parseArray(std::istream& input, Value& arr) {
            skipWhitespace(input);
            if (input.peek() == ']') {
                input.get();
                return;
            }

            while (true) {
                skipWhitespace(input);
                arr.array_values_.push_back(parseValue(input));

                skipWhitespace(input);
                char c;
                input >> c;  // , ou ]
                if (c == ']')
                    break;
            }
        }

        static std::shared_ptr<Value> parseValue(std::istream& input) {
            skipWhitespace(input);
            char c = input.peek();

            if (c == '{') {
                input.get();
                auto obj   = std::make_shared<Value>();
                obj->type_ = Type::Object;
                parseObject(input, *obj);
                return obj;
            } else if (c == '[') {
                input.get();
                auto arr   = std::make_shared<Value>();
                arr->type_ = Type::Array;
                parseArray(input, *arr);
                return arr;
            } else if (c == '"') {
                input.get();
                std::string value;
                while (input.get(c) && c != '"') {
                    value += c;
                }
                return std::make_shared<Value>(value);
            } else if (c == 't') {
                std::string value;
                for (int i = 0; i < 4; ++i) {
                    value += input.get();
                }
                if (value == "true") {
                    return std::make_shared<Value>(true);
                }
            } else if (c == 'f') {
                std::string value;
                for (int i = 0; i < 5; ++i) {
                    value += input.get();
                }
                if (value == "false") {
                    return std::make_shared<Value>(false);
                }
            } else if (c == 'n') {
                std::string value;
                for (int i = 0; i < 4; ++i) {
                    value += input.get();
                }
                if (value == "null") {
                    return std::make_shared<Value>();
                }
            } else if (std::isdigit(c) || c == '-') {
                std::string num_str;
                while (std::isdigit(input.peek()) || input.peek() == '.' || input.peek() == '-' || input.peek() == '+' || input.peek() == 'e' || input.peek() == 'E') {
                    num_str += input.get();
                }
                try {
                    if (num_str.find('.') != std::string::npos || num_str.find('e') != std::string::npos || num_str.find('E') != std::string::npos) {
                        return std::make_shared<Value>(std::stod(num_str));
                    } else {
                        return std::make_shared<Value>(std::stoi(num_str));
                    }
                } catch (...) {
                    return std::make_shared<Value>(0);
                }
            }

            return std::make_shared<Value>();
        }
    };

    SimpleJson() : root_(std::make_shared<Value>()) {}

    bool parse(std::istream& input) {
        try {
            root_ = Value::parse(input);
            return root_ != nullptr;
        } catch (...) {
            return false;
        }
    }

    bool contains(const std::string& key) const {
        return root_->contains(key);
    }

    const Value& operator[](const std::string& key) const {
        return (*root_)[key];
    }

    Value& operator[](const std::string& key) {
        return (*root_)[key];
    }

private:
    std::shared_ptr<Value> root_;
};

class FlowConfigReader {
public:
    FlowConfigReader(const std::string& path = "/userdata/flow.json") : flow_path_(path) {}

    // Recharge le fichier à chaque appel pour hot-reload
    bool reload() {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            std::ifstream flowifs(flow_path_);
            if (!flowifs.good())
                return false;
            flow_.parse(flowifs);
            return true;
        } catch (...) {
            return false;
        }
    }

    // Renvoie le SimpleJson avec la configuration complète
    SimpleJson& getConfig() {
        return flow_;
    }

    // Nouvelles méthodes pour accéder aux sections au niveau racine
    float getRootConfigFloat(const std::string& section, const std::string& param, float defaultValue) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (flow_.contains(section) && flow_[section].contains(param)) {
            return flow_[section][param].getFloat();
        }
        return defaultValue;
    }

    int getRootConfigInt(const std::string& section, const std::string& param, int defaultValue) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (flow_.contains(section) && flow_[section].contains(param)) {
            return flow_[section][param].getInt();
        }
        return defaultValue;
    }

    bool getRootConfigBool(const std::string& section, const std::string& param, bool defaultValue) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (flow_.contains(section) && flow_[section].contains(param)) {
            return flow_[section][param].getBool();
        }
        return defaultValue;
    }

    std::string getRootConfigString(const std::string& section, const std::string& param, const std::string& defaultValue) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (flow_.contains(section) && flow_[section].contains(param)) {
            return flow_[section][param].getString();
        }
        return defaultValue;
    }

    float getNodeConfigFloat(const std::string& nodeId, const std::string& param, float defaultValue) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!flow_.contains("nodes") || !flow_["nodes"].is_array())
            return defaultValue;

        for (const auto& node : flow_["nodes"]) {
            if (node.contains("id") && node["id"].getString() == nodeId && node.contains("config")) {
                const auto& config = node["config"];
                if (config.contains(param)) {
                    return config[param].getFloat();
                }
            }
        }
        return defaultValue;
    }

    int getNodeConfigInt(const std::string& nodeId, const std::string& param, int defaultValue) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!flow_.contains("nodes") || !flow_["nodes"].is_array())
            return defaultValue;

        for (const auto& node : flow_["nodes"]) {
            if (node.contains("id") && node["id"].getString() == nodeId && node.contains("config")) {
                const auto& config = node["config"];
                if (config.contains(param)) {
                    return config[param].getInt();
                }
            }
        }
        return defaultValue;
    }

    bool getNodeConfigBool(const std::string& nodeId, const std::string& param, bool defaultValue) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!flow_.contains("nodes") || !flow_["nodes"].is_array())
            return defaultValue;

        for (const auto& node : flow_["nodes"]) {
            if (node.contains("id") && node["id"].getString() == nodeId && node.contains("config")) {
                const auto& config = node["config"];
                if (config.contains(param)) {
                    return config[param].getBool();
                }
            }
        }
        return defaultValue;
    }

    std::string getNodeConfigString(const std::string& nodeId, const std::string& param, const std::string& defaultValue) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!flow_.contains("nodes") || !flow_["nodes"].is_array())
            return defaultValue;

        for (const auto& node : flow_["nodes"]) {
            if (node.contains("id") && node["id"].getString() == nodeId && node.contains("config")) {
                const auto& config = node["config"];
                if (config.contains(param)) {
                    return config[param].getString();
                }
            }
        }
        return defaultValue;
    }

private:
    std::string flow_path_;
    SimpleJson flow_;
    std::mutex mutex_;
};
