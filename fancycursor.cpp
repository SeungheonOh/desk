#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "../include/texture_manager.h"

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float px, float py) : x(px), y(py) {}

    Vec2 operator+(const Vec2& rhs) const { return Vec2{x + rhs.x, y + rhs.y}; }
    Vec2 operator-(const Vec2& rhs) const { return Vec2{x - rhs.x, y - rhs.y}; }
    Vec2 operator*(float s) const { return Vec2{x * s, y * s}; }
    Vec2 operator/(float s) const { return Vec2{x / s, y / s}; }
    Vec2& operator+=(const Vec2& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    float length() const { return std::sqrt(x * x + y * y); }

    Vec2 normalized(float eps = 1e-5f) const {
        float len = length();
        if (len <= eps) {
            return Vec2{};
        }
        return Vec2{x / len, y / len};
    }

    Vec2 min(const Vec2& other) const { return Vec2{std::min(x, other.x), std::min(y, other.y)}; }
    Vec2 max(const Vec2& other) const { return Vec2{std::max(x, other.x), std::max(y, other.y)}; }
};

struct SelectionBox {
    Vec2 min;
    Vec2 max;

    static SelectionBox fromPoints(const Vec2& a, const Vec2& b) {
        return SelectionBox{a.min(b), a.max(b)};
    }

    Vec2 center() const { return (min + max) * 0.5f; }
    Vec2 halfSize() const { return (max - min) * 0.5f; }
};

struct Element {
    int id = 0;
    Vec2 anchor;
    Vec2 targetPos;
    Vec2 visualPos;
    Vec2 grabOffset;
    Vec2 halfSize;
    std::array<float, 3> color{};
    float lift = 0.0f;
    float liftTarget = 0.0f;
    float rotation = 0.0f;
    float rotationTarget = 0.0f;
    bool isDragging = false;
    
    // Texture/GIF support
    GLuint textureId = 0;
    bool hasTexture = false;
    std::string imagePath;
    GifData* gifData = nullptr;

    bool contains(const Vec2& point) const {
        Vec2 min = visualPos - halfSize;
        Vec2 max = visualPos + halfSize;
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
    }

    std::array<float, 4> renderColor(bool selected) const {
        float selectionBoost = selected ? 0.12f : 0.0f;
        float liftBoost = isDragging ? 0.1f : (0.02f * lift);
        float boost = selectionBoost + liftBoost;
        return {
            std::min(color[0] + boost, 1.0f),
            std::min(color[1] + boost, 1.0f),
            std::min(color[2] + boost, 1.0f),
            1.0f,
        };
    }
};

enum class InteractionType { None, Dragging, Selecting, Resizing };

struct ResizeInfo {
    int id = -1;
    Vec2 pivot;
    Vec2 initialBR;
    float rotation = 0.0f;
};

struct Interaction {
    InteractionType type = InteractionType::None;
    std::vector<int> dragIds;
    Vec2 start;
    Vec2 current;
    std::vector<ResizeInfo> resizeInfos;
    Vec2 resizePointerStart;
};

static float smoothScalar(float current, float target, float dt, float speed) {
    float t = 1.0f - std::exp(-speed * dt);
    return current + (target - current) * t;
}

static Vec2 smoothVec(const Vec2& current, const Vec2& target, float dt, float speed) {
    return Vec2{
        smoothScalar(current.x, target.x, dt, speed),
        smoothScalar(current.y, target.y, dt, speed),
    };
}

static Vec2 rotateVec(const Vec2& v, float angle) {
    float s = std::sin(angle);
    float c = std::cos(angle);
    return Vec2{v.x * c - v.y * s, v.x * s + v.y * c};
}

static Vec2 rotateVecInverse(const Vec2& v, float angle) {
    float s = std::sin(angle);
    float c = std::cos(angle);
    return Vec2{v.x * c + v.y * s, -v.x * s + v.y * c};
}

class Desk {
  public:
    Desk(TextureManager* texMgr = nullptr) : textureManager(texMgr) {
        const std::array<std::array<float, 3>, 6> palette{{
            {0.94f, 0.45f, 0.37f},
            {0.43f, 0.70f, 0.94f},
            {0.93f, 0.82f, 0.40f},
            {0.55f, 0.84f, 0.64f},
            {0.76f, 0.50f, 0.84f},
            {0.40f, 0.62f, 0.91f},
        }};

        for (int id = 0; id < static_cast<int>(palette.size()); ++id) {
            int col = id % 3;
            int row = id / 3;
            Vec2 pos{180.0f + col * 220.0f, 140.0f + row * 200.0f};
            elements.push_back(Element{
                .id = id,
                .anchor = pos,
                .targetPos = pos,
                .visualPos = pos,
                .grabOffset = Vec2{},
                .halfSize = Vec2{90.0f, 65.0f},
                .color = palette[id],
            });
        }
    }
    
    void assignImageToElement(int elementId, const std::string& imagePath) {
        if (!textureManager) return;
        
        Element* element = elementById(elementId);
        if (!element) return;
        
        // Check if it's a GIF
        GifData* gif = textureManager->getGifPtr(imagePath);
        if (gif && gif->isValid) {
            element->gifData = gif;
            element->hasTexture = true;
            element->imagePath = imagePath;
            return;
        }
        
        // Try loading as regular texture
        TextureData texture = textureManager->getTexture(imagePath);
        if (texture.isValid) {
            element->textureId = texture.textureId;
            element->hasTexture = true;
            element->imagePath = imagePath;
            element->gifData = nullptr;
        }
    }

