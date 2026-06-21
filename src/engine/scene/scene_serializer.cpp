#include "scene/scene_serializer.h"

#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/scene.h"
#include "scene/transform.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using scene::Component;
using scene::MeshRenderer;

namespace SceneSerializer {

namespace {

enum class TokenType {
    End,
    ObjectBegin,
    ObjectEnd,
    ArrayBegin,
    ArrayEnd,
    Colon,
    Comma,
    String,
    Number,
};

struct Token {
    TokenType type = TokenType::End;
    std::string text;
};

class Tokenizer {
public:
    explicit Tokenizer(std::string text)
        : text_(std::move(text))
    {
    }

    Token next()
    {
        skipWhitespace();
        if (pos_ >= text_.size()) {
            return {TokenType::End, ""};
        }

        char c = text_[pos_];
        switch (c) {
            case '{': ++pos_; return {TokenType::ObjectBegin, "{"};
            case '}': ++pos_; return {TokenType::ObjectEnd, "}"};
            case '[': ++pos_; return {TokenType::ArrayBegin, "["};
            case ']': ++pos_; return {TokenType::ArrayEnd, "]"};
            case ':': ++pos_; return {TokenType::Colon, ":"};
            case ',': ++pos_; return {TokenType::Comma, ","};
            case '"': return readString();
            default:
                if (isNumberStart(c)) {
                    return readNumber();
                }
                setError(std::string("Unexpected character: ") + c);
                return {TokenType::End, ""};
        }
    }

    bool hasError() const { return hasError_; }
    const std::string& errorMessage() const { return errorMessage_; }

private:
    void skipWhitespace()
    {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    static bool isNumberStart(char c)
    {
        return std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.';
    }

    Token readString()
    {
        ++pos_; // opening quote
        size_t start = pos_;
        while (pos_ < text_.size() && text_[pos_] != '"') {
            ++pos_;
        }
        if (pos_ >= text_.size()) {
            setError("Unterminated string");
            return {TokenType::End, ""};
        }
        std::string value = text_.substr(start, pos_ - start);
        ++pos_; // closing quote
        return {TokenType::String, std::move(value)};
    }

    Token readNumber()
    {
        size_t start = pos_;
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (std::isspace(static_cast<unsigned char>(c)) || c == '{' || c == '}' ||
                c == '[' || c == ']' || c == ':' || c == ',') {
                break;
            }
            ++pos_;
        }
        return {TokenType::Number, text_.substr(start, pos_ - start)};
    }

    void setError(const std::string& message)
    {
        if (!hasError_) {
            hasError_ = true;
            errorMessage_ = message;
        }
    }

    std::string text_;
    size_t pos_ = 0;
    bool hasError_ = false;
    std::string errorMessage_;
};

class Parser {
public:
    Parser(std::string text, std::string* warning)
        : tokenizer_(std::move(text))
        , warning_(warning)
    {
        current_ = tokenizer_.next();
    }

    bool parseInto(Scene& scene, std::string& error)
    {
        if (!expect(TokenType::ObjectBegin, error)) return false;
        if (!expectString("objects", error)) return false;
        if (!expect(TokenType::Colon, error)) return false;
        if (!expect(TokenType::ArrayBegin, error)) return false;

        while (current_.type != TokenType::ArrayEnd) {
            if (!parseObject(scene, error, nullptr)) return false;
            if (current_.type == TokenType::Comma) {
                advance();
            } else if (current_.type != TokenType::ArrayEnd) {
                error = "Expected ',' or ']' in objects array";
                return false;
            }
        }

        if (!expect(TokenType::ArrayEnd, error)) return false;
        if (!expect(TokenType::ObjectEnd, error)) return false;
        if (tokenizer_.hasError()) {
            error = tokenizer_.errorMessage();
            return false;
        }
        return true;
    }

private:
    // Creates the GameObject up front (so any field order, including children,
    // can attach to it), applies fields as they are read, then attaches to
    // parent. Top-down recursion guarantees the parent exists before children.
    bool parseObject(Scene& scene, std::string& error, GameObject* parent)
    {
        if (!expect(TokenType::ObjectBegin, error)) return false;

        GameObject* obj = scene.createObject("Object");

        bool firstField = true;
        while (current_.type != TokenType::ObjectEnd) {
            if (!firstField) {
                if (current_.type == TokenType::Comma) {
                    advance();
                } else {
                    error = "Expected ',' between object fields";
                    return false;
                }
            }
            firstField = false;

            if (current_.type != TokenType::String) {
                error = "Expected field name";
                return false;
            }
            std::string field = current_.text;
            advance();
            if (!expect(TokenType::Colon, error)) return false;

            if (field == "name") {
                if (current_.type != TokenType::String) {
                    error = "Expected string for name";
                    return false;
                }
                obj->setName(current_.text);
                advance();
            } else if (field == "position") {
                if (!parseFloat3(obj->transform().position, error)) return false;
            } else if (field == "rotation") {
                if (!parseFloat3(obj->transform().rotation, error)) return false;
            } else if (field == "scale") {
                if (!parseFloat3(obj->transform().scale, error)) return false;
            } else if (field == "components") {
                std::vector<std::unique_ptr<Component>> comps;
                if (!parseComponents(comps, error)) return false;
                for (auto& c : comps) {
                    obj->addComponent(std::move(c));
                }
            } else if (field == "children") {
                if (!parseChildren(scene, obj, error)) return false;
            } else {
                error = "Unknown field: " + field;
                return false;
            }
        }

        if (!expect(TokenType::ObjectEnd, error)) return false;

        // Attach to parent (top-down: parent already exists and is fresh).
        if (parent && !scene.setParent(obj, parent)) {
            error = "Failed to attach child '" + obj->name() + "' to parent";
            return false;
        }

        if (!obj->hasComponent<MeshRenderer>()) {
            addWarning("Object '" + obj->name() + "' has no MeshRenderer and will not render.");
        }

        return true;
    }

