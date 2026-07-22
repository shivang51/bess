#include "ui_core.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

namespace {
    using namespace Bess;
    using namespace Bess::UI;

    class TestPlatformServices final : public UIPlatformServices {
      public:
        std::optional<std::string> readClipboardText() const override {
            return clipboard;
        }

        bool writeClipboardText(std::string_view text) override {
            clipboard = std::string{text};
            return true;
        }

        void beginTextInput() override {
            ++begins;
        }
        void updateTextInputArea(WidgetBounds bounds) override {
            caret = bounds;
            ++areaUpdates;
        }
        void endTextInput() override {
            ++ends;
        }

        mutable std::string clipboard;
        WidgetBounds caret;
        size_t begins = 0;
        size_t areaUpdates = 0;
        size_t ends = 0;
    };

    class TextPainter final : public UIPainter {
      public:
        glm::vec2 viewportSize() const noexcept override {
            return {400.f, 100.f};
        }

        void drawBox(const BoxPaint &paint) override {
            boxes.push_back(paint);
        }

        void drawText(std::string_view, const TextPaint &) override {
        }

        glm::vec2 measureText(std::string_view text,
                              float fontSize,
                              float letterSpacing) const override {
            ++measureCalls;
            return {static_cast<float>(text.size()) *
                        (fontSize * 0.6f + letterSpacing),
                    fontSize};
        }

        void pushClip(WidgetBounds) override {
        }

        void popClip() override {
        }

        std::vector<BoxPaint> boxes;
        mutable size_t measureCalls = 0;
    };

    UIEvent key(KeyCode code, bool shift = false, bool control = false) {
        return UIEvent{Input::KeyEvent{.key = code, .action = KeyAction::press},
                       {.control = control, .shift = shift}};
    }

    TEST(TextEditModelTests, NavigatesExtendedGraphemeClusters) {
        const std::string combining = "a\xCC\x81"
                                      "b";
        EXPECT_EQ(TextEditModel::nextGraphemeBoundary(combining, 0), 3u);
        EXPECT_EQ(TextEditModel::previousGraphemeBoundary(combining, 3), 0u);
        EXPECT_EQ(TextEditModel::previousGraphemeBoundary(combining, 4), 3u);

        const std::string zwj = "\xF0\x9F\x91\xA9\xE2\x80\x8D"
                                "\xF0\x9F\x92\xBBx";
        EXPECT_EQ(TextEditModel::nextGraphemeBoundary(zwj, 0), zwj.size() - 1);
        EXPECT_EQ(TextEditModel::previousGraphemeBoundary(zwj, zwj.size() - 1),
                  0u);

        const std::string flag = "\xF0\x9F\x87\xAE\xF0\x9F\x87\xB3!";
        EXPECT_EQ(TextEditModel::nextGraphemeBoundary(flag, 0), 8u);
    }

    TEST(TextEditModelTests, EditsSelectionAndOwnsBoundedUndoRedo) {
        TextEditModel model{"hello world"};
        ASSERT_TRUE(model.selectWordAt(7));
        EXPECT_EQ(model.selectedText(), "world");
        ASSERT_TRUE(model.replaceSelection("Bess"));
        EXPECT_EQ(model.text(), "hello Bess");
        EXPECT_TRUE(model.canUndo());
        ASSERT_TRUE(model.undo());
        EXPECT_EQ(model.text(), "hello world");
        EXPECT_EQ(model.selectedText(), "world");
        ASSERT_TRUE(model.redo());
        EXPECT_EQ(model.text(), "hello Bess");

        TextEditModel bounded{"", 3};
        EXPECT_FALSE(bounded.insertText("\xF0\x9F\x92\xBB"));
        EXPECT_TRUE(bounded.text().empty());
        EXPECT_TRUE(bounded.insertText("a\xCC\x81"));
        EXPECT_EQ(bounded.text().size(), 3u);
    }

    TEST(TextEditModelTests,
         RejectsOversizedGraphemeWithoutDeletingTheSelection) {
        TextEditModel model{"abc", 3};
        ASSERT_TRUE(model.setSelection(1, 2));
        EXPECT_FALSE(model.replaceSelection("\xF0\x9F\x92\xBB"));
        EXPECT_EQ(model.text(), "abc");
        EXPECT_EQ(model.selectedText(), "b");

        ASSERT_TRUE(model.beginComposition());
        EXPECT_FALSE(model.updateComposition("\xF0\x9F\x92\xBB", 4, 0));
        EXPECT_EQ(model.text(), "abc");
        EXPECT_TRUE(model.hasComposition());
        EXPECT_TRUE(model.commitComposition("\xF0\x9F\x92\xBB"));
        EXPECT_EQ(model.text(), "abc");
        EXPECT_EQ(model.selectedText(), "b");
        EXPECT_FALSE(model.hasComposition());
    }

    TEST(TextEditModelTests, CompositionCommitsAsOneUndoUnitAndCancels) {
        TextEditModel model{"A"};
        ASSERT_TRUE(model.setCaret(0));
        ASSERT_TRUE(model.moveToEnd());
        ASSERT_TRUE(model.beginComposition());
        ASSERT_TRUE(model.updateComposition("e\xCC\x81", 3, 0));
        ASSERT_TRUE(model.hasComposition());
        ASSERT_TRUE(model.commitComposition());
        EXPECT_EQ(model.text(), "Ae\xCC\x81");
        ASSERT_TRUE(model.undo());
        EXPECT_EQ(model.text(), "A");

        ASSERT_TRUE(model.beginComposition());
        ASSERT_TRUE(model.updateComposition("temporary", 9, 0));
        ASSERT_TRUE(model.cancelComposition());
        EXPECT_EQ(model.text(), "A");
        EXPECT_FALSE(model.hasComposition());
    }