    const std::vector<Element>& getElements() const { return elements; }

    const std::vector<int>& selectedIds() const { return selection; }

    std::optional<SelectionBox> selectionBox() const {
        if (interaction.type == InteractionType::Selecting) {
            return SelectionBox::fromPoints(interaction.start, interaction.current);
        }
        return std::nullopt;
    }
    
    std::optional<SelectionBox> selectedElementsBounds() const {
        if (selection.empty()) {
            return std::nullopt;
        }
        Vec2 minBounds{1e9f, 1e9f};
        Vec2 maxBounds{-1e9f, -1e9f};
        
        for (int id : selection) {
            if (const Element* element = elementById(id)) {
                Vec2 elemMin = element->visualPos - element->halfSize;
                Vec2 elemMax = element->visualPos + element->halfSize;
                minBounds = minBounds.min(elemMin);
                maxBounds = maxBounds.max(elemMax);
            }
        }
        
        if (minBounds.x > maxBounds.x || minBounds.y > maxBounds.y) {
            return std::nullopt;
        }
        return SelectionBox{minBounds, maxBounds};
    }
    
    int getSelectionCount() const {
        return selection.size();
    }

    void setRotationModifier(bool active) { rotateModifier = active; }

    void setResizeModifier(bool active) { resizeModifier = active; }

    void setZoomModifier(bool active) { zoomModifier = active; }

    float viewScaleValue() const { return viewScale; }

    void pointerMoved(const Vec2& screenPos) {
        pointerScreen = screenPos;
        updatePointerDerivedState();
    }

    void pointerPressed() {
        auto stack = idsAtPointer();
        if (!stack.empty()) {
            int id = stack.front();
            if (std::find(selection.begin(), selection.end(), id) == selection.end()) {
                selection.clear();
                selection.push_back(id);
            }
            std::vector<int> affected = selection;
            raiseToFront(affected);
            if (resizeModifier && beginResizeInteraction()) {
                return;
            }
            Vec2 ref = pointerPos;
            for (int dragId : affected) {
                if (Element* element = elementById(dragId)) {
                    element->isDragging = true;
                    element->liftTarget = 1.0f;
                    element->grabOffset = ref - element->visualPos;
                    element->targetPos = element->visualPos;
                }
            }
            interaction.type = InteractionType::Dragging;
            interaction.dragIds = std::move(affected);
        } else {
            selection.clear();
            interaction.type = InteractionType::Selecting;
            interaction.start = pointerPos;
            interaction.current = pointerPos;
            interaction.dragIds.clear();
        }
    }

    void pointerReleased() {
        if (interaction.type == InteractionType::Dragging) {
            for (int id : interaction.dragIds) {
                if (Element* element = elementById(id)) {
                    element->isDragging = false;
                    element->liftTarget = 0.0f;
                    element->anchor = element->targetPos;
                }
            }
        } else if (interaction.type == InteractionType::Resizing) {
            for (const ResizeInfo& info : interaction.resizeInfos) {
                if (Element* element = elementById(info.id)) {
                    element->liftTarget = 0.0f;
                }
            }
        }
        interaction = Interaction{};
    }

    void scrollView(const Vec2& delta) {
        viewOffset += delta / viewScale;
        updatePointerDerivedState();
    }

    Vec2 viewTranslation() const { return viewOffset; }

    bool mouseWheel(float amount) {
        if (std::abs(amount) < 1e-4f) {
            return false;
        }

        if (zoomModifier && zoomWithScroll(amount)) {
            return true;
        }

        if (rotateModifier && !selection.empty()) {
            float angle = amount * 0.04f;
            bool rotateGroup = selection.size() > 1;
            Vec2 pivot = rotateGroup ? selectionCenterPoint() : Vec2{};
            float s = std::sin(angle);
            float c = std::cos(angle);
            auto rotateAroundPivot = [&](const Vec2& point) {
                Vec2 rel = point - pivot;
                return pivot + Vec2{rel.x * c - rel.y * s, rel.x * s + rel.y * c};
            };
            for (int id : selection) {
                if (Element* element = elementById(id)) {
                    element->rotationTarget += angle;
                    if (rotateGroup) {
                        Vec2 rotated = rotateAroundPivot(element->visualPos);
                        element->visualPos = rotated;
                        element->targetPos = rotated;
                        if (!element->isDragging) {
                            element->anchor = rotated;
                        }
                        if (element->isDragging) {
                            element->grabOffset = pointerPos - rotated;
                        }
                    }
                }
            }
            return true;
        }

        return cycleHoverStack(amount);
    }

