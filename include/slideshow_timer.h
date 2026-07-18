#ifndef SLIDESHOW_TIMER_H
#define SLIDESHOW_TIMER_H

class SlideshowTimer {
public:
    explicit SlideshowTimer(unsigned long intervalMs);
    
    void start(unsigned long currentTimeMs);
    bool isElapsed(unsigned long currentTimeMs) const;
    void forceElapsed();
    void reset(unsigned long currentTimeMs);
    bool isPaused() const;
    void setPaused(bool paused);

private:
    unsigned long m_intervalMs;
    unsigned long m_startTimeMs;
    bool m_forced;
    bool m_paused;
};

#endif // SLIDESHOW_TIMER_H
