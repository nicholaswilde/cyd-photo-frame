#include "slideshow_timer.h"

SlideshowTimer::SlideshowTimer(unsigned long intervalMs)
    : m_intervalMs(intervalMs), m_startTimeMs(0), m_forced(false), m_paused(false) {}

void SlideshowTimer::start(unsigned long currentTimeMs) {
    m_startTimeMs = currentTimeMs;
    m_forced = false;
}

bool SlideshowTimer::isElapsed(unsigned long currentTimeMs) const {
    if (m_paused) {
        return false;
    }
    if (m_forced) {
        return true;
    }
    return (currentTimeMs - m_startTimeMs >= m_intervalMs);
}

void SlideshowTimer::forceElapsed() {
    m_forced = true;
}

void SlideshowTimer::reset(unsigned long currentTimeMs) {
    m_startTimeMs = currentTimeMs;
    m_forced = false;
}

bool SlideshowTimer::isPaused() const {
    return m_paused;
}

void SlideshowTimer::setPaused(bool paused) {
    m_paused = paused;
}