    void update(float dt) {
        std::vector<int> dragging = currentDragIds();
        for (Element& element : elements) {
            bool isDragging = std::find(dragging.begin(), dragging.end(), element.id) != dragging.end();
            if (!isDragging) {
                element.targetPos = element.anchor;
            }
            float stiffness = isDragging ? 18.0f : 9.0f;
            element.visualPos = smoothVec(element.visualPos, element.targetPos, dt, stiffness);
            element.lift = smoothScalar(element.lift, element.liftTarget, dt, 8.0f + element.liftTarget * 4.0f);
            element.rotation = smoothScalar(element.rotation, element.rotationTarget, dt, 12.0f);
            element.isDragging = isDragging;
            
            // Update GIF animations
            if (element.gifData) {
                element.gifData->updateFrame(dt);
            }
        }
    }

  private:
     std::vector<Element> elements;
     std::vector<int> selection;
     Interaction interaction{};
     Vec2 pointerPos;
     Vec2 pointerScreen;
     Vec2 viewOffset;
     bool rotateModifier = false;
     bool resizeModifier = false;
     bool zoomModifier = false;
     float viewScale = 1.0f;
     TextureManager* textureManager = nullptr;

    Element* elementById(int id) {
        auto it = std::find_if(elements.begin(), elements.end(), [&](const Element& e) { return e.id == id; });
        return it == elements.end() ? nullptr : &(*it);
    }

    const Element* elementById(int id) const {
        auto it = std::find_if(elements.begin(), elements.end(), [&](const Element& e) { return e.id == id; });
        return it == elements.end() ? nullptr : &(*it);
    }

    Vec2 selectionCenterPoint() const {
        Vec2 sum{};
        int count = 0;
        for (int id : selection) {
            if (const Element* element = elementById(id)) {
                sum += element->visualPos;
                ++count;
            }
        }
        if (count == 0) {
            return Vec2{};
        }
        return sum / static_cast<float>(count);
    }

    bool zoomWithScroll(float amount) {
        if (!zoomModifier) {
            return false;
        }
        float factor = std::exp(amount * 0.08f);
        float newScale = std::clamp(viewScale * factor, 0.4f, 3.0f);
        if (std::abs(newScale - viewScale) < 1e-4f) {
            return false;
        }
        Vec2 focus = pointerPos;
        viewScale = newScale;
        viewOffset = focus - (pointerScreen / viewScale);
        pointerPos = focus;
        return true;
    }

    bool beginResizeInteraction() {
        if (!resizeModifier || selection.empty()) {
            return false;
        }
        interaction.resizeInfos.clear();
        interaction.resizePointerStart = pointerPos;
        for (int id : selection) {
            if (Element* element = elementById(id)) {
                ResizeInfo info;
                info.id = id;
                info.rotation = element->rotation;
                Vec2 half = element->halfSize;
                Vec2 center = element->visualPos;
                Vec2 topLeftLocal{-half.x, -half.y};
                Vec2 bottomRightLocal{half.x, half.y};
                info.pivot = center + rotateVec(topLeftLocal, info.rotation);
                info.initialBR = center + rotateVec(bottomRightLocal, info.rotation);
                interaction.resizeInfos.push_back(info);
                element->liftTarget = 1.0f;
            }
        }
        if (interaction.resizeInfos.empty()) {
            return false;
        }
        interaction.type = InteractionType::Resizing;
        interaction.dragIds.clear();
        return true;
    }

    void applyResizeDelta(const Vec2& rawDelta) {
        if (interaction.resizeInfos.empty()) {
            return;
        }
        const Vec2 minFullSize{40.0f, 40.0f};
        for (const ResizeInfo& info : interaction.resizeInfos) {
            if (Element* element = elementById(info.id)) {
                Vec2 desiredBR = info.initialBR + rawDelta;
                Vec2 pivotToDesired = desiredBR - info.pivot;
                Vec2 localFull = rotateVecInverse(pivotToDesired, info.rotation);
                localFull.x = std::max(localFull.x, minFullSize.x);
                localFull.y = std::max(localFull.y, minFullSize.y);
                Vec2 localHalf = localFull * 0.5f;
                Vec2 newCenter = info.pivot + rotateVec(localHalf, info.rotation);
                element->halfSize = localHalf;
                element->targetPos = newCenter;
                element->visualPos = newCenter;
                element->anchor = newCenter;
            }
        }
    }

    void updatePointerDerivedState() {
        if (viewScale < 1e-5f) {
            viewScale = 1e-5f;
        }
        pointerPos = pointerScreen / viewScale + viewOffset;
        if (interaction.type == InteractionType::Selecting) {
            interaction.current = pointerPos;
            SelectionBox rect = SelectionBox::fromPoints(interaction.start, interaction.current);
            selection = idsInRect(rect);
        }
        if (interaction.type == InteractionType::Resizing) {
            applyResizeDelta(pointerPos - interaction.resizePointerStart);
        }
        for (int id : currentDragIds()) {
            if (Element* element = elementById(id)) {
                element->targetPos = pointerPos - element->grabOffset;
            }
        }
    }

    std::vector<int> currentDragIds() const {
        if (interaction.type == InteractionType::Dragging) {
            return interaction.dragIds;
        }
        return {};
    }

    std::vector<int> idsInRect(const SelectionBox& rect) const {
        std::vector<int> ids;
        for (const Element& element : elements) {
            Vec2 min = element.visualPos - element.halfSize;
            Vec2 max = element.visualPos + element.halfSize;
            bool overlap = !(rect.max.x < min.x || rect.min.x > max.x || rect.max.y < min.y || rect.min.y > max.y);
            if (overlap) {
                ids.push_back(element.id);
            }
        }
        return ids;
    }

