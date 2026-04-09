#pragma once

#include "source.hpp"
#include <string>

namespace sources {

/**
 * @brief Static image source — loads a PNG/JPG/etc. file and exposes it
 * as a GL texture. The texture never changes after initialization.
 */
class ImageFileSource : public Source {
public:
    explicit ImageFileSource(const std::string& filePath);
    ~ImageFileSource() { shutdown(); }

    bool        initialize()                  override;
    void        shutdown()                    override;
    bool        update(float /*deltaTime*/)   override { return true; }

    unsigned int getTextureHandle()   const override { return textureHandle_; }
    Vec2         getResolution()      const override { return resolution_; }
    bool         isValid()            const override { return textureHandle_ != 0; }
    SourceType   getType()            const override { return SourceType::IMAGE_FILE; }
    std::string  getName()            const override;

    const std::string& getFilePath()  const { return filePath_; }

private:
    std::string  filePath_;
    unsigned int textureHandle_ = 0;
    Vec2         resolution_    = {};
};

} // namespace sources
