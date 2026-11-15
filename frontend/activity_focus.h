#pragma once
// Which DS screen is the one being played, for a game that has no recompiled code to say (a
// generic game: gamecode.h). The screen that has been changing the most over about a second is the
// one looked at; the other is the small one. A difference has to be clear and last a moment, and
// a flip is not made again at once, so a menu, a fade or a flash does not make the picture jump.
// A manual choice (AutoFocus's mode and manualFlip) overrides this: it only says what AUTO does.
#include <cstdint>
#include <cstdlib>
#include <vector>

class ActivityFocus
{
public:
    bool wantBottom = false;     // the bottom screen is the large one
    float level[2] = { 0, 0 };   // how much of each screen has been changing lately (0 to 1, smoothed)

    // Frames of a clear difference before it counts, and the least between two flips.
    static constexpr int kHold = 90;
    static constexpr int kDwell = 300;
    static constexpr float kRatio = 2.5f;     // the other screen is changing this many times more
    static constexpr float kMinLevel = 0.004f; // ...and at least this much of it is changing
    static constexpr float kBusy = 0.30f;      // both above this: a fade or a wipe, leave it

    void Reset()
    {
        wantBottom = false;
        level[0] = level[1] = 0;
        have_[0] = have_[1] = false;
        pending_ = 0;
        frame_ = 0;
        lastFlip_ = -kDwell;
    }

    // One frame: the two screens' 256x192 pixels (0xRRGGBB in the low 24 bits). True when wantBottom
    // changed with this frame.
    bool Update(const uint32_t* top, const uint32_t* bot)
    {
        const uint32_t* fb[2] = { top, bot };
        frame_++;
        for (int s = 0; s < 2; s++)
        {
            if (prev_[s].size() != kSamples) { prev_[s].assign(kSamples, 0); have_[s] = false; }
            int changed = 0, i = 0;
            for (int y = 0; y < 192; y += kStepY)
                for (int x = 0; x < 256; x += kStepX, i++)
                {
                    const uint32_t c = fb[s][y * 256 + x] & 0xFFFFFF;
                    if (have_[s])
                    {
                        const uint32_t p = prev_[s][i];
                        const int d = std::abs((int)(c >> 16) - (int)(p >> 16)) + std::abs((int)((c >> 8) & 0xFF) - (int)((p >> 8) & 0xFF)) +
                                      std::abs((int)(c & 0xFF) - (int)(p & 0xFF));
                        if (d > 48) changed++;
                    }
                    prev_[s][i] = c;
                }
            if (have_[s]) level[s] += ((float)changed / (float)kSamples - level[s]) * kAlpha;
            have_[s] = true;
        }
        const int cur = wantBottom ? 1 : 0, other = 1 - cur;
        const bool busy = level[0] > kBusy && level[1] > kBusy;
        const bool clear = !busy && level[other] > kMinLevel && level[other] > level[cur] * kRatio;
        pending_ = clear ? pending_ + 1 : 0;
        if (pending_ >= kHold && frame_ - lastFlip_ >= kDwell)
        {
            wantBottom = !wantBottom;
            lastFlip_ = frame_;
            pending_ = 0;
            return true;
        }
        return false;
    }

private:
    static constexpr int kStepX = 4, kStepY = 2;
    static constexpr size_t kSamples = (256 / kStepX) * (192 / kStepY);
    static constexpr float kAlpha = 1.0f / 90.0f; // about a second and a half
    std::vector<uint32_t> prev_[2];
    bool have_[2] = { false, false };
    int pending_ = 0;
    int frame_ = 0;
    int lastFlip_ = -kDwell;
};