    std::vector<int> idsAtPointer() const {
        std::vector<int> stacks;
        for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
            if (it->contains(pointerPos)) {
                stacks.push_back(it->id);
            }
        }
        return stacks;
    }

    void raiseToFront(const std::vector<int>& ids) {
        if (ids.empty()) {
            return;
        }
        std::vector<Element> moved;
        for (auto it = elements.begin(); it != elements.end();) {
            if (std::find(ids.begin(), ids.end(), it->id) != ids.end()) {
                moved.push_back(*it);
                it = elements.erase(it);
            } else {
                ++it;
            }
        }
        for (Element& element : moved) {
            elements.push_back(element);
        }
    }

    bool cycleHoverStack(float scroll) {
        int direction = scroll > 0.0f ? 1 : -1;
        auto stack = idsAtPointer();
        if (stack.size() < 2) {
            return false;
        }

        int current = stack.front();
        for (int id : selection) {
            if (std::find(stack.begin(), stack.end(), id) != stack.end()) {
                current = id;
                break;
            }
        }

        auto it = std::find(stack.begin(), stack.end(), current);
        int idx = static_cast<int>(std::distance(stack.begin(), it));
        int size = static_cast<int>(stack.size());
        idx = (idx + direction) % size;
        if (idx < 0) {
            idx += size;
        }

        int nextId = stack[static_cast<std::size_t>(idx)];
        selection.clear();
        selection.push_back(nextId);
        raiseToFront(selection);
        return true;
    }
};

class Renderer {
  public:
     Renderer() {
        program = createProgram();
        resolutionLoc = glGetUniformLocation(program, "u_resolution");
        centerLoc = glGetUniformLocation(program, "u_center");
        baseHalfLoc = glGetUniformLocation(program, "u_base_half");
        depthLoc = glGetUniformLocation(program, "u_depth");
        colorLoc = glGetUniformLocation(program, "u_color");
        rotationLoc = glGetUniformLocation(program, "u_rotation");
        renderModeLoc = glGetUniformLocation(program, "u_render_mode");
        hasTextureLoc = glGetUniformLocation(program, "u_has_texture");
        textureLoc = glGetUniformLocation(program, "u_texture");

        cursorProgram = createCursorProgram();
        cursorResolutionLoc = glGetUniformLocation(cursorProgram, "u_resolution");
        cursorCenterLoc = glGetUniformLocation(cursorProgram, "u_center");
        
        selectionOutlineProgram = createSelectionOutlineProgram();
        outlineResolutionLoc = glGetUniformLocation(selectionOutlineProgram, "u_resolution");
        outlineViewOffsetLoc = glGetUniformLocation(selectionOutlineProgram, "u_view_offset");
        outlineViewScaleLoc = glGetUniformLocation(selectionOutlineProgram, "u_view_scale");
        outlineColorLoc = glGetUniformLocation(selectionOutlineProgram, "u_color");

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
         glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
         glBindVertexArray(0);
         
         // Create a dummy white texture for when no texture is available
         glGenTextures(1, &dummyTexture);
         glBindTexture(GL_TEXTURE_2D, dummyTexture);
         uint8_t whitePixel[4] = {255, 255, 255, 255};
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         glBindTexture(GL_TEXTURE_2D, 0);

         glEnable(GL_BLEND);
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         
         // Create outline VAO/VBO
         glGenVertexArrays(1, &outlineVAO);
         glGenBuffers(1, &outlineVBO);
         }

