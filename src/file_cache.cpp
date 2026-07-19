#include "file_cache.h"

FileCache::FileCache(size_t maxCapacity) : m_currentIndex(0), m_maxCapacity(maxCapacity) {}

bool FileCache::addFile(const std::string& path) {
    if (m_files.size() >= m_maxCapacity) {
        return false;
    }
    m_files.push_back(path);
    return true;
}

void FileCache::clear() {
    m_files.clear();
    m_currentIndex = 0;
}

std::string FileCache::getCurrent() const {
    if (m_files.empty()) {
        return "";
    }
    return m_files[m_currentIndex];
}

std::string FileCache::getNext() {
    if (m_files.empty()) {
        return "";
    }
    m_currentIndex = (m_currentIndex + 1) % m_files.size();
    return m_files[m_currentIndex];
}

std::string FileCache::getPrevious() {
    if (m_files.empty()) {
        return "";
    }
    if (m_currentIndex == 0) {
        m_currentIndex = m_files.size() - 1;
    } else {
        m_currentIndex--;
    }
    return m_files[m_currentIndex];
}

size_t FileCache::size() const {
    return m_files.size();
}

size_t FileCache::getIndex() const {
    return m_currentIndex;
}

bool FileCache::setIndex(size_t index) {
    if (index >= m_files.size()) {
        return false;
    }
    m_currentIndex = index;
    return true;
}

bool FileCache::isEmpty() const {
    return m_files.empty();
}

std::string FileCache::getAt(size_t index) const {
    if (index >= m_files.size()) {
        return "";
    }
    return m_files[index];
}
