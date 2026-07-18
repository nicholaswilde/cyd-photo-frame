#ifndef FILE_CACHE_H
#define FILE_CACHE_H

#include <vector>
#include <string>

class FileCache {
public:
    explicit FileCache(size_t maxCapacity = 64);

    bool addFile(const std::string& path);
    void clear();

    std::string getCurrent() const;
    std::string getNext();
    std::string getPrevious();

    size_t size() const;
    size_t getIndex() const;
    bool setIndex(size_t index);
    bool isEmpty() const;

private:
    std::vector<std::string> m_files;
    size_t m_currentIndex;
    size_t m_maxCapacity;
};

#endif // FILE_CACHE_H
