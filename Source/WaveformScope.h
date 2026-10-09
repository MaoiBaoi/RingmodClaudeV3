#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SignalAnalysis.h"

//==============================================================================
// Zeigt beide Signale uebereinander (halbtransparent) und am Sidechain-Einsatz
// ausgerichtet, damit man sieht, ob Kick und Bass phasengleich laufen.
//   Blau   = Signal auf der Spur (z.B. Bass)
//   Orange = Sidechain (z.B. Kick)
//==============================================================================
class WaveformScope : public juce::Component
{
public:
    static inline const juce::Colour mainColour { 0xff4d8dff };
    static inline const juce::Colour sideColour { 0xffff7a45 };

    void setData (const ScopeResult& newResult, bool normaliseEach)
    {
        result = newResult;
        normalise = normaliseEach;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff111317));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

        auto area = bounds.reduced (8.0f, 22.0f);
        const float cy = area.getCentreY();
        const float halfH = area.getHeight() * 0.5f;

        // Raster: Mittellinie + 4 senkrechte Linien
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawHorizontalLine ((int) cy, area.getX(), area.getRight());
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) (cy - halfH * 0.5f), area.getX(), area.getRight());
        g.drawHorizontalLine ((int) (cy + halfH * 0.5f), area.getX(), area.getRight());

        for (int k = 1; k < 5; ++k)
            g.drawVerticalLine ((int) (area.getX() + area.getWidth() * (float) k / 5.0f), area.getY(), area.getBottom());

        // Legende
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (mainColour);
        g.drawText ("SIGNAL", juce::Rectangle<float> (bounds.getX() + 10.0f, bounds.getY() + 4.0f, 60.0f, 16.0f),
                    juce::Justification::centredLeft);
        g.setColour (sideColour);
        g.drawText ("SIDECHAIN", juce::Rectangle<float> (bounds.getX() + 66.0f, bounds.getY() + 4.0f, 80.0f, 16.0f),
                    juce::Justification::centredLeft);

        if (result.main.empty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawText ("Warte auf Audio ...", bounds, juce::Justification::centred);
            return;
        }

        const int n = (int) result.main.size();

        // Skalierung
        const float pm = peakOf (result.main), ps = peakOf (result.side);
        float scaleMain, scaleSide;

        if (normalise)
        {
            scaleMain = pm > 1e-6f ? 1.0f / pm : 0.0f;
            scaleSide = ps > 1e-6f ? 1.0f / ps : 0.0f;
        }
        else
        {
            const float p = juce::jmax (pm, ps);
            scaleMain = scaleSide = p > 1e-6f ? 1.0f / p : 0.0f;
        }

        // Einsatz-Markierung und Zeitbeschriftung
        const float windowMs = result.windowMs;
        const float zeroX = result.triggered ? area.getX() + area.getWidth() * (float) result.triggerIndex / (float) n
                                             : area.getX();

        if (result.triggered)
        {
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.drawVerticalLine ((int) zeroX, area.getY(), area.getBottom());
        }

        g.setFont (juce::FontOptions (10.0f));
        g.setColour (juce::Colours::white.withAlpha (0.45f));

        for (int k = 0; k <= 5; ++k)
        {
            const float x = area.getX() + area.getWidth() * (float) k / 5.0f;
            const float ms = (x - zeroX) / area.getWidth() * windowMs;
            auto labelArea = juce::Rectangle<float> (x - 28.0f, bounds.getBottom() - 18.0f, 56.0f, 14.0f);
            auto just = juce::Justification::centred;

            if (k == 0) { labelArea.setX (x);          just = juce::Justification::centredLeft; }
            if (k == 5) { labelArea.setX (x - 56.0f);  just = juce::Justification::centredRight; }

            g.drawText (juce::String (juce::roundToInt (ms)) + " ms", labelArea, just);
        }

        drawTrace (g, result.main, scaleMain, area, mainColour);
        drawTrace (g, result.side, scaleSide, area, sideColour);

        // Phasen-Anzeige
        const float c = result.correlation;
        juce::String text;
        juce::Colour colour;

        if (c >= 0.6f)        { text = "in Phase";           colour = juce::Colour (0xff5fd075); }
        else if (c >= 0.2f)   { text = "teilweise in Phase"; colour = juce::Colour (0xffe0c050); }
        else if (c > -0.2f)   { text = "unkorreliert";       colour = juce::Colour (0xffe0a050); }
        else                  { text = "GEGENPHASIG";        colour = juce::Colour (0xffe05555); }

        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.setColour (colour);
        g.drawText ("PHASE " + juce::String (c, 2) + "  " + text,
                    juce::Rectangle<float> (bounds.getRight() - 260.0f, bounds.getY() + 4.0f, 250.0f, 16.0f),
                    juce::Justification::centredRight);
    }

private:
    static float peakOf (const std::vector<float>& v)
    {
        float p = 0.0f;
        for (float x : v) p = juce::jmax (p, std::abs (x));
        return p;
    }

    static void drawTrace (juce::Graphics& g, const std::vector<float>& v, float scale,
                           juce::Rectangle<float> area, juce::Colour colour)
    {
        const int n = (int) v.size();
        const int cols = juce::jmax (2, (int) area.getWidth());
        const float cy = area.getCentreY(), halfH = area.getHeight() * 0.5f;

        std::vector<float> tops ((size_t) cols), bottoms ((size_t) cols);

        for (int x = 0; x < cols; ++x)
        {
            const int i0 = (int) ((int64_t) x * n / cols);
            const int i1 = juce::jmax (i0 + 1, (int) ((int64_t) (x + 1) * n / cols));
            float lo = v[(size_t) i0], hi = v[(size_t) i0];

            for (int i = i0; i < i1 && i < n; ++i)
            {
                lo = juce::jmin (lo, v[(size_t) i]);
                hi = juce::jmax (hi, v[(size_t) i]);
            }

            tops[(size_t) x]    = cy - hi * scale * halfH;
            bottoms[(size_t) x] = cy - lo * scale * halfH;
        }

        juce::Path p;
        p.startNewSubPath (area.getX(), tops[0]);

        for (int x = 1; x < cols; ++x)
            p.lineTo (area.getX() + (float) x, tops[(size_t) x]);

        for (int x = cols - 1; x >= 0; --x)
            p.lineTo (area.getX() + (float) x, bottoms[(size_t) x]);

        p.closeSubPath();

        g.setColour (colour.withAlpha (0.28f));
        g.fillPath (p);
        g.setColour (colour.withAlpha (0.85f));
        g.strokePath (p, juce::PathStrokeType (1.1f));
    }

    ScopeResult result;
    bool normalise = true;
};