    TEST(TextBoxTests, UsesPlatformClipboardAndImeServices) {
        WidgetTree tree;
        tree.setViewportSize({400.f, 100.f});
        auto services = std::make_shared<TestPlatformServices>();
        services->clipboard = "pasted";
        tree.setPlatformServices(services);
        auto model = std::make_shared<TextEditModel>("old");
        size_t changes = 0;
        const auto textBox = tree.emplaceWidget<TextBox>(
            model, [&changes](const std::string &) { ++changes; });
        tree.performLayout();

        ASSERT_TRUE(tree.setFocus(textBox));
        EXPECT_EQ(services->begins, 1u);
        TextPainter painter;
        tree.paint(painter);
        EXPECT_EQ(services->areaUpdates, 1u);
        EXPECT_GE(services->caret.topLeft().x, 0.f);
        EXPECT_LE(services->caret.bottomRight().x, tree.getViewportSize().x);
        EXPECT_GE(services->caret.topLeft().y, 0.f);
        EXPECT_LE(services->caret.bottomRight().y, tree.getViewportSize().y);
        static_cast<void>(tree.dispatchEvent(key(KeyCode::a, false, true)));
        static_cast<void>(tree.dispatchEvent(key(KeyCode::v, false, true)));
        EXPECT_EQ(model->text(), "pasted");
        EXPECT_EQ(changes, 1u);

        static_cast<void>(
            tree.dispatchEvent(UIEvent{Input::TextCompositionEvent{
                .phase = Input::TextCompositionPhase::begin}}));
        static_cast<void>(
            tree.dispatchEvent(UIEvent{Input::TextCompositionEvent{
                .phase = Input::TextCompositionPhase::update,
                .text = "!",
                .selectionStart = 1}}));
        static_cast<void>(
            tree.dispatchEvent(UIEvent{Input::TextCompositionEvent{
                .phase = Input::TextCompositionPhase::commit, .text = "!"}}));
        EXPECT_EQ(model->text(), "pasted!");
        EXPECT_FALSE(model->hasComposition());

        ASSERT_TRUE(model->selectAll());
        static_cast<void>(tree.dispatchEvent(key(KeyCode::c, false, true)));
        EXPECT_EQ(services->clipboard, "pasted!");
        tree.clearFocus();
        EXPECT_EQ(services->ends, 1u);
    }

    TEST(TextBoxTests, ReadOnlyStillAllowsSelectionAndCopy) {
        WidgetTree tree;
        tree.setViewportSize({300.f, 80.f});
        auto services = std::make_shared<TestPlatformServices>();
        tree.setPlatformServices(services);
        auto model = std::make_shared<TextEditModel>("immutable");
        const auto textBox =
            tree.emplaceWidget<TextBox>(model,
                                        TextBox::Changed{},
                                        TextBox::Submitted{},
                                        TextBoxOptions{.readOnly = true});
        ASSERT_TRUE(tree.setFocus(textBox));
        static_cast<void>(tree.dispatchEvent(key(KeyCode::a, false, true)));
        static_cast<void>(tree.dispatchEvent(key(KeyCode::c, false, true)));
        static_cast<void>(
            tree.dispatchEvent(Input::TextInputEvent{.codepoint = U'X'}));
        static_cast<void>(
            tree.dispatchEvent(UIEvent{Input::TextCompositionEvent{
                .phase = Input::TextCompositionPhase::begin}}));
        EXPECT_EQ(model->text(), "immutable");
        EXPECT_EQ(services->clipboard, "immutable");
        EXPECT_FALSE(model->hasComposition());
    }

    TEST(TextBoxTests, CaretAtStartRemainsFullyInsideTheContentClip) {
        WidgetTree tree;
        tree.setViewportSize({240.f, 80.f});
        const auto model = std::make_shared<TextEditModel>();
        const auto textBox = tree.emplaceWidget<TextBox>(model);
        tree.performLayout();
        ASSERT_TRUE(tree.setFocus(textBox));

        TextPainter painter;
        tree.paint(painter);
        ASSERT_GE(painter.boxes.size(), 2u);
        const auto &caret = painter.boxes.back().bounds;
        const auto bounds = tree.getBounds(textBox);
        const auto &padding = tree.theme().textBox.padding;
        const float contentLeft = bounds.topLeft().x + padding.x;
        EXPECT_FLOAT_EQ(caret.topLeft().x, contentLeft);
        EXPECT_GT(caret.size.x, 0.f);
        EXPECT_LE(caret.bottomRight().x, bounds.bottomRight().x - padding.x);
    }

    TEST(TextBoxTests, ReusesCaretMetricsUntilTextOrStyleChanges) {
        WidgetTree tree;
        tree.setViewportSize({300.f, 80.f});
        const auto model = std::make_shared<TextEditModel>("retained");
        const auto textBox = tree.emplaceWidget<TextBox>(model);
        tree.performLayout();

        TextPainter painter;
        tree.paint(painter);
        const size_t initialMeasurements = painter.measureCalls;
        EXPECT_GT(initialMeasurements, 0u);

        tree.paint(painter);
        EXPECT_EQ(painter.measureCalls, initialMeasurements);

        ASSERT_TRUE(model->insertText(" UI"));
        tree.paint(painter);
        EXPECT_GT(painter.measureCalls, initialMeasurements);
    }

} // namespace
