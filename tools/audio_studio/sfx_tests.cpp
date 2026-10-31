#include "session.h"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>

namespace AudioStudio {
namespace {
class SfxSessionTest : public testing::Test {
  protected:
    fs::path directory;
    std::unique_ptr<Session> session;

    void SetUp() override {
        directory =
            fs::temp_directory_path() /
            ("gpp-sfx-session-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        audio::DeviceSettings device;
        device.device = 0;
        session = std::make_unique<Session>(device, directory);
    }

    void TearDown() override {
        session.reset();
        fs::remove_all(directory);
    }
};
} // namespace

TEST_F(SfxSessionTest, GeneratesWithoutMusicAndPreservesLastValidResult) {
    auto &s = *session;
    ASSERT_TRUE(s.source.empty());
    s.generateSfx();
    s.wait();
    ASSERT_TRUE(s.sfxProduct);
    EXPECT_FALSE(s.sfxStale);
    EXPECT_FALSE(s.product);
    const auto previous = s.sfxProduct;
    const auto definition = audio::sfx::serialize(s.sfxDefinition);

    std::ofstream(directory / "broken.lua")
        << "return {schemaVersion=1,id='bad',duration=0/0}";
    s.loadSfx(directory / "broken.lua");
    EXPECT_THROW(s.wait(), audio::Error);
    EXPECT_EQ(s.sfxProduct, previous);
    EXPECT_EQ(audio::sfx::serialize(s.sfxDefinition), definition);
    EXPECT_TRUE(s.sfxPath.empty());

    s.sfxDefinition.duration = -1;
    s.editSfx();
    EXPECT_THROW(s.generateSfx(), audio::Error);
    EXPECT_EQ(s.sfxProduct, previous);
    EXPECT_TRUE(s.sfxStale);
}

TEST_F(SfxSessionTest, CancellationAndDraftEditsDoNotPublishWrongSnapshot) {
    auto &s = *session;
    s.generateSfx();
    s.wait();
    const auto previous = s.sfxProduct;
    s.sfxDefinition.duration = 25;
    s.sfxDefinition.layers[0].waveform = audio::sfx::Wave::Triangle;
    s.editSfx();
    s.generateSfx();
    s.cancel();
    s.tick();
    EXPECT_EQ(s.sfxProduct, previous);

    s.sfxDefinition.duration = .1;
    s.generateSfx();
    s.sfxDefinition.seed = 99;
    s.editSfx();
    s.wait();
    EXPECT_TRUE(s.sfxStale);
    EXPECT_NE(s.sfxProduct->definition.seed, 99u);
    s.generateSfx();
    s.wait();
    EXPECT_FALSE(s.sfxStale);
    EXPECT_EQ(s.sfxProduct->definition.seed, 99u);
}

TEST_F(SfxSessionTest, SaveLoadExportAndNoClobber) {
    auto &s = *session;
    s.saveSfx("recipe.lua");
    s.sfxDefinition.seed = 1;
    s.loadSfx(directory / "recipe.lua");
    s.wait();
    EXPECT_EQ(s.sfxDefinition.seed, 12031u);
    s.sfxDefinition.seed = 24;
    s.saveSfx("recipe.lua");
    EXPECT_EQ(audio::sfx::load(directory / "recipe.lua").seed, 24u);

    std::ofstream(directory / "third-party.lua") << "preserve this";
    EXPECT_THROW(s.saveSfx("third-party.lua"), audio::Error);
    s.generateSfx();
    s.wait();
    s.exportSfx("result.wav", false);
    s.wait();
    const auto bytes = fs::file_size(directory / "result.wav");
    s.exportSfx("result.wav", true);
    EXPECT_THROW(s.wait(), audio::Error);
    EXPECT_EQ(fs::file_size(directory / "result.wav"), bytes);
    EXPECT_THROW(s.exportSfx("../outside.wav", true), audio::Error);
    EXPECT_TRUE(s.sfxProduct);
}

TEST_F(SfxSessionTest, PreviewLoopsDeviceRecreationAndIndependentFailSynth) {
    auto &s = *session;
    s.sfxDefinition.loop = {true, 0, 0, .02};
    s.sfxDefinition.layers[0].envelope = {0, 0, 1, 0, false};
    s.generateSfx();
    s.wait();
    EXPECT_NO_THROW(s.playSfx(true));
    EXPECT_NE(s.sfxPreview.handle(), 0u);
    EXPECT_EQ(s.preview.handle(), 0u);
    EXPECT_EQ(s.sfxPreview.loopError(), 0);
    EXPECT_NE(s.notice.find("No Sound"), std::string::npos);
    EXPECT_NO_THROW(s.stopSfx());
    EXPECT_NO_THROW(s.applyDevice(s.deviceSettings));
    EXPECT_EQ(s.sfxPreview.handle(), 0u);
    ASSERT_TRUE(s.sfxProduct);
    EXPECT_NO_THROW(s.playSfx(false));
    EXPECT_NO_THROW(s.stopSfx());
}

TEST_F(SfxSessionTest, UnsavedDraftRequiresSaveOrExplicitDiscard) {
    auto &s = *session;
    s.saveSfx("first.lua");
    auto other = s.sfxDefinition;
    other.id = "other";
    audio::sfx::save(directory / "other.lua", other);

    s.sfxDefinition.seed = 99;
    s.editSfx();
    s.generateSfx();
    s.wait();
    EXPECT_TRUE(s.sfxDirty);
    EXPECT_FALSE(s.sfxStale);
    const auto product = s.sfxProduct;
    EXPECT_THROW(s.loadSfx(directory / "other.lua"), audio::Error);
    EXPECT_THROW(s.newSfx(), audio::Error);
    EXPECT_EQ(s.sfxDefinition.seed, 99u);
    EXPECT_EQ(s.sfxProduct, product);

    EXPECT_THROW(s.saveSfx("other.lua"), audio::Error);
    EXPECT_TRUE(s.sfxDirty);
    s.loadSfx(directory / "missing.lua", true);
    EXPECT_THROW(s.wait(), audio::Error);
    EXPECT_TRUE(s.sfxDirty);
    EXPECT_EQ(s.sfxDefinition.seed, 99u);

    s.saveSfx("first.lua");
    EXPECT_FALSE(s.sfxDirty);
    EXPECT_EQ(audio::sfx::load(directory / "first.lua").seed, 99u);
    s.loadSfx(directory / "other.lua");
    s.wait();
    EXPECT_FALSE(s.sfxDirty);
    EXPECT_EQ(s.sfxDefinition.id, "other");

    s.newSfx();
    EXPECT_TRUE(s.sfxDirty);
    s.loadSfx(directory / "first.lua", true);
    s.wait();
    EXPECT_FALSE(s.sfxDirty);
    EXPECT_EQ(s.sfxDefinition.seed, 99u);
    EXPECT_EQ(s.sfxProduct, product);
}

TEST_F(SfxSessionTest, PreparedPreviewRetainsBytesUntilTransportCloses) {
    auto &s = *session;
    s.generateSfx();
    s.wait();
    ASSERT_TRUE(s.sfxProduct->previewWav);
    std::weak_ptr<const std::vector<uint8_t>> bytes = s.sfxProduct->previewWav;
    s.playSfx(false);
    const auto handle = s.sfxPreview.handle();

    EXPECT_THROW(s.sfxPreview.openWav(nullptr), audio::Error);
    EXPECT_THROW(s.sfxPreview.openWav(
                     std::make_shared<const std::vector<uint8_t>>(16, 0)),
                 audio::Error);
    EXPECT_EQ(s.sfxPreview.handle(), handle);
    s.sfxProduct.reset();
    EXPECT_FALSE(bytes.expired());
    EXPECT_NO_THROW(s.sfxPreview.position(.1));
    EXPECT_NO_THROW(s.sfxPreview.play());
    s.sfxPreview.close();
    EXPECT_TRUE(bytes.expired());
}
} // namespace AudioStudio