    bool parseChildren(Scene& scene, GameObject* parent, std::string& error)
    {
        if (!expect(TokenType::ArrayBegin, error)) return false;

        while (current_.type != TokenType::ArrayEnd) {
            if (!parseObject(scene, error, parent)) return false;
            if (current_.type == TokenType::Comma) {
                advance();
            } else if (current_.type != TokenType::ArrayEnd) {
                error = "Expected ',' or ']' in children array";
                return false;
            }
        }

        return expect(TokenType::ArrayEnd, error);
    }

    bool parseComponents(std::vector<std::unique_ptr<Component>>& out, std::string& error)
    {
        if (!expect(TokenType::ArrayBegin, error)) return false;

        while (current_.type != TokenType::ArrayEnd) {
            if (!parseComponent(out, error)) return false;
            if (current_.type == TokenType::Comma) {
                advance();
            } else if (current_.type != TokenType::ArrayEnd) {
                error = "Expected ',' or ']' in components array";
                return false;
            }
        }

        return expect(TokenType::ArrayEnd, error);
    }

    bool parseComponent(std::vector<std::unique_ptr<Component>>& out, std::string& error)
    {
        if (!expect(TokenType::ObjectBegin, error)) return false;

        std::string type;
        simd::float4 color{1.0f, 1.0f, 1.0f, 1.0f};
        std::string mesh = "cube";
        Vec3 angularVelocity{0.0f, 0.0f, 0.0f};

        bool firstField = true;
        while (current_.type != TokenType::ObjectEnd) {
            if (!firstField) {
                if (current_.type == TokenType::Comma) {
                    advance();
                } else {
                    error = "Expected ',' between component fields";
                    return false;
                }
            }
            firstField = false;

            if (current_.type != TokenType::String) {
                error = "Expected component field name";
                return false;
            }
            std::string field = current_.text;
            advance();
            if (!expect(TokenType::Colon, error)) return false;

            if (field == "type") {
                if (current_.type != TokenType::String) {
                    error = "Expected string for component type";
                    return false;
                }
                type = current_.text;
                advance();
            } else if (field == "mesh") {
                if (current_.type != TokenType::String) {
                    error = "Expected string for mesh";
                    return false;
                }
                mesh = current_.text;
                advance();
            } else if (field == "color") {
                if (!parseFloat4(color, error)) return false;
            } else if (field == "angularVelocity") {
                if (!parseFloat3(angularVelocity, error)) return false;
            } else {
                error = "Unknown component field: " + field;
                return false;
            }
        }

        if (!expect(TokenType::ObjectEnd, error)) return false;

        if (type == "MeshRenderer") {
            scene::MeshShape shape;
            if (!scene::meshShapeFromString(mesh, shape)) {
                error = "Unknown mesh shape: " + mesh;
                return false;
            }
            auto meshRenderer = std::make_unique<MeshRenderer>();
            meshRenderer->shape = shape;
            meshRenderer->color = color;
            out.push_back(std::move(meshRenderer));
        } else if (type == "RotateComponent") {
            auto rotator = std::make_unique<scene::RotateComponent>();
            rotator->angularVelocityEuler = angularVelocity;
            out.push_back(std::move(rotator));
        } else {
            error = "Unknown component type: " + type;
            return false;
        }

        return true;
    }

    bool parseFloat3(Vec3& out, std::string& error)
    {
        if (!expect(TokenType::ArrayBegin, error)) return false;
        if (!parseNumber(out.x, error)) return false;
        if (!expect(TokenType::Comma, error)) return false;
        if (!parseNumber(out.y, error)) return false;
        if (!expect(TokenType::Comma, error)) return false;
        if (!parseNumber(out.z, error)) return false;
        if (!expect(TokenType::ArrayEnd, error)) return false;
        return true;
    }

    bool parseFloat4(simd::float4& out, std::string& error)
    {
        float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
        if (!expect(TokenType::ArrayBegin, error)) return false;
        if (!parseNumber(x, error)) return false;
        if (!expect(TokenType::Comma, error)) return false;
        if (!parseNumber(y, error)) return false;
        if (!expect(TokenType::Comma, error)) return false;
        if (!parseNumber(z, error)) return false;
        if (!expect(TokenType::Comma, error)) return false;
        if (!parseNumber(w, error)) return false;
        if (!expect(TokenType::ArrayEnd, error)) return false;
        out = {x, y, z, w};
        return true;
    }

