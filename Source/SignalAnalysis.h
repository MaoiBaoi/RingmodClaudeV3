#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

//==============================================================================
// Ringpuffer mit den letzten Samples beider Signale (Hauptsignal + Sidechain).
// Der Audio-Thread schreibt, die Oberflaeche liest. Nur fuer Anzeigezwecke.
//==============================================================================
class SignalRecorder
{
public:
    static constexpr int size = 1 << 17;      // ca. 2,7 s bei 48 kHz
    static constexpr int mask = size - 1;

    SignalRecorder() { clear(); }

    void clear() noexcept
    {
        for (auto& v : mainData) v.store (0.0f, std::memory_order_relaxed);
        for (auto& v : sideData) v.store (0.0f, std::memory_order_relaxed);
        writePos.store (0, std::memory_order_release);
    }

    int64_t getWritePos() const noexcept { return writePos.load (std::memory_order_acquire); }

    void write (int64_t pos, float mainSample, float sideSample) noexcept
    {
        const auto idx = (size_t) (pos & mask);
        mainData[idx].store (mainSample, std::memory_order_relaxed);
        sideData[idx].store (sideSample, std::memory_order_relaxed);
    }

    void publish (int64_t newPos) noexcept { writePos.store (newPos, std::memory_order_release); }

    float getMain (int64_t pos) const noexcept { return mainData[(size_t) (pos & mask)].load (std::memory_order_relaxed); }
    float getSide (int64_t pos) const noexcept { return sideData[(size_t) (pos & mask)].load (std::memory_order_relaxed); }

private:
    std::array<std::atomic<float>, size> mainData, sideData;
    std::atomic<int64_t> writePos { 0 };
};

//==============================================================================
// Ergebnis der Wellenform-Analyse: beide Signale, zeitlich am Sidechain-Einsatz
// (z.B. Kick) ausgerichtet, plus Phasen-Korrelation (-1 .. +1).
//==============================================================================
struct ScopeResult
{
    std::vector<float> main, side;
    float   correlation  = 0.0f;
    bool    triggered    = false;
    int     triggerIndex = -1;       // Position des Einsatzes im Fenster
    int64_t triggerPos   = -1;       // absolute Position (zum Wiedererkennen)
    float   windowMs     = 100.0f;
};

class ScopeAnalyzer
{
public:
    // Gibt true zurueck, wenn sich 'result' geaendert hat.
    static bool analyse (const SignalRecorder& rec, double sampleRate, int windowSamples, ScopeResult& result)
    {
        if (windowSamples < 32 || sampleRate <= 0.0)
            return false;

        const int searchSamples = (int) (sampleRate * 0.6);
        const int total = std::min (windowSamples + searchSamples, SignalRecorder::size - 8);
        const int64_t end = rec.getWritePos();

        if (end < total || total <= windowSamples)
            return false;

        const int64_t start = end - total;

        std::vector<float> a ((size_t) total), b ((size_t) total);
        float peak = 0.0f;

        for (int i = 0; i < total; ++i)
        {
            a[(size_t) i] = rec.getMain (start + i);
            b[(size_t) i] = rec.getSide (start + i);
            peak = std::max (peak, std::abs (b[(size_t) i]));
        }

        // Sidechain-Einsatz suchen (Schmitt-Trigger auf dem Betrag des Sidechains)
        const int pre = windowSamples / 10;
        int trigger = -1;

        if (peak > 0.003f)
        {
            const float upper = 0.4f * peak, lower = 0.1f * peak;
            const float decay = std::exp (-1.0f / (float) (0.010 * sampleRate));
            float env = 0.0f;
            bool armed = true;

            for (int i = 0; i < total; ++i)
            {
                const float x = std::abs (b[(size_t) i]);
                env = std::max (x, env * decay);

                if (armed && x > upper)
                {
                    armed = false;

                    // nur Einsaetze nehmen, bei denen das ganze Fenster schon aufgenommen ist
                    if (i - pre >= 0 && i - pre + windowSamples <= total)
                        trigger = i;
                }
                else if (! armed && env < lower)
                {
                    armed = true;
                }
            }
        }

        const bool triggered = trigger >= 0;
        const int  s0 = triggered ? trigger - pre : total - windowSamples;
        const int64_t triggerPos = triggered ? start + trigger : -1;

        if (triggered && result.triggered && result.triggerPos == triggerPos
            && (int) result.main.size() == windowSamples)
            return false;   // derselbe Einsatz wie zuvor: Anzeige bleibt stehen

        result.main.assign (a.begin() + s0, a.begin() + s0 + windowSamples);
        result.side.assign (b.begin() + s0, b.begin() + s0 + windowSamples);
        result.triggered    = triggered;
        result.triggerIndex = triggered ? pre : -1;
        result.triggerPos   = triggerPos;
        result.windowMs     = (float) (1000.0 * windowSamples / sampleRate);

        double sab = 0.0, saa = 0.0, sbb = 0.0;

        for (int i = 0; i < windowSamples; ++i)
        {
            const double x = result.main[(size_t) i], y = result.side[(size_t) i];
            sab += x * y;  saa += x * x;  sbb += y * y;
        }

        result.correlation = (saa > 1e-12 && sbb > 1e-12) ? (float) (sab / std::sqrt (saa * sbb)) : 0.0f;
        return true;
    }
};

