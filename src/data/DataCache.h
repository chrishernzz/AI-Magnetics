#pragma once
#include<vector>
#include<iostream>
#include<string>

/*
DataCache<T> holds the "load once at startup, serve from memory forever
after" logic that CoreDatabase and Materials both needed, identically.
Before this file existed, that logic (the cachedData member, the empty check + warning,
setData()) was written out twice, once per file, with only the type name different. 
This is the same logic, written once.

'label' is just used in the warning message so you can still tell which one is empty ("CoreDatabase" vs "Materials")
if you ever see it print
*/
template<typename T>
class DataCache {
private:
    static std::vector<T> cachedData;
    //load() is called once per material/candidate evaluated (MaterialEvaluation.cpp, CoreLoss.cpp), so an empty cache would otherwise print this same warning dozens of times per request - real log noise, not
    //a repeated new finding each time. Warn once per process instead; the underlying "empty" fact doesn't change between calls, only whether you've already been told.
    static bool warnedEmpty;
public:
    static const std::vector<T>& load(const char* label) {
        if (cachedData.empty() && !warnedEmpty) {
            std::cout << "WARNING: " << label << "::load() called with no data set. setData() was never called successfully - check startup logs." << std::endl;
            warnedEmpty = true;
        }
        return cachedData;
    }
    static void setData(std::vector<T> data){
        cachedData = std::move(data);
        warnedEmpty = false;
    }
};

template<typename T>
std::vector<T> DataCache<T>::cachedData;
template<typename T>
bool DataCache<T>::warnedEmpty = false;