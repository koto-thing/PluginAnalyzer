#pragma once

#include <JuceHeader.h>

class SSLLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SSLLookAndFeel()
    {
        // Color Palette
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff1a1a1a));
        
        // Button colors
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00a0ff));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc0c0c0));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        
        // Slider colors
        setColour(juce::Slider::thumbColourId, juce::Colour(0xff00a0ff));
        setColour(juce::Slider::trackColourId, juce::Colour(0xff404040));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff2d2d2d));
        setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff404040));
        
        // Label colors
        setColour(juce::Label::textColourId, juce::Colour(0xffc0c0c0));
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        
        // TabbedComponent colors
        setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::TabbedComponent::outlineColourId, juce::Colour(0xff404040));
        setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colour(0xff404040));
        setColour(juce::TabbedButtonBar::frontOutlineColourId, juce::Colour(0xff00a0ff));
        setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colour(0xff808080));
        setColour(juce::TabbedButtonBar::frontTextColourId, juce::Colours::white);
    }
    
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
        auto baseColour = backgroundColour.withMultipliedSaturation(button.hasKeyboardFocus(true) ? 1.3f : 0.9f)
                                          .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);

        if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
            baseColour = baseColour.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.05f);

        // SSL-style gradient
        auto gradient = juce::ColourGradient::vertical(
            baseColour.brighter(0.1f), bounds.getY(),
            baseColour.darker(0.2f), bounds.getBottom()
        );
        
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(bounds, 4.0f);

        // Highlight edge
        g.setColour(baseColour.brighter(0.3f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 1.0f);
        
        // Outer border
        g.setColour(juce::Colour(0xff404040));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }
    
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          juce::Slider::SliderStyle /*style*/, juce::Slider& slider) override
    {
        auto trackWidth = juce::jmin(6.0f, slider.isHorizontal() ? height * 0.25f : width * 0.25f);
        
        juce::Point<float> startPoint(slider.isHorizontal() ? x : x + width * 0.5f,
                                      slider.isHorizontal() ? y + height * 0.5f : height + y);
        
        juce::Point<float> endPoint(slider.isHorizontal() ? width + x : startPoint.x,
                                    slider.isHorizontal() ? startPoint.y : y);
        
        juce::Path backgroundTrack;
        backgroundTrack.startNewSubPath(startPoint);
        backgroundTrack.lineTo(endPoint);
        g.setColour(juce::Colour(0xff2d2d2d));
        g.strokePath(backgroundTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        
        juce::Path valueTrack;
        juce::Point<float> minPoint, maxPoint, thumbPoint;
        
        if (slider.isHorizontal())
        {
            minPoint = startPoint;
            maxPoint = { sliderPos, y + height * 0.5f };
            thumbPoint = maxPoint;
        }
        else
        {
            minPoint = { x + width * 0.5f, sliderPos };
            maxPoint = endPoint;
            thumbPoint = minPoint;
        }
        
        auto kx = slider.isHorizontal() ? sliderPos : (x + width * 0.5f);
        auto ky = slider.isHorizontal() ? (y + height * 0.5f) : sliderPos;
        
        valueTrack.startNewSubPath(minPoint);
        valueTrack.lineTo(slider.isHorizontal() ? maxPoint : minPoint);
        g.setColour(juce::Colour(0xff00a0ff));
        g.strokePath(valueTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        
        auto thumbWidth = getSliderThumbRadius(slider);
        
        // Glow effect
        g.setColour(juce::Colour(0xff00a0ff).withAlpha(0.3f));
        g.fillEllipse(juce::Rectangle<float>(thumbWidth * 2.5f, thumbWidth * 2.5f).withCentre({ kx, ky }));
        
        // Main
        auto thumbGradient = juce::ColourGradient::vertical(
            juce::Colour(0xff4d4d4d), ky - thumbWidth,
            juce::Colour(0xff1a1a1a), ky + thumbWidth
        );
        g.setGradientFill(thumbGradient);
        g.fillEllipse(juce::Rectangle<float>(thumbWidth * 2.0f, thumbWidth * 2.0f).withCentre({ kx, ky }));
        
        // indicator
        g.setColour(juce::Colour(0xff00a0ff));
        g.fillEllipse(juce::Rectangle<float>(thumbWidth * 1.2f, thumbWidth * 1.2f).withCentre({ kx, ky }));
        
        // border
        g.setColour(juce::Colour(0xff808080));
        g.drawEllipse(juce::Rectangle<float>(thumbWidth * 2.0f, thumbWidth * 2.0f).withCentre({ kx, ky }), 1.0f);
    }
    
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto fontSize = juce::jmin(15.0f, button.getHeight() * 0.75f);
        auto tickWidth = fontSize * 1.2f;
        
        drawTickBox(g, button, 4.0f, (button.getHeight() - tickWidth) * 0.5f,
                    tickWidth, tickWidth, button.getToggleState(),
                    button.isEnabled(), shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        
        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(fontSize);
        
        if (!button.isEnabled())
            g.setOpacity(0.5f);
        
        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds().withTrimmedLeft(juce::roundToInt(tickWidth) + 10)
                                                .withTrimmedRight(2),
                         juce::Justification::centredLeft, 10);
    }
    
    void drawTickBox(juce::Graphics& g, juce::Component& /*component*/,
                     float x, float y, float w, float h,
                     bool ticked, bool /*isEnabled*/,
                     bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override
    {
        juce::Rectangle<float> tickBounds(x, y, w, h);
        
        // indicator
        auto gradient = juce::ColourGradient::vertical(
            juce::Colour(0xff2d2d2d), tickBounds.getY(),
            juce::Colour(0xff1a1a1a), tickBounds.getBottom()
        );
        
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(tickBounds, 3.0f);
        
        g.setColour(juce::Colour(0xff404040));
        g.drawRoundedRectangle(tickBounds, 3.0f, 1.0f);
        
        if (ticked)
        {
            auto ledBounds = tickBounds.reduced(4.0f);
            
            // Glow
            g.setColour(juce::Colour(0xff00a0ff).withAlpha(0.5f));
            g.fillEllipse(ledBounds.expanded(2.0f));
            
            g.setColour(juce::Colour(0xff00a0ff));
            g.fillEllipse(ledBounds);
            
            // Highlight
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.fillEllipse(ledBounds.reduced(ledBounds.getWidth() * 0.3f).translated(0, -ledBounds.getHeight() * 0.1f));
        }
    }
    
    juce::Font getLabelFont(juce::Label& /*label*/) override
    {
        return juce::Font(juce::FontOptions("Arial", 13.0f, juce::Font::plain));
    }
    
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return juce::Font(juce::FontOptions("Arial", juce::jmin(15.0f, buttonHeight * 0.6f), juce::Font::bold));
    }
    
    int getSliderThumbRadius(juce::Slider& slider) override
    {
        return juce::jmin(12, slider.isHorizontal() ? slider.getHeight() / 2 : slider.getWidth() / 2);
    }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SSLLookAndFeel)
};