//==============================================================================
// Pegelverlauf beider Signale (50 Werte pro Sekunde, in dB)
//==============================================================================
class LevelHistory
{
public:
    static constexpr int   capacity        = 1 << 13;   // 8192 Werte = ca. 163 s
    static constexpr int   framesPerSecond = 50;
    static constexpr float floorDb         = -100.0f;

    LevelHistory() { clear(); }

    void clear() noexcept
    {
        for (auto& v : mainDb) v.store (floorDb, std::memory_order_relaxed);
        for (auto& v : sideDb) v.store (floorDb, std::memory_order_relaxed);
        writePos.store (0, std::memory_order_release);
    }

    void push (float mainLevelDb, float sideLevelDb) noexcept
    {
        const auto p = writePos.load (std::memory_order_relaxed);
        const auto idx = (size_t) (p & (capacity - 1));
        mainDb[idx].store (mainLevelDb, std::memory_order_relaxed);
        sideDb[idx].store (sideLevelDb, std::memory_order_relaxed);
        writePos.store (p + 1, std::memory_order_release);
    }

    // Letzte 'count' Werte (aeltester zuerst); die Leistung wird ueber
    // 'smoothFrames' Werte gemittelt (gleitender Durchschnitt).
    void getSeries (int count, int smoothFrames, std::vector<float>& outMain, std::vector<float>& outSide) const
    {
        smoothFrames = std::max (1, smoothFrames);
        count = std::clamp (count, 1, capacity - smoothFrames - 1);

        const int64_t end = writePos.load (std::memory_order_acquire);

        auto build = [&] (const std::array<std::atomic<float>, capacity>& src, std::vector<float>& out)
        {
            const int n = count + smoothFrames - 1;
            std::vector<double> power ((size_t) n);

            for (int i = 0; i < n; ++i)
            {
                const int64_t idx = end - n + i;
                const float db = idx >= 0 ? src[(size_t) (idx & (capacity - 1))].load (std::memory_order_relaxed) : floorDb;
                power[(size_t) i] = std::pow (10.0, (double) db / 10.0);
            }

            out.resize ((size_t) count);
            double sum = 0.0;

            for (int i = 0; i < smoothFrames - 1; ++i)
                sum += power[(size_t) i];

            for (int i = 0; i < count; ++i)
            {
                sum += power[(size_t) (i + smoothFrames - 1)];
                const double avg = sum / smoothFrames;
                out[(size_t) i] = avg > 1e-10 ? (float) (10.0 * std::log10 (avg)) : floorDb;
                sum -= power[(size_t) i];
            }
        };

        build (mainDb, outMain);
        build (sideDb, outSide);
    }

private:
    std::array<std::atomic<float>, capacity> mainDb, sideDb;
    std::atomic<int64_t> writePos { 0 };
};