    ~Renderer() {
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &outlineVBO);
        glDeleteVertexArrays(1, &outlineVAO);
        glDeleteProgram(program);
        glDeleteProgram(cursorProgram);
        glDeleteProgram(selectionOutlineProgram);
        if (screenTexture) glDeleteTextures(1, &screenTexture);
        if (dummyTexture) glDeleteTextures(1, &dummyTexture);
    }

    void updateOutlineGeometry(const SelectionBox& bounds) {
        if (!outlineVAO || !outlineVBO) return;
        
        // Get bounds
        Vec2 min = bounds.min;
        Vec2 max = bounds.max;
        Vec2 center = (min + max) * 0.5f;
        Vec2 halfSize = (max - min) * 0.5f;
        
        // Generate organic outline with outward bulges
        std::vector<float> vertices;
        int segments = 120;  // Smooth segments for organic feel
        
        for (int i = 0; i < segments; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(segments);
            float angle = t * 2.0f * 3.14159265f;
            
            // Create point on rounded rectangle
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);
            
            // Use absolute values for rounded rectangle shape
            float absX = std::abs(cosA);
            float absY = std::abs(sinA);
            
            // Normalize to box space (creates rounded rectangle)
            float maxAbs = std::max(absX, absY);
            float boxX = (maxAbs > 0.0f) ? cosA / maxAbs : 0.0f;
            float boxY = (maxAbs > 0.0f) ? sinA / maxAbs : 0.0f;
            
            // Base position on box edge with rounded corners
            float cornerSoftness = 0.3f;
            float cornerAmount = std::min(absX, absY) * cornerSoftness;
            boxX = boxX * (1.0f - cornerAmount) + cosA * cornerAmount;
            boxY = boxY * (1.0f - cornerAmount) + sinA * cornerAmount;
            
            // Position on bounding box
            Vec2 basePos = center + Vec2{boxX * halfSize.x, boxY * halfSize.y};
            
            // Add organic variation - outward bulges
            float noise1 = std::sin(t * 12.5f) * 0.4f + std::sin(t * 7.3f) * 0.3f;
            float noise2 = std::cos(t * 9.8f) * 0.35f + std::cos(t * 5.2f) * 0.25f;
            
            // Outward normal direction
            Vec2 normal{boxX, boxY};
            normal = normal.normalized();
            
            // Apply organic offset outward (not inward)
            float outwardBulge = 12.0f + noise1 * 6.0f;
            Vec2 finalPos = basePos + normal * outwardBulge;
            
            // Add slight organic wobble perpendicular to normal
            Vec2 tangent{-normal.y, normal.x};
            finalPos = finalPos + tangent * noise2 * 3.0f;
            
            vertices.push_back(finalPos.x);
            vertices.push_back(finalPos.y);
        }
        
        outlineVertexCount = static_cast<int>(vertices.size()) / 2;
        
        glBindVertexArray(outlineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, outlineVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glBindVertexArray(0);
    }

    void resize(int width, int height) {
         viewportWidth = width;
         viewportHeight = height;
         glViewport(0, 0, width, height);

        // Recreate screen texture
        if (screenTexture) glDeleteTextures(1, &screenTexture);

        glGenTextures(1, &screenTexture);
        glBindTexture(GL_TEXTURE_2D, screenTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void draw(const std::vector<Element>& elements,
             const std::vector<int>& selected,
             std::optional<SelectionBox> selectionBox,
             std::optional<SelectionBox> selectedBounds,
             const Vec2& viewOffset,
             float viewScale,
             const Vec2& cursorPos,
             float elapsedTime) {
       if (viewportWidth == 0 || viewportHeight == 0) {
           return;
       }

       glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
       glClear(GL_COLOR_BUFFER_BIT);

       glUseProgram(program);
       glUniform2f(resolutionLoc, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight));
       glBindVertexArray(vao);

       auto drawPass = [&](int mode) {
           glUniform1i(renderModeLoc, mode);
           for (const Element& element : elements) {
               bool selectedFlag = std::find(selected.begin(), selected.end(), element.id) != selected.end();
               Vec2 liftedCenter = element.visualPos + Vec2{0.0f, -element.lift * 22.0f};
               Vec2 screenCenter = (liftedCenter - viewOffset) * viewScale;
               Vec2 scaledHalf = element.halfSize * viewScale;
               
               GLuint textureId = 0;
               bool hasTexture = false;
               if (element.hasTexture) {
                   if (element.gifData && element.gifData->isValid) {
                       textureId = element.gifData->getCurrentTexture();
                   } else {
                       textureId = element.textureId;
                   }
                   hasTexture = (textureId != 0);
               }
               
               drawElement(screenCenter, scaledHalf, element.lift, element.renderColor(selectedFlag), element.rotation, textureId, hasTexture);
           }
       };
       // Draw every shadow first so the later card pass can fully occlude them.
       drawPass(1);
       drawPass(0);

       if (selectionBox) {
           glUniform1i(renderModeLoc, 0);
           Vec2 center = (selectionBox->center() - viewOffset) * viewScale;
           Vec2 half = selectionBox->halfSize() * viewScale;
           drawElement(center, half, -1.0f, {0.35f, 0.6f, 0.95f, 0.15f}, 0.0f);
       }
       
       // Draw selection outline for multiple selected elements
       if (selectedBounds && selected.size() > 1) {
           // Update outline geometry
           updateOutlineGeometry(*selectedBounds);
           
           glBindVertexArray(outlineVAO);
           glUseProgram(selectionOutlineProgram);
           
           glUniform2f(outlineResolutionLoc, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight));
           glUniform2f(outlineViewOffsetLoc, viewOffset.x, viewOffset.y);
           glUniform1f(outlineViewScaleLoc, viewScale);
           
           // Draw thick outer glow (soft)
           glLineWidth(12.0f);
           glUniform4f(outlineColorLoc, 0.1f, 0.8f, 1.0f, 0.3f);
           glDrawArrays(GL_LINE_LOOP, 0, outlineVertexCount);
           
           // Draw sharp inner line (bright)
           glLineWidth(2.5f);
           glUniform4f(outlineColorLoc, 0.4f, 1.0f, 1.0f, 0.85f);
           glDrawArrays(GL_LINE_LOOP, 0, outlineVertexCount);
           
           glLineWidth(1.0f);
       }

       // Capture screen content to texture using glCopyTexSubImage2D (before cursor)
       glBindTexture(GL_TEXTURE_2D, screenTexture);
       glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, viewportWidth, viewportHeight);

       // Draw liquid glass cursor (always drawn last, on top)
       glUseProgram(cursorProgram);
       glBindVertexArray(vao);
       glUniform2f(cursorResolutionLoc, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight));
       glUniform2f(cursorCenterLoc, cursorPos.x, cursorPos.y);
       glActiveTexture(GL_TEXTURE0);
       glBindTexture(GL_TEXTURE_2D, screenTexture);
       glUniform1i(glGetUniformLocation(cursorProgram, "u_screen_texture"), 0);
       glDrawArrays(GL_TRIANGLES, 0, 6);

       glBindVertexArray(0);
       glUseProgram(0);
    }

  private:
      GLuint program = 0;
     GLuint cursorProgram = 0;
     GLuint selectionOutlineProgram = 0;
     GLuint vao = 0;
     GLuint vbo = 0;
     GLuint screenTexture = 0;
     GLuint dummyTexture = 0;
     GLuint outlineVAO = 0;
     GLuint outlineVBO = 0;
     int outlineVertexCount = 0;
     GLint resolutionLoc = -1;
     GLint centerLoc = -1;
     GLint baseHalfLoc = -1;
     GLint depthLoc = -1;
     GLint colorLoc = -1;
     GLint rotationLoc = -1;
     GLint renderModeLoc = -1;
     GLint hasTextureLoc = -1;
     GLint textureLoc = -1;
     GLint cursorResolutionLoc = -1;
     GLint cursorCenterLoc = -1;
     GLint outlineResolutionLoc = -1;
     GLint outlineViewOffsetLoc = -1;
     GLint outlineViewScaleLoc = -1;
     GLint outlineColorLoc = -1;
     int viewportWidth = 0;
     int viewportHeight = 0;

    static constexpr float quadVertices[12] = {
        -1.0f, -1.0f, //
        1.0f,  -1.0f, //
        1.0f,  1.0f,  //
        -1.0f, -1.0f, //
        1.0f,  1.0f,  //
        -1.0f, 1.0f,  //
    };

    static GLuint compileShader(GLenum type, const char* src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint status = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            std::string log(length, '\0');
            glGetShaderInfoLog(shader, length, nullptr, log.data());
            std::cerr << "Shader compile error: " << log << std::endl;
        }
        return shader;
    }

    static GLuint createCursorProgram() {
         constexpr const char* vertexShader = R"(#version 330 core
    layout (location = 0) in vec2 a_position;
    uniform vec2 u_resolution;
    uniform vec2 u_center;
    
    out vec2 v_uv;
    out vec2 v_screen_pos;
    
    void main() {
        float radius = 28.0;
        vec2 local = a_position * radius;
        vec2 world = local + u_center;
        v_uv = local / radius;
        v_screen_pos = world;
        vec2 ndc = (world / u_resolution) * 2.0 - 1.0;
        gl_Position = vec4(ndc * vec2(1.0, -1.0), 0.0, 1.0);
    }
)";

        constexpr const char* fragmentShader = R"(#version 330 core
        in vec2 v_uv;
        in vec2 v_screen_pos;
        uniform vec2 u_resolution;
        uniform vec2 u_center;
        uniform sampler2D u_screen_texture;
        out vec4 out_color;

