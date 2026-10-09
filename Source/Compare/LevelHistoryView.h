#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

//==============================================================================
// Zeigt die Lautstaerke beider Signale als zwei halbtransparente Linien.
// Neueste Werte rechts, die Zeit laeuft nach links. 0 dB liegt oben.
//   Blau   = Signal auf der Spur
//   Orange = Sidechain
//==============================================================================
class LevelHistoryView : public juce::Component
{
public:
    static inline const juce::Colour mainColour { 0xff4d8dff };
    static inline const juce::Colour sideColour { 0xffff7a45 };

    static constexpr float minDb = -60.0f;

    void setData (std::vector<float> newMain, std::vector<float> newSide, float windowSeconds)
    {
        mainDb = std::move (newMain);
        sideDb = std::move (newSide);
        seconds = windowSeconds;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff111317));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

        // Zeichenflaeche: rechts Platz fuer die dB-Skala, unten fuer die Zeit
        auto plot = bounds.reduced (10.0f, 10.0f);
        plot.removeFromRight (46.0f);
        plot.removeFromBottom (40.0f);

        auto yFor = [&] (float db)
        {
            const float c = juce::jlimit (minDb, 0.0f, db);
            return plot.getY() + plot.getHeight() * (c / minDb);   // 0 dB oben, -60 dB unten
        };

        // dB-Raster und Skala (rechts)
        g.setFont (juce::FontOptions (11.0f));

        for (int db = 0; db >= (int) minDb; db -= 10)
        {
            const float y = yFor ((float) db);
            g.setColour (juce::Colours::white.withAlpha (db == 0 ? 0.20f : 0.07f));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawText (db == 0 ? "-0 dB" : juce::String (db) + " dB",
                        juce::Rectangle<float> (plot.getRight() + 4.0f, y - 8.0f, 44.0f, 16.0f),
                        juce::Justification::centredLeft);
        }

        // Zeitraster (unten)
        for (int k = 0; k <= 5; ++k)
        {
            const float x = plot.getRight() - plot.getWidth() * (float) k / 5.0f;
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());

            const float t = seconds * (float) k / 5.0f;
            const juce::String text = k == 0 ? juce::String ("jetzt") : "-" + juce::String (t, t < 10.0f ? 1 : 0) + " s";

            auto labelArea = juce::Rectangle<float> (x - 26.0f, plot.getBottom() + 3.0f, 52.0f, 14.0f);
            auto just = juce::Justification::centred;

            if (k == 5) { labelArea.setX (x - 2.0f);  just = juce::Justification::centredLeft; }
            if (k == 0) { labelArea.setX (x - 50.0f); just = juce::Justification::centredRight; }

            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawText (text, labelArea, just);
        }

        // Pfeil "Zeit" (die Zeit laeuft von rechts nach links)
        {
            const float ay = plot.getBottom() + 28.0f;
            const float cx = plot.getCentreX();
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawLine (cx + 36.0f, ay, cx - 36.0f, ay, 1.5f);

            juce::Path head;
            head.addTriangle (cx - 44.0f, ay, cx - 34.0f, ay - 5.0f, cx - 34.0f, ay + 5.0f);
            g.fillPath (head);

            g.setFont (juce::FontOptions (12.0f));
            g.drawText ("Zeit", juce::Rectangle<float> (cx + 42.0f, ay - 8.0f, 40.0f, 16.0f), juce::Justification::centredLeft);
        }

        if (mainDb.empty() || sideDb.empty())
            return;

        drawLine (g, mainDb, plot, yFor, mainColour);
        drawLine (g, sideDb, plot, yFor, sideColour);
    }

private:
    template <typename YFunc>
    static void drawLine (juce::Graphics& g, const std::vector<float>& db, juce::Rectangle<float> plot,
                          YFunc yFor, juce::Colour colour)
    {
        const int n = (int) db.size();
        const int cols = juce::jmax (2, (int) plot.getWidth());

        juce::Path line;

        for (int x = 0; x < cols; ++x)
        {
            const int i0 = (int) ((int64_t) x * n / cols);
            const int i1 = juce::jmax (i0 + 1, (int) ((int64_t) (x + 1) * n / cols));
            float v = db[(size_t) i0];

            for (int i = i0; i < i1 && i < n; ++i)
                v = juce::jmax (v, db[(size_t) i]);

            const float px = plot.getX() + (float) x * plot.getWidth() / (float) (cols - 1);
            const float py = yFor (v);

            if (x == 0) line.startNewSubPath (px, py);
            else        line.lineTo (px, py);
        }

        juce::Path fill (line);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();

        g.setColour (colour.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (colour.withAlpha (0.90f));
        g.strokePath (line, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
    }

    std::vector<float> mainDb, sideDb;
    float seconds = 10.0f;
};
