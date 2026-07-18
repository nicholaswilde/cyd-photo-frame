#ifndef BACKLIGHT_FADER_H
#define BACKLIGHT_FADER_H

class BacklightFader {
public:
    enum FadeState {
        STATE_IDLE,
        STATE_FADE_OUT,
        STATE_FADE_IN
    };

private:
    FadeState _state = STATE_IDLE;
    int _startBrightness = 0;
    int _targetBrightness = 0;
    unsigned long _startTimeMs = 0;
    unsigned long _durationMs = 0;
    int _currentVal = 0;

public:
    BacklightFader() = default;

    void startFade(int start, int target, unsigned long durationMs) {
        if (durationMs == 0) {
            _currentVal = target;
            _targetBrightness = target;
            _state = STATE_IDLE;
            return;
        }
        _startBrightness = start;
        _targetBrightness = target;
        _durationMs = durationMs;
        _startTimeMs = 0; // Init on first update
        _state = (target < start) ? STATE_FADE_OUT : STATE_FADE_IN;
        _currentVal = start;
    }

    bool update(unsigned long now, int& currentOut) {
        if (_state == STATE_IDLE) {
            currentOut = _targetBrightness;
            return false;
        }

        if (_startTimeMs == 0) {
            _startTimeMs = now;
        }

        unsigned long elapsed = now - _startTimeMs;
        if (elapsed >= _durationMs) {
            _currentVal = _targetBrightness;
            currentOut = _currentVal;
            // Capture state before resetting it to IDLE
            _state = STATE_IDLE;
            return true; // Fade complete
        }

        float progress = (float)elapsed / (float)_durationMs;
        _currentVal = _startBrightness + (int)((float)(_targetBrightness - _startBrightness) * progress);
        currentOut = _currentVal;
        return false; // Still fading
    }

    FadeState getState() const { return _state; }
    void stop() { _state = STATE_IDLE; }
};

#endif // BACKLIGHT_FADER_H
