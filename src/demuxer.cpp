#include "demuxer.h"
#include "util.h"
#include <memory.h>

Demuxer::Demuxer(Options* opt){
    mOptions = opt;
    mBuf = NULL;
    init();
}

Demuxer::~Demuxer() {
    if(mBuf) {
        delete mBuf;
        mBuf = NULL;
    }
}

void Demuxer::init() {
    if(mOptions == NULL)
        return;

    mUseIndex1 = false;
    mUseIndex2 = false;
    mHasN = false;
    mLongestIndex = 0;

    bool sameLength = true;
    for(int i=0; i<mOptions->samples.size(); i++) {
        Sample s = mOptions->samples[i];
        if(!s.index1.empty())
            mUseIndex1 = true;
        if(!s.index2.empty())
            mUseIndex2 = true;
        if(contains(s.index1, 'N') || contains(s.index2, 'N'))
            mHasN = true;

        if(s.index1.empty() && s.index2.empty()) {
            error_exit("bad format sample sheet. You should specify either index1 or index2 for each record");
        }

        if(s.index1.length() > mLongestIndex)
            mLongestIndex = s.index1.length();
        if(s.index2.length() > mLongestIndex)
            mLongestIndex = s.index2.length();

        if(i>0) {
            if(mOptions->samples[i].index1.length() != mOptions->samples[i-1].index1.length())
                sameLength = false;
            if(mOptions->samples[i].index2.length() != mOptions->samples[i-1].index2.length())
                sameLength = false;
        }
        cout << "file (" << s.file << ") = " << s.index1 << "," << s.index2 << endl;
    }

    mFastMode = true;
    if(mUseIndex1 && mUseIndex2) {
        mFastMode = false;
        error_exit("bad format sample sheet. You can use either index1 or index2, but cannot use both.");
    }
    if(mHasN) {
        mFastMode = false;
        error_exit("bad format sample sheet. N base is not supported.");
    }
    if(!sameLength)
        mFastMode = false;
    if(mLongestIndex > 14)
        mFastMode = false;


    mBuf = NULL;
    if(mFastMode) {
        mBufLen = 0x01 << (mLongestIndex*2);
        mBuf = new int[mBufLen];
        // initialize to Undetermined
        memset(mBuf, -1, sizeof(int)*mBufLen);
        for(int i=0; i<mOptions->samples.size(); i++) {
            Sample s = mOptions->samples[i];
            int kmer = 0;
            if(mUseIndex1)
                kmer = kmer2int(s.index1);
            else
                kmer = kmer2int(s.index2);

            if(kmer <0)
                error_exit("index can only have A/T/C/G bases");

            mBuf[kmer] = i;
        }
    } else {
        mShortestIndex = mLongestIndex;
        for(int i=0; i<mOptions->samples.size(); i++) {
            Sample s = mOptions->samples[i];
            string index;
            if(mUseIndex1)
                index = s.index1;
            else
                index = s.index2;
            if(index.length() < mShortestIndex)
                mShortestIndex = index.length();

            mIndexSample[index] = i;
        }
    }
}

int Demuxer::demux(Read* r) {
    string index;
    if(mUseIndex1)
        index = r->firstIndex();
    else if(mUseIndex2)
        index = r->lastIndex();

    if(mFastMode) {
        // return the Undetermined one
        if(index.length() < mLongestIndex)
            return -1;
        else if(index.length() > mLongestIndex)
            index = index.substr(0, mLongestIndex);

        int kmer = kmer2int(index);
        if(kmer<0)
            return -1;

        return mBuf[kmer];
    } else {
        // match longer first
        for(int l = std::min(mLongestIndex, (int)index.length());  l>= mShortestIndex; l--) {
            if(l == index.length()) {
                if(mIndexSample.count(index) >0 ) {
                    return mIndexSample[index];
                }
            } else {
                string indexPart = index.substr(0, l);
                if(mIndexSample.count(indexPart) >0 ) {
                    return mIndexSample[indexPart];
                }
            }
        }
        return -1;
    }
}

int Demuxer::kmer2int(string& str) {
    int kmer = 0;
    const char* data = str.c_str();
    for(int k=0; k<str.length(); k++) {
        int val = base2val(data[k]);
        if(val < 0) {
            return -1;
        }
        kmer = (kmer<<2) | val;
    }
    return kmer;
}

int Demuxer::base2val(char base) {
    switch(base){
        case 'A':
            return 0;
        case 'T':
            return 1;
        case 'C':
            return 2;
        case 'G':
            return 3;
        default:
            return -1;
    }
}

bool Demuxer::test(){
    Demuxer d(NULL);
    string s1("AGTCAGAA");
    string s2("ATTCAGAA");
    cout << d.kmer2int(s1) << endl;
    cout << d.kmer2int(s2) << endl;

    return true;
}