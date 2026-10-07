#pragma once
#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>
#include <algorithm>

// Streaming 16 kHz / 1024-point Hann FFT with 50% overlap; PCM is discarded per frame.
class BeeGuardFeatures {
 public:
  static const int FRAME=1024, HOP=512, MAX_FRAMES=320;
  BeeGuardFeatures():fft_(real_,imag_,FRAME,16000){reset();}
  void reset(){received_=0;count_=0;frames_=0;sumSq_=0;peak_=0;
    for(int i=0;i<MAX_FRAMES;++i){rms_[i]=db_[i]=dom_[i]=cent_[i]=bw_[i]=roll_[i]=flat_[i]=zcr_[i]=0;}
    for(int i=0;i<6;++i)be_[i]=br_[i]=0;for(int i=0;i<7;++i)peakBins_[i]=0;e200_=r200_=elow_=rlow_=0;peak200_=0;}
  bool addSample(int16_t v){if(received_>=160000)return false;frame_[count_++]=v;++received_;if(count_==FRAME){process();memmove(frame_,frame_+HOP,sizeof(int16_t)*HOP);count_=HOP;}return true;}
  bool finish(double *o){if(received_!=160000||frames_==0)return false;int best=0;for(int i=1;i<7;++i)if(peakBins_[i]>peakBins_[best])best=i;peak200_=(13+best)*16000.0/FRAME;int k=0;stats(rms_,o[k],o[k+1],o[k+2],o[k+3]);k+=4;stats(db_,o[k],o[k+1],o[k+2],o[k+3]);k+=4;
    double med; stats(dom_,o[k],o[k+1],o[k+2],o[k+3],&med); // reordered below into mean,max,min,std,median
    double mx=o[k+1],mn=o[k+2],sd=o[k+3],mean=o[k];o[k++]=mean;o[k++]=mx;o[k++]=mn;o[k++]=sd;o[k++]=med;
    for(int b=0;b<6;++b){o[k++]=be_[b]/frames_;o[k++]=br_[b]/frames_;}
    appendMeanSd(cent_,o,k);appendMeanSd(bw_,o,k);appendMeanSd(roll_,o,k);appendMeanSd(flat_,o,k);appendMeanSd(zcr_,o,k);
    o[k++]=e200_/frames_;o[k++]=r200_/frames_;o[k++]=peak200_;o[k++]=elow_/frames_;o[k++]=rlow_/frames_;return k==40;}
 private:
  int16_t frame_[FRAME]; double real_[FRAME],imag_[FRAME]; ArduinoFFT<double> fft_;
  uint32_t received_; int count_,frames_; double sumSq_,peak_;
  double rms_[MAX_FRAMES],db_[MAX_FRAMES],dom_[MAX_FRAMES],cent_[MAX_FRAMES],bw_[MAX_FRAMES],roll_[MAX_FRAMES],flat_[MAX_FRAMES],zcr_[MAX_FRAMES];
  double be_[6],br_[6],peakBins_[7],e200_,r200_,elow_,rlow_,peak200_;
  void stats(const double*v,double&mean,double&mx,double&mn,double&sd,double*median=nullptr){double a[MAX_FRAMES],s=0,s2=0;mean=0;mx=v[0];mn=v[0];for(int i=0;i<frames_;++i){a[i]=v[i];s+=v[i];s2+=v[i]*v[i];if(v[i]>mx)mx=v[i];if(v[i]<mn)mn=v[i];}mean=s/frames_;sd=sqrt(fmax(0,s2/frames_-mean*mean));if(median){std::sort(a,a+frames_);*median=frames_%2?a[frames_/2]:(a[frames_/2-1]+a[frames_/2])*0.5;}}
  void appendMeanSd(const double*v,double*o,int&k){double mean,mx,mn,sd;stats(v,mean,mx,mn,sd);o[k++]=mean;o[k++]=sd;}
  void process(){if(frames_>=MAX_FRAMES)return;int j=frames_++;double sq=0;int crossings=0;for(int i=0;i<FRAME;++i){double x=frame_[i]/32768.0;sq+=x*x;if(i&&((frame_[i]>=0)!=(frame_[i-1]>=0)))++crossings;real_[i]=x*(0.5*(1-cos(2*PI*i/(FRAME-1))));imag_[i]=0;}rms_[j]=sqrt(sq/FRAME);db_[j]=20*log10(fmax(rms_[j],1e-12));zcr_[j]=double(crossings)/(FRAME-1);fft_.compute(FFTDirection::Forward);fft_.complexToMagnitude();
    double p[FRAME/2+1],total=0,weighted=0,peak=-1;int dom=0;for(int b=0;b<=FRAME/2;++b){p[b]=real_[b]*real_[b];total+=p[b];double f=b*16000.0/FRAME;weighted+=p[b]*f;if(p[b]>peak){peak=p[b];dom=b;}}double safe=fmax(total,1e-20),c=weighted/safe,var=0,cum=0,geo=0;int roll=0;double bands[6]={0},rel[6]={0},low200=0,low1080=0;
    const int lo[6]={100,300,600,1000,2000,4000},hi[6]={300,600,1000,2000,4000,8000};for(int b=0;b<=FRAME/2;++b){double f=b*16000.0/FRAME;var+=p[b]*(f-c)*(f-c);cum+=p[b];if(!roll&&cum>=.85*total)roll=b;geo+=log(fmax(p[b]/safe,1e-20));if(f>=200&&f<270)low200+=p[b];if(f<1080)low1080+=p[b];if(b>=13&&b<=19)peakBins_[b-13]+=p[b];for(int z=0;z<6;++z)if(f>=lo[z]&&(z==5?f<=hi[z]:f<hi[z])){bands[z]+=p[b];rel[z]+=p[b]/safe;}}
    dom_[j]=dom*16000.0/FRAME;cent_[j]=c;bw_[j]=sqrt(var/safe);roll_[j]=roll*16000.0/FRAME;flat_[j]=exp(geo/(FRAME/2+1))*(FRAME/2+1);for(int b=0;b<6;++b){be_[b]+=bands[b]/(FRAME*FRAME);br_[b]+=rel[b];}e200_+=low200/(FRAME*FRAME);r200_+=low200/safe;elow_+=low1080/(FRAME*FRAME);rlow_+=low1080/safe;
  }
};
