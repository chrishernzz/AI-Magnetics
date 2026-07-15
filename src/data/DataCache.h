#pragma once
#include<vector>
#include<iostream>
#include<string>

/*
DataCache<T> holds the "load once at startup, serve from memory forever
after" logic that CoreDatabase and Materials both needed, identically.
Before this file existed, that logic (teh cachedData member, the empty check + warning,
setData()) was written out twice, once per file, with only the type name different. 
This is the same logic, written once.

'label' is just used in the warning message so you can still tell which one is empty ("CoreDatabase" vs "Materials")
if you ever see it print
*/
template<typename T>
class DataCache {
private: 
    static std::vector<T> cachedData;
public:
    static const std::vector<T>& load(const char* label) {
        if (cachedData.empty()) {
            std::cout << "WARNING: " << label << "::load() called with no data set. setData() was never called successfully - check startup logs." << std::endl;
        }
        return cachedData;
    }
    static void setData(std::vector<T> data){
        cachedData = std::move(data);
    }
};

template<typename T>
std::vector<T> DataCache<T>::cachedData;