void main() {
     vec2 uv = v_uv;
     float dist = length(uv);

    // Early discard for points outside circle
    if (dist > 1.0) discard;

    // Convex lens distortion - light bends inward at edges
    // Spherical lens effect - stronger at edges
    float sphereProfile = sqrt(1.0 - dist * dist);
    float distortionStrength = (1.0 - sphereProfile) * 3.5; // Extreme distortion

    // Push sample position inward toward center (convex bulging effect)
    vec2 sampleLocalPos = uv * (1.0 - distortionStrength);

    // Clamp to circle - ensure we stay within bounds
    float sampleDist = length(sampleLocalPos);
    if (sampleDist > 1.0) {
        sampleLocalPos = normalize(sampleLocalPos) * 0.99;
    }

    // Convert local position to screen coordinates
    vec2 sampleScreenPos = v_screen_pos + sampleLocalPos * 28.0;
    vec2 sampleUv = sampleScreenPos / u_resolution;
    sampleUv.y = 1.0 - sampleUv.y;

    // Chromatic aberration - visible at edges
    float edgeFactor = smoothstep(0.6, 1.0, dist); // Stronger at edge
    float aberrationAmount = edgeFactor * 0.025; // More noticeable

    // Sample each color channel with offsets
    vec2 aberrationDir = normalize(uv + vec2(0.001));

    vec2 redUv = sampleUv + aberrationDir * aberrationAmount * 1.0;
    vec2 greenUv = sampleUv;
    vec2 blueUv = sampleUv - aberrationDir * aberrationAmount * 1.0;

    float r = texture(u_screen_texture, redUv).r;
    float g = texture(u_screen_texture, greenUv).g;
    float b = texture(u_screen_texture, blueUv).b;

    vec3 color = vec3(r, g, b);
    
    // Apply orange tint
    vec3 orangeTint = vec3(1.0, 0.6, 0.2);
    color = mix(color, orangeTint, 0.3);

    // Fresnel effect - inverted for bright backgrounds
    float fresnel = smoothstep(0.65, 1.0, dist);
    float brightness = dot(color, vec3(0.299, 0.587, 0.114)); // Perceptual brightness
    
    // Invert fresnel when background is bright
    vec3 fresnelGlow = brightness > 0.5 ? vec3(0.0) : vec3(1.0);
    vec3 fresnelColor = mix(color, fresnelGlow, fresnel * 0.25);

    // Smooth alpha with soft edges
    float alpha = (1.0 - dist * dist) * 0.95;

    out_color = vec4(fresnelColor, alpha);
}
)";

        GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShader);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShader);
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        GLint status = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status != GL_TRUE) {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            std::string log(length, '\0');
            glGetProgramInfoLog(program, length, nullptr, log.data());
            std::cerr << "Cursor program link error: " << log << std::endl;
        }
        glDetachShader(program, vs);
        glDetachShader(program, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return program;
    }

    static GLuint createSelectionOutlineProgram() {
        constexpr const char* vertexShader = R"(#version 330 core
    layout (location = 0) in vec2 a_position;
    uniform vec2 u_resolution;
    uniform vec2 u_view_offset;
    uniform float u_view_scale;

    out vec2 v_pos;

    void main() {
    // Transform world coordinates to screen coordinates
    vec2 screen_pos = (a_position - u_view_offset) * u_view_scale;
    
    // Transform to NDC
    vec2 ndc = (screen_pos / u_resolution) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_pos = screen_pos;
    }
    )";

        constexpr const char* fragmentShader = R"(#version 330 core
    uniform vec4 u_color;
    out vec4 out_color;

    void main() {
    out_color = u_color;
    }
    )";

        GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShader);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShader);
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        GLint status = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status != GL_TRUE) {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            std::string log(length, '\0');
            glGetProgramInfoLog(program, length, nullptr, log.data());
            std::cerr << "Selection outline program link error: " << log << std::endl;
        }
        glDetachShader(program, vs);
        glDetachShader(program, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return program;
    }

    static GLuint createProgram() {
        constexpr const char* vertexShader = R"(#version 330 core
layout (location = 0) in vec2 a_position;
uniform vec2 u_resolution;
uniform vec2 u_center;
uniform vec2 u_base_half;
uniform float u_depth;
uniform float u_rotation;

out vec2 v_local;
out vec2 v_card_half;
out vec2 v_shadow_half;
out vec2 v_shadow_offset;
out float v_shadow_alpha;

void main() {
    float depth = max(u_depth, 0.0);
    float cardScale = 1.0 + depth * 0.04;
    v_card_half = u_base_half * cardScale;
    v_shadow_half = u_base_half * (1.0 + depth * 0.15) + vec2(6.0);
    vec2 shadowOffsetWorld = vec2(0.0, 18.0 + depth * 42.0);
    float s = sin(u_rotation);
    float c = cos(u_rotation);
    v_shadow_offset = vec2(
        shadowOffsetWorld.x * c + shadowOffsetWorld.y * s,
        -shadowOffsetWorld.x * s + shadowOffsetWorld.y * c
    );
    v_shadow_alpha = clamp(0.25 + depth * 0.3, 0.0, 0.85);
    if (u_depth < 0.0) {
        v_shadow_alpha = 0.0;
    }

    vec2 geom_half = vec2(
        max(v_card_half.x, abs(v_shadow_offset.x) + v_shadow_half.x),
        max(v_card_half.y, abs(v_shadow_offset.y) + v_shadow_half.y)
    );
    vec2 local = a_position * geom_half;
    v_local = local;
    vec2 rotated = vec2(local.x * c - local.y * s, local.x * s + local.y * c);
    vec2 world = rotated + u_center;
    vec2 ndc = (world / u_resolution) * 2.0 - 1.0;
    gl_Position = vec4(ndc * vec2(1.0, -1.0), 0.0, 1.0);
}
)";

        constexpr const char* fragmentShader = R"(#version 330 core
