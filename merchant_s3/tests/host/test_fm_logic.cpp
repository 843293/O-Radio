#include "fm_logic.h"
#include <cassert>
int main(){
 assert(fmFrequencyValid(65000)&&fmFrequencyValid(108000)&&fmFrequencyValid(99510));
 assert(!fmFrequencyValid(64990)&&!fmFrequencyValid(108010)&&!fmFrequencyValid(99501));
 assert(fmVolumeDb(0)==-60&&fmVolumeDb(100)==-6&&fmVolumeDb(50)==-18);
 assert(fmVolumeDb(25)==-30&&fmVolumeDb(10)==-46);
 for(int p=1;p<=100;++p){assert(fmVolumeDb(p)>=fmVolumeDb(p-1));assert(fmVolumeDb(p)>=-60&&fmVolumeDb(p)<=-6);}
 FmQuality q;q.valid=true;q.status=1000;q.level=200;q.usn=100;q.wam=100;q.offset=0;
 assert(fmSignalCandidate(q));q.status=0;assert(!fmSignalCandidate(q));q.status=1000;
 q.level=179;assert(!fmSignalCandidate(q));q.level=200;q.usn=301;assert(!fmSignalCandidate(q));q.usn=100;q.offset=-301;assert(!fmSignalCandidate(q));
 FmPeakList peaks;peaks.add(90000,200);peaks.add(90100,250);peaks.add(90200,210);
 assert(peaks.count==1&&peaks.entries[0].khz==90100);
 peaks.add(90400,220);assert(peaks.count==2);peaks.clear();
 for(unsigned i=0;i<64;++i)peaks.add(65000+i*300,200);
 assert(peaks.count==64);const auto saved=peaks.entries[63].khz;
 peaks.add(saved+300,900);peaks.add(saved+400,999);
 assert(peaks.count==64&&peaks.entries[63].khz==saved);
}