    bool parseNumber(float& out, std::string& error)
    {
        if (current_.type != TokenType::Number) {
            error = "Expected number";
            return false;
        }
        try {
            size_t idx = 0;
            out = std::stof(current_.text, &idx);
            if (idx != current_.text.size()) {
                error = "Invalid number: " + current_.text;
                return false;
            }
        } catch (...) {
            error = "Invalid number: " + current_.text;
            return false;
        }
        advance();
        return true;
    }

    bool expectString(const std::string& value, std::string& error)
    {
        if (current_.type != TokenType::String || current_.text != value) {
            error = "Expected '" + value + "'";
            return false;
        }
        advance();
        return true;
    }

    bool expect(TokenType type, std::string& error)
    {
        if (current_.type != type) {
            error = "Unexpected token";
            return false;
        }
        advance();
        return true;
    }

    void advance()
    {
        if (!tokenizer_.hasError()) {
            current_ = tokenizer_.next();
        }
    }

    void addWarning(const std::string& message)
    {
        if (!warning_) return;
        if (!warning_->empty()) {
            *warning_ += "\n";
        }
        *warning_ += message;
    }

    Tokenizer tokenizer_;
    Token current_;
    std::string* warning_ = nullptr;
};

} // namespace

// Recursive object writer. Flat objects (no children) serialize byte-for-byte
// as before: the "children" field is omitted entirely. Children, if any, are
// written in insertion order on a deeper-indented line.
static void writeObject(std::ostream& out, const GameObject& obj, const std::string& indent)
{
    const Transform& t = obj.transform();
    out << indent << "{ ";
    out << "\"name\": \"" << obj.name() << "\", ";
    out << "\"position\": [" << t.position.x << ", " << t.position.y << ", " << t.position.z << "], ";
    out << "\"rotation\": [" << t.rotation.x << ", " << t.rotation.y << ", " << t.rotation.z << "], ";
    out << "\"scale\": [" << t.scale.x << ", " << t.scale.y << ", " << t.scale.z << "], ";
    out << "\"components\": [";

    bool firstComp = true;
    for (const auto& comp : obj.components()) {
        if (auto* mesh = dynamic_cast<const MeshRenderer*>(comp.get())) {
            if (!firstComp) out << ", ";
            firstComp = false;
            out << "{\"type\": \"MeshRenderer\", ";
            out << "\"mesh\": \"" << scene::meshShapeToString(mesh->shape) << "\", ";
            out << "\"color\": [" << mesh->color.x << ", " << mesh->color.y << ", " << mesh->color.z << ", " << mesh->color.w << "]}";
        } else if (auto* rot = dynamic_cast<const scene::RotateComponent*>(comp.get())) {
            if (!firstComp) out << ", ";
            firstComp = false;
            out << "{\"type\": \"RotateComponent\", ";
            out << "\"angularVelocity\": [" << rot->angularVelocityEuler.x << ", " << rot->angularVelocityEuler.y << ", " << rot->angularVelocityEuler.z << "]}";
        }
    }

    out << "]";
    if (!obj.children().empty()) {
        out << ", \"children\": [\n";
        const std::string childIndent = indent + "    ";
        bool firstChild = true;
        for (const GameObject* child : obj.children()) {
            if (!firstChild) out << ",\n";
            firstChild = false;
            writeObject(out, *child, childIndent);
        }
        out << "\n" << indent << "]";
    }
    out << " }";
}

bool save(const Scene& scene, const std::string& path, std::string& error)
{
    std::filesystem::path filePath(path);
    try {
        std::filesystem::create_directories(filePath.parent_path());
    } catch (const std::exception& e) {
        error = std::string("Failed to create directory: ") + e.what();
        return false;
    }

    std::ofstream out(filePath);
    if (!out.is_open()) {
        error = "Failed to open file for writing: " + path;
        return false;
    }

    out << std::fixed << std::setprecision(4);
    out << "{\n";
    out << "  \"objects\": [\n";

    bool first = true;
    for (const auto& obj : scene.objects()) {
        if (scene.isCamera(obj.get())) {
            continue;
        }
        if (obj->parent() != nullptr) {
            // Children are serialized nested under their parent; skip them here.
            continue;
        }

        if (!first) {
            out << ",\n";
        }
        first = false;

        writeObject(out, *obj, "    ");
    }

    out << "\n  ]\n";
    out << "}\n";
    return true;
}

bool load(Scene& scene, const std::string& path, std::string& error, std::string* warning)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        error = "Failed to open file for reading: " + path;
        return false;
    }

    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.fail() && !in.eof()) {
        error = "Failed to read file: " + path;
        return false;
    }

    scene.clearObjects();

    Parser parser(std::move(text), warning);
    if (!parser.parseInto(scene, error)) {
        return false;
    }

    return true;
}

} // namespace SceneSerializer