in vec2 v_local;
in vec2 v_card_half;
in vec2 v_shadow_half;
in vec2 v_shadow_offset;
in float v_shadow_alpha;
uniform vec4 u_color;
uniform int u_render_mode;
uniform bool u_has_texture;
uniform sampler2D u_texture;
out vec4 out_color;

float rect_mask(vec2 halfSize, vec2 pos, float softness) {
    vec2 d = abs(pos) - halfSize;
    float outside = max(max(d.x, d.y), 0.0);
    return clamp(1.0 - outside / max(softness, 0.0001), 0.0, 1.0);
}

void main() {
    float cardAlpha = rect_mask(v_card_half, v_local, 2.0);
    if (u_render_mode == 0) {
        if (cardAlpha > 0.001) {
            if (u_has_texture) {
                // Map local coordinates to texture coordinates
                vec2 texCoord = (v_local / v_card_half) * 0.5 + 0.5;
                vec4 texColor = texture(u_texture, texCoord);
                // Apply slight color tint on top
                vec3 tinted = mix(texColor.rgb, u_color.rgb, 0.1);
                out_color = vec4(tinted, texColor.a * cardAlpha);
            } else {
                out_color = vec4(u_color.rgb, u_color.a * cardAlpha);
            }
            return;
        }
        discard;
    }

    if (cardAlpha > 0.001) {
        discard;
    }

    if (v_shadow_alpha > 0.001) {
        float shadowAlpha = rect_mask(v_shadow_half, v_local - v_shadow_offset, 10.0) * v_shadow_alpha;
        if (shadowAlpha > 0.001) {
            out_color = vec4(0.0, 0.0, 0.0, shadowAlpha);
            return;
        }
    }

    discard;
}
)";

        GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShader);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShader);
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        GLint status = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status != GL_TRUE) {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            std::string log(length, '\0');
            glGetProgramInfoLog(program, length, nullptr, log.data());
            std::cerr << "Program link error: " << log << std::endl;
        }
        glDetachShader(program, vs);
        glDetachShader(program, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return program;
    }

    void drawElement(const Vec2& center,
                     const Vec2& baseHalf,
                     float depth,
                     const std::array<float, 4>& color,
                     float rotation,
                     GLuint textureId = 0,
                     bool hasTexture = false) {
        glUniform2f(centerLoc, center.x, center.y);
        glUniform2f(baseHalfLoc, baseHalf.x, baseHalf.y);
        glUniform1f(depthLoc, depth);
        glUniform4f(colorLoc, color[0], color[1], color[2], color[3]);
        glUniform1f(rotationLoc, rotation);
        glUniform1i(hasTextureLoc, hasTexture ? 1 : 0);
        
        glActiveTexture(GL_TEXTURE0);
        if (hasTexture && textureId != 0) {
            glBindTexture(GL_TEXTURE_2D, textureId);
        } else {
            glBindTexture(GL_TEXTURE_2D, dummyTexture);
        }
        glUniform1i(textureLoc, 0);
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

};

struct AppContext {
    Desk* desk = nullptr;
};

int main() {
     if (!glfwInit()) {
         std::cerr << "Failed to initialize GLFW\n";
         return 1;
     }

     glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
     glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
     glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
 #ifdef __APPLE__
     glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
 #endif

     GLFWwindow* window = glfwCreateWindow(1280, 720, "Animated Desk (C++)", nullptr, nullptr);
     if (!window) {
         std::cerr << "Failed to create window\n";
         glfwTerminate();
         return 1;
     }

     glfwMakeContextCurrent(window);
     glfwSwapInterval(1);

     // Hide system cursor
     glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

     Renderer renderer;
     int fbWidth = 0, fbHeight = 0;
     glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
     renderer.resize(fbWidth, fbHeight);

     // Initialize texture manager and load images
     TextureManager textureManager;
     textureManager.loadImagesFromDirectory("./imgs");
     
     Desk desk(&textureManager);
     
     // Assign images to elements cyclically
     auto images = textureManager.getAvailableImages();
     for (int i = 0; i < 6 && i < static_cast<int>(images.size()); ++i) {
         desk.assignImageToElement(i, images[i]);
         std::cout << "Assigned image " << images[i] << " to element " << i << std::endl;
     }
     
     AppContext context{&desk};
     glfwSetWindowUserPointer(window, &context);

    glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
        auto* ctx = static_cast<AppContext*>(glfwGetWindowUserPointer(win));
        if (ctx && ctx->desk) {
            bool consumed = ctx->desk->mouseWheel(static_cast<float>(yoffset));
            if (!consumed) {
                constexpr float scrollSpeed = 48.0f;
                Vec2 delta{
                    static_cast<float>(-xoffset) * scrollSpeed,
                    static_cast<float>(-yoffset) * scrollSpeed,
                };
                ctx->desk->scrollView(delta);
            }
        }
    });

    bool prevMouseDown = false;
    auto startTime = std::chrono::steady_clock::now();
    auto lastFrame = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        renderer.resize(fbWidth, fbHeight);

        int winWidth = 0, winHeight = 0;
        glfwGetWindowSize(window, &winWidth, &winHeight);
        double cursorX = 0.0, cursorY = 0.0;
        glfwGetCursorPos(window, &cursorX, &cursorY);
        float scaleX = (winWidth > 0) ? static_cast<float>(fbWidth) / static_cast<float>(winWidth) : 1.0f;
        float scaleY = (winHeight > 0) ? static_cast<float>(fbHeight) / static_cast<float>(winHeight) : 1.0f;
        Vec2 scaledCursorPos{
            static_cast<float>(cursorX) * scaleX,
            static_cast<float>(cursorY) * scaleY,
        };

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastFrame).count();
        dt = std::max(dt, 0.001f);
        
        desk.pointerMoved(scaledCursorPos);

        bool mouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (mouseDown && !prevMouseDown) {
            desk.pointerPressed();
        } else if (!mouseDown && prevMouseDown) {
            desk.pointerReleased();
        }
        prevMouseDown = mouseDown;

        desk.setRotationModifier(glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS);
        desk.setResizeModifier(glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS);
        desk.setZoomModifier(glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS);

        dt = std::min(dt, 0.05f);
        lastFrame = now;

        float elapsedTime = std::chrono::duration<float>(now - startTime).count();

        desk.update(dt);
         renderer.draw(desk.getElements(),
                       desk.selectedIds(),
                       desk.selectionBox(),
                       desk.selectedElementsBounds(),
                       desk.viewTranslation(),
                       desk.viewScaleValue(),
                       scaledCursorPos,
                       elapsedTime);

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
