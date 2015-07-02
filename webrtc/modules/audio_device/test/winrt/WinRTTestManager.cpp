/*
*  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <collection.h>
#include <ppltasks.h>
#include <Audioclient.h>

#include "WinRTTestManager.h"

#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/modules/audio_device/audio_device_impl.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

std::string  RecordedMicrophoneFile = "recorded_microphone_mono_48.pcm";
std::string  RecordedMicrophoneVolumeFile = "recorded_microphone_volume_mono_48.pcm";
std::string  RecordedMicrophoneMuteFile = "recorded_microphone_mute_mono_48.pcm";
std::string  RecordedMicrophoneBoostFile = "recorded_microphone_boost_mono_48.pcm";
std::string  RecordedMicrophoneAGCFile = "recorded_microphone_AGC_mono_48.pcm";
std::string  RecordedSpeakerFile = "recorded_speaker_48.pcm";

#ifdef WINRT
#include <time.h>
HANDLE  WinRTTestManager::_semhandle = CreateSemaphoreEx(NULL, 0, 1, NULL, 0, SEMAPHORE_ALL_ACCESS);
#endif

std::string getRecordOutputFilePath(const std::string& filename) {
  return webrtc::test::OutputPath() + filename;
}

// Helper functions
#if !defined(WEBRTC_IOS)
char* GetFilename(char* filename)
{
  return filename;
}
const char* GetFilename(const char* filename)
{
  return  filename;
}
char* GetResource(char* resource)
{
  return resource;
}
const char* GetResource(const char* resource)
{
  return resource;
}
#endif

using namespace webrtc;

AudioEventObserver::AudioEventObserver(AudioDeviceModule* audioDevice)
{
}

AudioEventObserver::~AudioEventObserver()
{
}

void AudioEventObserver::OnErrorIsReported(const ErrorCode error)
{
  TEST_LOG("\n[*** ERROR ***] => OnErrorIsReported(%d)\n \n", error);
  _error = error;
}


void AudioEventObserver::OnWarningIsReported(const WarningCode warning)
{
  TEST_LOG("\n[*** WARNING ***] => OnWarningIsReported(%d)\n \n", warning);
  _warning = warning;
}

AudioTransportImpl::AudioTransportImpl(AudioDeviceModule* audioDevice) :
_audioDevice(audioDevice),
_playFromFile(false),
_fullDuplex(false),
_speakerVolume(false),
_speakerMute(false),
_microphoneVolume(false),
_microphoneMute(false),
_microphoneBoost(false),
_microphoneAGC(false),
_loopBackMeasurements(false),
_playFile(*FileWrapper::Create()),
_recCount(0),
_playCount(0)
{
  _resampler.Reset(48000, 48000, 2);
}

AudioTransportImpl::~AudioTransportImpl()
{
  _playFile.Flush();
  _playFile.CloseFile();
  delete &_playFile;

  for (AudioPacketList::iterator iter = _audioList.begin();
    iter != _audioList.end(); ++iter) {
    delete *iter;
  }
}

// ----------------------------------------------------------------------------
//	AudioTransportImpl::SetFilePlayout
// ----------------------------------------------------------------------------

int32_t AudioTransportImpl::SetFilePlayout(bool enable, const char* fileName)
{
  _playFromFile = enable;
  if (enable && fileName)
  {
    return (_playFile.OpenFile(fileName, true, true, false));
  }
  else
  {
    _playFile.Flush();
    return (_playFile.CloseFile());
  }
}
;

void AudioTransportImpl::SetFullDuplex(bool enable)
{
  _fullDuplex = enable;

  for (AudioPacketList::iterator iter = _audioList.begin();
    iter != _audioList.end(); ++iter) {
    delete *iter;
  }
  _audioList.clear();
}

int32_t AudioTransportImpl::RecordedDataIsAvailable(
  const void* audioSamples,
  const uint32_t nSamples,
  const uint8_t nBytesPerSample,
  const uint8_t nChannels,
  const uint32_t samplesPerSec,
  const uint32_t totalDelayMS,
  const int32_t clockDrift,
  const uint32_t currentMicLevel,
  const bool keyPressed,
  uint32_t& newMicLevel)
{
  if (_fullDuplex && _audioList.size() < 15)
  {
    AudioPacket* packet = new AudioPacket();
    memcpy(packet->dataBuffer, audioSamples, nSamples * nBytesPerSample);
    packet->nSamples = (uint16_t)nSamples;
    packet->nBytesPerSample = nBytesPerSample;
    packet->nChannels = nChannels;
    packet->samplesPerSec = samplesPerSec;
    _audioList.push_back(packet);
  }

  _recCount++;
  if (_recCount % 100 == 0)
  {
    bool addMarker(true);

    if (_loopBackMeasurements)
    {
      addMarker = false;
    }

    if (_microphoneVolume)
    {
      uint32_t maxVolume(0);
      uint32_t minVolume(0);
      uint32_t volume(0);
      uint16_t stepSize(0);
      EXPECT_EQ(0, _audioDevice->MaxMicrophoneVolume(&maxVolume));
      EXPECT_EQ(0, _audioDevice->MinMicrophoneVolume(&minVolume));
      EXPECT_EQ(0, _audioDevice->MicrophoneVolumeStepSize(&stepSize));
      EXPECT_EQ(0, _audioDevice->MicrophoneVolume(&volume));

      if (volume == 0)
      {
        TEST_LOG("[0]");
        addMarker = false;
      }
      int stepScale = (int)((maxVolume - minVolume) / (stepSize * 10));
      volume += (stepScale * stepSize);
      if (volume > maxVolume)
      {
        TEST_LOG("[MAX]");
        volume = 0;
        addMarker = false;
      }
      EXPECT_EQ(0, _audioDevice->SetMicrophoneVolume(volume));
    }

    if (_microphoneAGC)
    {
      uint32_t maxVolume(0);
      uint32_t minVolume(0);
      uint16_t stepSize(0);
      EXPECT_EQ(0, _audioDevice->MaxMicrophoneVolume(&maxVolume));
      EXPECT_EQ(0, _audioDevice->MinMicrophoneVolume(&minVolume));
      EXPECT_EQ(0, _audioDevice->MicrophoneVolumeStepSize(&stepSize));

      if (currentMicLevel <= 1)
      {
        TEST_LOG("[MIN]");
        addMarker = false;
      }
      int stepScale = (int)((maxVolume - minVolume) / (stepSize * 10));
      newMicLevel = currentMicLevel + (stepScale * stepSize);
      if (newMicLevel > maxVolume)
      {
        TEST_LOG("[MAX]");
        newMicLevel = 1; // set lowest (non-zero) AGC level
        addMarker = false;
      }
    }

    if (_microphoneMute && (_recCount % 500 == 0))
    {
      bool muted(false);
       EXPECT_EQ(0, _audioDevice->MicrophoneMute(&muted));
      muted = !muted;
       EXPECT_EQ(0, _audioDevice->SetMicrophoneMute(muted));
      if (muted)
      {
        TEST_LOG("[MUTE ON]");
        addMarker = false;
      }
      else
      {
        TEST_LOG("[MUTE OFF]");
        addMarker = false;
      }
    }

    if (_microphoneBoost && (_recCount % 500 == 0))
    {
      bool boosted(false);
      EXPECT_EQ(0, _audioDevice->MicrophoneBoost(&boosted));
      boosted = !boosted;
      EXPECT_EQ(0, _audioDevice->SetMicrophoneBoost(boosted));
      if (boosted)
      {
        TEST_LOG("[BOOST ON]");
        addMarker = false;
      }
      else
      {
        TEST_LOG("[BOOST OFF]");
        addMarker = false;
      }
    }

    if ((nChannels == 1) && addMarker)
    {
      // mono
      TEST_LOG("-");
    }
    else if ((nChannels == 2) && (nBytesPerSample == 2) && addMarker)
    {
      AudioDeviceModule::ChannelType
        chType(AudioDeviceModule::kChannelLeft);
       EXPECT_EQ(0, _audioDevice->RecordingChannel(&chType));

      if (chType == AudioDeviceModule::kChannelLeft)
        TEST_LOG("-|");
      else
        TEST_LOG("|-");
    }
    else if (addMarker)
    {
      // stereo
      TEST_LOG("--");
    }

    if (nChannels == 2 && nBytesPerSample == 2)
    {
       TEST_LOG("=> emulated mono (one channel exctracted from stereo input)\n");
    }
  }

  return 0;
}


int32_t AudioTransportImpl::NeedMorePlayData(
  const uint32_t nSamples,
  const uint8_t nBytesPerSample,
  const uint8_t nChannels,
  const uint32_t samplesPerSec,
  void* audioSamples,
  uint32_t& nSamplesOut,
  int64_t* elapsed_time_ms,
  int64_t* ntp_time_ms)
{
  if (_fullDuplex)
  {
    if (_audioList.empty())
    {
      // use zero stuffing when not enough data
      memset(audioSamples, 0, nBytesPerSample * nSamples);
    }
    else
    {
      AudioPacket* packet = _audioList.front();
      _audioList.pop_front();
      if (packet)
      {
        int ret(0);
        int lenOut(0);
        int16_t tmpBuf_96kHz[80 * 12];
        int16_t* ptr16In = NULL;
        int16_t* ptr16Out = NULL;

        const uint16_t nSamplesIn = packet->nSamples;
        const uint8_t nChannelsIn = packet->nChannels;
        const uint32_t samplesPerSecIn = packet->samplesPerSec;
        const uint16_t nBytesPerSampleIn =
          packet->nBytesPerSample;

        int32_t fsInHz(samplesPerSecIn);
        int32_t fsOutHz(samplesPerSec);

        if (fsInHz == 44100)
          fsInHz = 44000;

        if (fsOutHz == 44100)
          fsOutHz = 44000;

        if (nChannelsIn == 2 && nBytesPerSampleIn == 4)
        {
          // input is stereo => we will resample in stereo
          ret = _resampler.ResetIfNeeded(fsInHz, fsOutHz, 2);
          if (ret == 0)
          {
            if (nChannels == 2)
            {
              _resampler.Push(
                (const int16_t*)packet->dataBuffer,
                2 * nSamplesIn,
                (int16_t*)audioSamples, 2
                * nSamples, lenOut);
            }
            else
            {
              _resampler.Push(
                (const int16_t*)packet->dataBuffer,
                2 * nSamplesIn, tmpBuf_96kHz, 2
                * nSamples, lenOut);

              ptr16In = &tmpBuf_96kHz[0];
              ptr16Out = (int16_t*)audioSamples;

              // do stereo -> mono
              for (unsigned int i = 0; i < nSamples; i++)
              {
                *ptr16Out = *ptr16In; // use left channel
                ptr16Out++;
                ptr16In++;
                ptr16In++;
              }
            }
            assert(2 * nSamples == (uint32_t)lenOut);
          }
          else
          {
            if (_playCount % 100 == 0)
              TEST_LOG(
              "ERROR: unable to resample from %d to %d\n",
              samplesPerSecIn, samplesPerSec);
          }
        }
        else
        {
          // input is mono (can be "reduced from stereo" as well) =>
          // we will resample in mono
          ret = _resampler.ResetIfNeeded(fsInHz, fsOutHz, 1);
          if (ret == 0)
          {
            if (nChannels == 1)
            {
              _resampler.Push(
                (const int16_t*)packet->dataBuffer,
                nSamplesIn,
                (int16_t*)audioSamples,
                nSamples, lenOut);
            }
            else
            {
              _resampler.Push(
                (const int16_t*)packet->dataBuffer,
                nSamplesIn, tmpBuf_96kHz, nSamples,
                lenOut);

              ptr16In = &tmpBuf_96kHz[0];
              ptr16Out = (int16_t*)audioSamples;

              // do mono -> stereo
              for (unsigned int i = 0; i < nSamples; i++)
              {
                *ptr16Out = *ptr16In; // left
                ptr16Out++;
                *ptr16Out = *ptr16In; // right (same as left sample)
                ptr16Out++;
                ptr16In++;
              }
            }
            assert(nSamples == (uint32_t)lenOut);
          }
          else
          {
            if (_playCount % 100 == 0)
              TEST_LOG("ERROR: unable to resample from %d to %d\n",
              samplesPerSecIn, samplesPerSec);
          }
        }
        nSamplesOut = nSamples;
        delete packet;
      }
    }
  }  // if (_fullDuplex)

  if (_playFromFile && _playFile.Open())
  {
    int16_t fileBuf[480];

    // read mono-file
    int32_t len = _playFile.Read((int8_t*)fileBuf, 2
      * nSamples);
    if (len != 2 * (int32_t)nSamples)
    {
      _playFile.Rewind();
      _playFile.Read((int8_t*)fileBuf, 2 * nSamples);
    }

    // convert to stero if required
    if (nChannels == 1)
    {
      memcpy(audioSamples, fileBuf, 2 * nSamples);
    }
    else
    {
      // mono sample from file is duplicated and sent to left and right
      // channels
      int16_t* audio16 = (int16_t*)audioSamples;
      for (unsigned int i = 0; i < nSamples; i++)
      {
        (*audio16) = fileBuf[i]; // left
        audio16++;
        (*audio16) = fileBuf[i]; // right
        audio16++;
      }
    }
  }  // if (_playFromFile && _playFile.Open())

  _playCount++;

  if (_playCount % 100 == 0)
  {
    bool addMarker(true);

    if (_speakerVolume)
    {
      uint32_t maxVolume(0);
      uint32_t minVolume(0);
      uint32_t volume(0);
      uint16_t stepSize(0);
      EXPECT_EQ(0, _audioDevice->MaxSpeakerVolume(&maxVolume));
      EXPECT_EQ(0, _audioDevice->MinSpeakerVolume(&minVolume));
      EXPECT_EQ(0, _audioDevice->SpeakerVolumeStepSize(&stepSize));
      EXPECT_EQ(0, _audioDevice->SpeakerVolume(&volume));

      if (volume == 0)
      {
        TEST_LOG("[0]");
        addMarker = false;
      }
      uint32_t step = (maxVolume - minVolume) / 10;
      step = (step < stepSize ? stepSize : step);
      volume += step;
      if (volume > maxVolume)
      {
        TEST_LOG("[MAX]");
        volume = 0;
        addMarker = false;
      }
      EXPECT_EQ(0, _audioDevice->SetSpeakerVolume(volume));
    }

    if (_speakerMute && (_playCount % 500 == 0))
    {
      bool muted(false);
      EXPECT_EQ(0, _audioDevice->SpeakerMute(&muted));

      muted = !muted;
      EXPECT_EQ(0, _audioDevice->SetSpeakerMute(muted));

      if (muted)
      {
        TEST_LOG("[MUTE ON]");
        addMarker = false;
      }
      else
      {
        TEST_LOG("[MUTE OFF]");
        addMarker = false;
      }
    }

    if (_loopBackMeasurements)
    {
      uint16_t recDelayMS(0);
      uint16_t playDelayMS(0);
      size_t nItemsInList(0);

      nItemsInList = _audioList.size();
      EXPECT_EQ(0, _audioDevice->RecordingDelay(&recDelayMS));
      EXPECT_EQ(0, _audioDevice->PlayoutDelay(&playDelayMS));
      //TEST_LOG("Delay (rec+play)+buf: %3zu (%3u+%3u)+%3zu [ms]\n",
      //  recDelayMS + playDelayMS + 10 * (nItemsInList + 1),
      //  recDelayMS, playDelayMS, 10 * (nItemsInList + 1));

      addMarker = false;
    }

    if ((nChannels == 1) && addMarker)
    {
      TEST_LOG("+");
    }
    else if ((nChannels == 2) && addMarker)
    {
      TEST_LOG("++");
    }
  }  // if (_playCount % 100 == 0)

  nSamplesOut = nSamples;

  return 0;
}

int AudioTransportImpl::OnDataAvailable(const int voe_channels[],
  int number_of_voe_channels,
  const int16_t* audio_data,
  int sample_rate,
  int number_of_channels,
  int number_of_frames,
  int audio_delay_milliseconds,
  int current_volume,
  bool key_pressed,
  bool need_audio_processing) {
  return 0;
}

void AudioTransportImpl::PushCaptureData(int voe_channel,
  const void* audio_data,
  int bits_per_sample, int sample_rate,
  int number_of_channels,
  int number_of_frames) {}

void AudioTransportImpl::PullRenderData(int bits_per_sample, int sample_rate,
  int number_of_channels,
  int number_of_frames,
  void* audio_data,
  int64_t* elapsed_time_ms,
  int64_t* ntp_time_ms) {}


WinRTTestManager::WinRTTestManager() :
_audioDevice(NULL),
_audioEventObserver(NULL),
_audioTransport(NULL)
{

  _playoutFile48 = 
    webrtc::test::ProjectRootPath() + "audio_device/audio_short48.pcm";
  _playoutFile44 =
    webrtc::test::ProjectRootPath() + "audio_device/audio_short44.pcm";
  _playoutFile16 = 
    webrtc::test::ProjectRootPath() + "audio_device/audio_short16.pcm";
  _playoutFile8 = 
    webrtc::test::ProjectRootPath() + "audio_device/audio_short8.pcm";
}

WinRTTestManager::~WinRTTestManager()
{
}

int32_t WinRTTestManager::SelectRecordingDevice()
{
  int16_t nDevices = _audioDevice->RecordingDevices();
  char name[kAdmMaxDeviceNameSize];
  char guid[kAdmMaxGuidSize];
  int32_t ret(-1);

#ifdef _WIN32
  TEST_LOG("\nSelect Recording Device\n \n");
  TEST_LOG("  (%d) Default\n", 0);
  TEST_LOG("  (%d) Default Communication [Win 7]\n", 1);
  TEST_LOG("- - - - - - - - - - - - - - - - - - - -\n");
  for (int i = 0; i < nDevices; i++)
  {
    EXPECT_EQ(0, _audioDevice->RecordingDeviceName(i, name, guid));
    TEST_LOG(" (%d) Device %d (%s)\n", i + 10, i, name);
  }
  TEST_LOG("\n: ");

  /*int sel(0);

  scanf("%u", &sel);

  if (sel == 0)
  {*/
    EXPECT_EQ(0, (ret = _audioDevice->SetRecordingDevice(AudioDeviceModule::kDefaultDevice)));
 /* }
  else if (sel == 1)
  {
    EXPECT_TRUE((ret = _audioDevice->SetRecordingDevice(
      AudioDeviceModule::kDefaultCommunicationDevice)) == 0);
  }
  else if (sel < (nDevices + 10))
  {
    EXPECT_EQ(0, (ret = _audioDevice->SetRecordingDevice(sel - 10)));
  }
  else
  {
    return -1;
  }*/
#else
  TEST_LOG("\nSelect Recording Device\n \n");
  for (int i = 0; i < nDevices; i++)
  {
    EXPECT_EQ(0, _audioDevice->RecordingDeviceName(i, name, guid));
    TEST_LOG(" (%d) Device %d (%s)\n", i, i, name);
  }
  TEST_LOG("\n: ");
  int sel(0);
  EXPECT_TRUE(scanf("%u", &sel) > 0);
  if (sel < (nDevices))
  {
    EXPECT_EQ(0, (ret = _audioDevice->SetRecordingDevice(sel)));
  }
  else
  {
    return -1;
  }
#endif

  return ret;
}

int32_t WinRTTestManager::SelectPlayoutDevice()
{
  int16_t nDevices = _audioDevice->PlayoutDevices();
  char name[kAdmMaxDeviceNameSize];
  char guid[kAdmMaxGuidSize];

#ifdef _WIN32
  TEST_LOG("\nSelect Playout Device\n \n");
  TEST_LOG("  (%d) Default\n", 0);
  TEST_LOG("  (%d) Default Communication [Win 7]\n", 1);
  TEST_LOG("- - - - - - - - - - - - - - - - - - - -\n");
  for (int i = 0; i < nDevices; i++)
  {
    EXPECT_EQ(0, _audioDevice->PlayoutDeviceName(i, name, guid));
    TEST_LOG(" (%d) Device %d (%s)\n", i + 10, i, name);
  }
  TEST_LOG("\n: ");

  //int sel(0);

  //scanf("%u", &sel);

  int32_t ret(0);

  //if (sel == 0)
  //{
    EXPECT_TRUE((ret = _audioDevice->SetPlayoutDevice(
      AudioDeviceModule::kDefaultDevice)) == 0);
  //}
  //else if (sel == 1)
  //{
  //  EXPECT_TRUE((ret = _audioDevice->SetPlayoutDevice(
  //    AudioDeviceModule::kDefaultCommunicationDevice)) == 0);
  //}
  //else if (sel < (nDevices + 10))
  //{
  //  EXPECT_EQ(0, (ret = _audioDevice->SetPlayoutDevice(sel - 10)));
  //}
  //else
  //{
  //  return -1;
  //}
#else
  TEST_LOG("\nSelect Playout Device\n \n");
  for (int i = 0; i < nDevices; i++)
  {
    EXPECT_EQ(0, _audioDevice->PlayoutDeviceName(i, name, guid));
    TEST_LOG(" (%d) Device %d (%s)\n", i, i, name);
  }
  TEST_LOG("\n: ");
  int sel(0);
  EXPECT_TRUE(scanf("%u", &sel) > 0);
  int32_t ret(0);
  if (sel < (nDevices))
  {
    EXPECT_EQ(0, (ret = _audioDevice->SetPlayoutDevice(sel)));
  }
  else
  {
    return -1;
  }
#endif

  return ret;
}


int32_t WinRTTestManager::Init()
{

  Trace::CreateTrace();
  Trace::SetTraceFile((test::OutputPath() + "audio_device_test_winrt_trace.txt").c_str());
  Trace::set_level_filter(webrtc::kTraceAll);
  EXPECT_TRUE((_processThread = ProcessThread::Create()) != NULL);
  //_processThread = webrtc::ProcessThread::Create();
  if (_processThread == NULL)
  {
    return -1;
  }
  _processThread->Start();

  // create the Audio Device module
  EXPECT_TRUE((_audioDevice = AudioDeviceModuleImpl::Create(
    555, ADM_AUDIO_LAYER)) != NULL);

  if (_audioDevice == NULL)
  {
    return -1;
  }
  EXPECT_EQ(1, _audioDevice->AddRef());

  // register the Audio Device module
  _processThread->RegisterModule(_audioDevice);

  // register event observer
  _audioEventObserver = new AudioEventObserver(_audioDevice);
  //EXPECT_EQ(0, _audioDevice->RegisterEventObserver(_audioEventObserver));
  _audioDevice->RegisterEventObserver(_audioEventObserver);

  // register audio transport
  _audioTransport = new AudioTransportImpl(_audioDevice);
  //EXPECT_EQ(0, _audioDevice->RegisterAudioCallback(_audioTransport));
  _audioDevice->RegisterAudioCallback(_audioTransport);

  //_audioDevice->Init();

  return 0;
}

int32_t WinRTTestManager::TestDeviceEnumeration()
{
  TEST_LOG("\n=======================================\n");
  TEST_LOG(" Device Enumeration test:\n");
  TEST_LOG("=======================================\n");

  if (_audioDevice == NULL)
  {
    return -1;
  }

  RESET_TEST;

  AudioDeviceModule* audioDevice = _audioDevice;

  //audioDevice->Init();
  //audioDevice->Initialized();
  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());

  char name[kAdmMaxDeviceNameSize];
  char guid[kAdmMaxGuidSize];

  const int16_t nPlayoutDevices(audioDevice->PlayoutDevices());
  EXPECT_TRUE(nPlayoutDevices >= 0);
  TEST_LOG("\nPlayoutDevices: %u\n \n", nPlayoutDevices);
  for (int n = 0; n < nPlayoutDevices; n++)
  {
    EXPECT_EQ(0, audioDevice->PlayoutDeviceName(n, name, guid));
    //audioDevice->PlayoutDeviceName(n, name, guid);
    TEST_LOG(
      "PlayoutDeviceName(%d) :   name=%s \n \
            	                 guid=%s\n",
                               n, name, guid);
  }

#ifdef _WIN32
  // default (-1)
  // TODO(henrika): fix below test.
#if 0
  EXPECT_EQ(0, audioDevice->PlayoutDeviceName(-1, name, guid));
  TEST_LOG("PlayoutDeviceName(%d):   default name=%s \n \
                      	                 default guid=%s\n", -1, name, guid);
#endif  // 0
#else
  // should fail
  EXPECT_EQ(-1, audioDevice->PlayoutDeviceName(-1, name, guid));
#endif

  const int16_t nRecordingDevices(audioDevice->RecordingDevices());
  EXPECT_TRUE(nRecordingDevices >= 0);
  TEST_LOG("\nRecordingDevices: %u\n \n", nRecordingDevices);
  for (int n = 0; n < nRecordingDevices; n++)
  {
    EXPECT_EQ(0, audioDevice->RecordingDeviceName(n, name, guid));
    //audioDevice->RecordingDeviceName(n, name, guid);
    TEST_LOG(
      "RecordingDeviceName(%d) : name=%s \n \
            	                 guid=%s\n",
                               n, name, guid);
  }

#ifdef _WIN32
  // default (-1)
  // TODO(henrika): fix below test.
#if 0
  EXPECT_EQ(0, audioDevice->RecordingDeviceName(-1, name, guid));
  TEST_LOG("RecordingDeviceName(%d): default name=%s \n \
                      	                 default guid=%s\n", -1, name, guid);
#endif
#else
  // should fail
  EXPECT_EQ(-1, audioDevice->PlayoutDeviceName(-1, name, guid));
#endif

  EXPECT_EQ(0, audioDevice->Terminate());
  EXPECT_FALSE(audioDevice->Initialized());
  //audioDevice->Terminate();
  //audioDevice->Initialized();

  PRINT_TEST_RESULTS;

  return 0;
}

int32_t WinRTTestManager::TestDeviceSelection()
{
  TEST_LOG("\n=======================================\n");
  TEST_LOG(" Device Selection test:\n");
  TEST_LOG("=======================================\n");

  if (_audioDevice == NULL)
  {
    return -1;
  }

  RESET_TEST;

#define PRINT_HEADING(a, b) \
  	{ \
		TEST_LOG("Set" #a "Device(" #b ") => \n"); \
  	} \

#define PRINT_HEADING_IDX(a, b,c ) \
  	{ \
		TEST_LOG("Set" #a "Device(%d) (%s) => \n", b, c); \
  	} \

#define PRINT_STR(a, b) \
  	{ \
                char str[128]; \
                (b == true) ? (sprintf_s(str, "  %-17s: available\n", #a)) : (sprintf_s(str, "  %-17s: NA\n", #a)); \
                TEST_LOG("%s", str); \
  	} \

  AudioDeviceModule* audioDevice = _audioDevice;

  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());

  bool available(false);
  int16_t nDevices(-1);
  char name[kAdmMaxDeviceNameSize];
  char guid[kAdmMaxGuidSize];

  // =======
  // Playout

  nDevices = audioDevice->PlayoutDevices();
  EXPECT_TRUE(nDevices >= 0);

  TEST_LOG("\n");
#ifdef _WIN32
  EXPECT_TRUE(audioDevice->SetPlayoutDevice(
    AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  PRINT_HEADING(Playout, kDefaultCommunicationDevice);
  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
  PRINT_STR(Playout, available);
  if (available)
  {
    EXPECT_EQ(0, audioDevice->StereoPlayoutIsAvailable(&available));
    PRINT_STR(Stereo Playout, available);
  }
  else
  {
    PRINT_STR(Stereo Playout, false);
  }
  EXPECT_EQ(0, audioDevice->SpeakerVolumeIsAvailable(&available));
  PRINT_STR(Speaker Volume, available);
  EXPECT_EQ(0, audioDevice->SpeakerMuteIsAvailable(&available));
  PRINT_STR(Speaker Mute, available);

  EXPECT_EQ(0, audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice));
  PRINT_HEADING(Playout, kDefaultDevice);
  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
  PRINT_STR(Playout, available);
  if (available)
  {
    EXPECT_EQ(0, audioDevice->StereoPlayoutIsAvailable(&available));
    PRINT_STR(Stereo Playout, available);
  }
  else
  {
    PRINT_STR(Stereo Playout, false);
  }
  EXPECT_EQ(0, audioDevice->SpeakerVolumeIsAvailable(&available));
  PRINT_STR(Speaker Volume, available);
  EXPECT_EQ(0, audioDevice->SpeakerMuteIsAvailable(&available));
  PRINT_STR(Speaker Mute, available);
#else
  EXPECT_TRUE(audioDevice->SetPlayoutDevice(
    AudioDeviceModule::kDefaultCommunicationDevice) == -1);
  EXPECT_EQ(-1, audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice));
#endif

  for (int i = 0; i < nDevices; i++)
  {
    EXPECT_EQ(0, audioDevice->SetPlayoutDevice(i));
    EXPECT_EQ(0, audioDevice->PlayoutDeviceName(i, name, guid));
    PRINT_HEADING_IDX(Playout, i, name);
    EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
    PRINT_STR(Playout, available);
    if (available)
    {
      EXPECT_EQ(0, audioDevice->StereoPlayoutIsAvailable(&available));
      PRINT_STR(Stereo Playout, available);
    }
    else
    {
      PRINT_STR(Stereo Playout, false);
    }
    EXPECT_EQ(0, audioDevice->SpeakerVolumeIsAvailable(&available));
    PRINT_STR(Speaker Volume, available);
    EXPECT_EQ(0, audioDevice->SpeakerMuteIsAvailable(&available));
    PRINT_STR(Speaker Mute, available);
  }

  // =========
  // Recording

  nDevices = audioDevice->RecordingDevices();
  EXPECT_TRUE(nDevices >= 0);

  TEST_LOG("\n");
#ifdef _WIN32
  EXPECT_TRUE(audioDevice->SetRecordingDevice(
    AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  PRINT_HEADING(Recording, kDefaultCommunicationDevice);
  EXPECT_EQ(0, audioDevice->RecordingIsAvailable(&available));
  PRINT_STR(Recording, available);
  if (available)
  {
    EXPECT_EQ(0, audioDevice->StereoRecordingIsAvailable(&available));
    PRINT_STR(Stereo Recording, available);
  }
  else
  {
    // special fix to ensure that we don't log 'available' when recording is not OK
    PRINT_STR(Stereo Recording, false);
  }
  EXPECT_EQ(0, audioDevice->MicrophoneVolumeIsAvailable(&available));
  PRINT_STR(Microphone Volume, available);
  EXPECT_EQ(0, audioDevice->MicrophoneMuteIsAvailable(&available));
  PRINT_STR(Microphone Mute, available);
  EXPECT_EQ(0, audioDevice->MicrophoneBoostIsAvailable(&available));
  PRINT_STR(Microphone Boost, available);

  EXPECT_EQ(0, audioDevice->SetRecordingDevice(AudioDeviceModule::kDefaultDevice));
  PRINT_HEADING(Recording, kDefaultDevice);
  EXPECT_EQ(0, audioDevice->RecordingIsAvailable(&available));
  PRINT_STR(Recording, available);
  if (available)
  {
    EXPECT_EQ(0, audioDevice->StereoRecordingIsAvailable(&available));
    PRINT_STR(Stereo Recording, available);
  }
  else
  {
    // special fix to ensure that we don't log 'available' when recording is not OK
    PRINT_STR(Stereo Recording, false);
  }
  EXPECT_EQ(0, audioDevice->MicrophoneVolumeIsAvailable(&available));
  PRINT_STR(Microphone Volume, available);
  EXPECT_EQ(0, audioDevice->MicrophoneMuteIsAvailable(&available));
  PRINT_STR(Microphone Mute, available);
  EXPECT_EQ(0, audioDevice->MicrophoneBoostIsAvailable(&available));
  PRINT_STR(Microphone Boost, available);
#else
  EXPECT_TRUE(audioDevice->SetRecordingDevice(
    AudioDeviceModule::kDefaultCommunicationDevice) == -1);
  EXPECT_EQ(-1, audioDevice->SetRecordingDevice(AudioDeviceModule::kDefaultDevice));
#endif

  for (int i = 0; i < nDevices; i++)
  {
    EXPECT_EQ(0, audioDevice->SetRecordingDevice(i));
    EXPECT_EQ(0, audioDevice->RecordingDeviceName(i, name, guid));
    PRINT_HEADING_IDX(Recording, i, name);
    EXPECT_EQ(0, audioDevice->RecordingIsAvailable(&available));
    PRINT_STR(Recording, available);
    if (available)
    {
      EXPECT_EQ(0, audioDevice->StereoRecordingIsAvailable(&available));
      PRINT_STR(Stereo Recording, available);
    }
    else
    {
      // special fix to ensure that we don't log 'available' when recording
      // is not OK
      PRINT_STR(Stereo Recording, false);
    }
    EXPECT_EQ(0, audioDevice->MicrophoneVolumeIsAvailable(&available));
    PRINT_STR(Microphone Volume, available);
    EXPECT_EQ(0, audioDevice->MicrophoneMuteIsAvailable(&available));
    PRINT_STR(Microphone Mute, available);
    EXPECT_EQ(0, audioDevice->MicrophoneBoostIsAvailable(&available));
    PRINT_STR(Microphone Boost, available);
  }

  EXPECT_EQ(0, audioDevice->Terminate());
  EXPECT_FALSE(audioDevice->Initialized());

  PRINT_TEST_RESULTS;

  return 0;
}

int32_t WinRTTestManager::TestAudioTransport()
{
  TEST_LOG("\n=======================================\n");
  TEST_LOG(" Audio Transport test:\n");
  TEST_LOG("=======================================\n");

  std::wstring filePathW = Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data();
  std::string filePath;
  filePath.assign(filePathW.begin(), filePathW.end());

  TEST_LOG(filePath.c_str());

  if (_audioDevice == NULL)
  {
    return -1;
  }

  RESET_TEST;

  AudioDeviceModule* audioDevice = _audioDevice;

  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());

  bool recIsAvailable(false);
  bool playIsAvailable(false);

  if (SelectRecordingDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  EXPECT_EQ(0, audioDevice->RecordingIsAvailable(&recIsAvailable));
  if (!recIsAvailable)
  {
    TEST_LOG(
      "\nWARNING: Recording is not available for the selected device!\n \n");
  }

  if (SelectPlayoutDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&playIsAvailable));
  if (recIsAvailable && playIsAvailable)
  {
    _audioTransport->SetFullDuplex(true);
  }
  else if (!playIsAvailable)
  {
    TEST_LOG(
      "\nWARNING: Playout is not available for the selected device!\n \n");
  }

  bool available(false);
  uint32_t samplesPerSec(0);

  if (playIsAvailable)
  {
    // =========================================
    // Start by playing out an existing PCM file

    EXPECT_EQ(0, audioDevice->SpeakerVolumeIsAvailable(&available));
    if (available)
    {
      uint32_t maxVolume(0);
      EXPECT_EQ(0, audioDevice->MaxSpeakerVolume(&maxVolume));
      EXPECT_EQ(0, audioDevice->SetSpeakerVolume(maxVolume / 2));
    }

    EXPECT_EQ(0, audioDevice->RegisterAudioCallback(_audioTransport));

    EXPECT_EQ(0, audioDevice->InitPlayout());
    EXPECT_EQ(0, audioDevice->PlayoutSampleRate(&samplesPerSec));
    if (samplesPerSec == 48000) {
      _audioTransport->SetFilePlayout(
        true, GetResource(_playoutFile48.c_str()));
    }
    else if (samplesPerSec == 44100 || samplesPerSec == 44000) {
      _audioTransport->SetFilePlayout(
        true, GetResource(_playoutFile44.c_str()));
    }
    else if (samplesPerSec == 16000) {
      _audioTransport->SetFilePlayout(
        true, GetResource(_playoutFile16.c_str()));
    }
    else if (samplesPerSec == 8000) {
      _audioTransport->SetFilePlayout(
        true, GetResource(_playoutFile8.c_str()));
    }
    else {
      TEST_LOG("\nERROR: Sample rate (%u) is not supported!\n \n",
        samplesPerSec);
      return -1;
    }
    EXPECT_EQ(0, audioDevice->StartPlayout());

    if (audioDevice->Playing())
    {
      TEST_LOG("\n> Listen to the file being played (fs=%d) out "
        "and verify that the audio quality is OK.\n"
        "> Press any key to stop playing...\n \n",
        samplesPerSec);
      PAUSE(DEFAULT_PAUSE_TIME);
    }

    EXPECT_EQ(0, audioDevice->StopPlayout());
    EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));

    _audioTransport->SetFilePlayout(false);
  }

  bool enabled(false);
  if (recIsAvailable)
  {
    // ====================================
    // Next, record from microphone to file

    EXPECT_EQ(0, audioDevice->MicrophoneVolumeIsAvailable(&available));
    if (available)
    {
      uint32_t maxVolume(0);
      EXPECT_EQ(0, audioDevice->MaxMicrophoneVolume(&maxVolume));
      EXPECT_EQ(0, audioDevice->SetMicrophoneVolume(maxVolume));
    }

    std::string recordingFile = filePath + "\\" + RecordedMicrophoneFile;
    //EXPECT_TRUE(audioDevice->StartRawInputFileRecording(GetFilename(getRecordOutputFilePath(RecordedMicrophoneFile).c_str())) == 0);
    EXPECT_TRUE(audioDevice->StartRawInputFileRecording(GetFilename(recordingFile.c_str())) == 0);
    EXPECT_EQ(0, audioDevice->RegisterAudioCallback(_audioTransport));

    EXPECT_EQ(0, audioDevice->InitRecording());
    EXPECT_EQ(0, audioDevice->StereoRecording(&enabled));
    if (enabled)
    {
      // ensure file recording in mono
      EXPECT_EQ(0, audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelLeft));
    }
    EXPECT_EQ(0, audioDevice->StartRecording());
    SleepMs(100);

    EXPECT_TRUE(audioDevice->Recording());
    if (audioDevice->Recording())
    {
      TEST_LOG("\n \n> The microphone input signal is now being recorded "
        "to a PCM file.\n"
        "> Speak into the microphone to ensure that your voice is"
        " recorded.\n> Press any key to stop recording...\n \n");
      PAUSE(DEFAULT_PAUSE_TIME);
    }

    EXPECT_EQ(0, audioDevice->StereoRecording(&enabled));
    if (enabled)
    {
      EXPECT_EQ(0, audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelBoth));
    }
    EXPECT_EQ(0, audioDevice->StopRecording());
    EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));
    EXPECT_EQ(0, audioDevice->StopRawInputFileRecording());
  }

  if (recIsAvailable && playIsAvailable)
  {
    // ==========================
    // Play out the recorded file

    /*_audioTransport->SetFilePlayout(true,
      GetFilename(getRecordOutputFilePath(RecordedMicrophoneFile).c_str()));*/
    std::string recordingFile = filePath + "\\" + RecordedMicrophoneFile;
    _audioTransport->SetFilePlayout(true, GetFilename(recordingFile.c_str()));

    EXPECT_EQ(0, audioDevice->RegisterAudioCallback(_audioTransport));
    EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
    if (available)
    {
      EXPECT_EQ(0, audioDevice->InitPlayout());
      EXPECT_EQ(0, audioDevice->StartPlayout());
      SleepMs(100);
    }

    EXPECT_TRUE(audioDevice->Playing());
    if (audioDevice->Playing())
    {
      TEST_LOG("\n \n> Listen to the recorded file and verify that the "
        "audio quality is OK.\n"
        "> Press any key to stop listening...\n \n");
      PAUSE(DEFAULT_PAUSE_TIME);
      //Sleep(10000);
    }

    EXPECT_EQ(0, audioDevice->StopPlayout());
    EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));

    _audioTransport->SetFilePlayout(false);
  }

  if (recIsAvailable && playIsAvailable)
  {
    // ==============================
    // Finally, make full duplex test

    uint32_t playSamplesPerSec(0);
    uint32_t recSamplesPerSecRec(0);

    EXPECT_EQ(0, audioDevice->RegisterAudioCallback(_audioTransport));

    _audioTransport->SetFullDuplex(true);

    EXPECT_EQ(0, audioDevice->MicrophoneVolumeIsAvailable(&available));
    if (available)
    {
      uint32_t maxVolume(0);
      EXPECT_EQ(0, audioDevice->MaxMicrophoneVolume(&maxVolume));
      EXPECT_EQ(0, audioDevice->SetMicrophoneVolume(maxVolume));
    }

    EXPECT_EQ(0, audioDevice->InitRecording());
    EXPECT_EQ(0, audioDevice->InitPlayout());
    EXPECT_EQ(0, audioDevice->PlayoutSampleRate(&playSamplesPerSec));
    EXPECT_EQ(0, audioDevice->RecordingSampleRate(&recSamplesPerSecRec));
    if (playSamplesPerSec != recSamplesPerSecRec)
    {
      TEST_LOG("\nERROR: sample rates does not match (fs_play=%u, fs_rec=%u)",
        playSamplesPerSec, recSamplesPerSecRec);
      EXPECT_EQ(0, audioDevice->StopRecording());
      EXPECT_EQ(0, audioDevice->StopPlayout());
      EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));
      _audioTransport->SetFullDuplex(false);
      return -1;
    }

    EXPECT_EQ(0, audioDevice->StartRecording());
    EXPECT_EQ(0, audioDevice->StartPlayout());
    SleepMs(100);

    if (audioDevice->Playing() && audioDevice->Recording())
    {
      TEST_LOG("\n \n> Full duplex audio (fs=%u) is now active.\n"
        "> Speak into the microphone and verify that your voice is "
        "played out in loopback.\n> Press any key to stop...\n \n",
        playSamplesPerSec);
      PAUSE(DEFAULT_PAUSE_TIME);
    }

    EXPECT_EQ(0, audioDevice->StopRecording());
    EXPECT_EQ(0, audioDevice->StopPlayout());
    EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));

    _audioTransport->SetFullDuplex(false);
  }

  EXPECT_EQ(0, audioDevice->Terminate());
  EXPECT_FALSE(audioDevice->Initialized());

  TEST_LOG("\n");
  PRINT_TEST_RESULTS;

  return 0;
}
int32_t WinRTTestManager::TestLoopback()
{
  TEST_LOG("\n=======================================\n");
  TEST_LOG(" Loopback measurement test:\n");
  TEST_LOG("=======================================\n");

  if (_audioDevice == NULL)
  {
    return -1;
  }

  RESET_TEST;

  AudioDeviceModule* audioDevice = _audioDevice;

  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());

  bool recIsAvailable(false);
  bool playIsAvailable(false);
  uint8_t nPlayChannels(0);
  uint8_t nRecChannels(0);

  if (SelectRecordingDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  EXPECT_EQ(0, audioDevice->RecordingIsAvailable(&recIsAvailable));
  if (!recIsAvailable)
  {
    TEST_LOG("\nERROR: Recording is not available for the selected device!\n \n");
    return -1;
  }

  if (SelectPlayoutDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&playIsAvailable));
  if (recIsAvailable && playIsAvailable)
  {
    _audioTransport->SetFullDuplex(true);
    _audioTransport->SetLoopbackMeasurements(true);
  }
  else if (!playIsAvailable)
  {
    TEST_LOG("\nERROR: Playout is not available for the selected device!\n \n");
    return -1;
  }

  bool enabled(false);
  bool available(false);

  if (recIsAvailable && playIsAvailable)
  {
    uint32_t playSamplesPerSec(0);
    uint32_t recSamplesPerSecRec(0);

    EXPECT_EQ(0, audioDevice->RegisterAudioCallback(_audioTransport));

    _audioTransport->SetFullDuplex(true);

    EXPECT_EQ(0, audioDevice->StereoRecordingIsAvailable(&available));
    if (available)
    {
      EXPECT_EQ(0, audioDevice->SetStereoRecording(true));
    }

    EXPECT_EQ(0, audioDevice->StereoPlayoutIsAvailable(&available));
    if (available)
    {
      EXPECT_EQ(0, audioDevice->SetStereoPlayout(true));
    }

    EXPECT_EQ(0, audioDevice->MicrophoneVolumeIsAvailable(&available));
    if (available)
    {
      uint32_t maxVolume(0);
      EXPECT_EQ(0, audioDevice->MaxMicrophoneVolume(&maxVolume));
      EXPECT_EQ(0, audioDevice->SetMicrophoneVolume(maxVolume));
    }

    EXPECT_EQ(0, audioDevice->InitRecording());
    EXPECT_EQ(0, audioDevice->InitPlayout());
    EXPECT_EQ(0, audioDevice->PlayoutSampleRate(&playSamplesPerSec));
    EXPECT_EQ(0, audioDevice->RecordingSampleRate(&recSamplesPerSecRec));
    EXPECT_EQ(0, audioDevice->StereoPlayout(&enabled));
    enabled ? nPlayChannels = 2 : nPlayChannels = 1;
    EXPECT_EQ(0, audioDevice->StereoRecording(&enabled));
    enabled ? nRecChannels = 2 : nRecChannels = 1;
    EXPECT_EQ(0, audioDevice->StartRecording());
    EXPECT_EQ(0, audioDevice->StartPlayout());

    if (audioDevice->Playing() && audioDevice->Recording())
    {
      TEST_LOG("\n \n> Loopback audio is now active.\n"
        "> Rec : fs=%u, #channels=%u.\n"
        "> Play: fs=%u, #channels=%u.\n"
        "> Speak into the microphone and verify that your voice is"
        "  played out in loopback.\n"
        "> Press any key to stop...\n \n",
        recSamplesPerSecRec, nRecChannels, playSamplesPerSec,
        nPlayChannels);
      PAUSE(30000);
      //Sleep(30000);
    }

    EXPECT_EQ(0, audioDevice->StopRecording());
    EXPECT_EQ(0, audioDevice->StopPlayout());
    EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));

    _audioTransport->SetFullDuplex(false);
    _audioTransport->SetLoopbackMeasurements(false);
  }

  EXPECT_EQ(0, audioDevice->Terminate());
  EXPECT_FALSE(audioDevice->Initialized());

  TEST_LOG("\n");
  PRINT_TEST_RESULTS;

  return 0;
}

int32_t WinRTTestManager::TestMicrophoneAGC()
{
  TEST_LOG("\n=======================================\n");
  TEST_LOG(" Microphone AGC test:\n");
  TEST_LOG("=======================================\n");

  std::wstring filePathW = Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data();
  std::string filePath;
  filePath.assign(filePathW.begin(), filePathW.end());

  TEST_LOG(filePath.c_str());

  if (_audioDevice == NULL)
  {
    return -1;
  }

  RESET_TEST;

  AudioDeviceModule* audioDevice = _audioDevice;

  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());

  if (SelectRecordingDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  bool available(false);
  EXPECT_EQ(0, audioDevice->MicrophoneVolumeIsAvailable(&available));
  if (available)
  {
    _audioTransport->SetMicrophoneAGC(true);
  }
  else
  {
    TEST_LOG("\nERROR: It is not possible to control the microphone volume"
      " for the selected device!\n \n");
    return -1;
  }

  if (SelectPlayoutDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
  if (available)
  {
    _audioTransport->SetFullDuplex(true);
  }
  else
  {
    TEST_LOG("\nERROR: Playout is not available for the selected device!\n \n");
    return -1;
  }

  TEST_LOG("\nEnable recording of microphone input to file (%s) during "
    "this test (Y/N)?\n: ",
    RecordedMicrophoneAGCFile);
  //char ch;
  bool fileRecording(false);
  //EXPECT_TRUE(scanf(" %c", &ch) > 0);
  //ch = toupper(ch);
  //if (ch == 'Y')
  //{
    fileRecording = true;
  //}

  uint32_t startVolume(0);
  bool enabled(false);

  // store initial volume setting
  EXPECT_EQ(0, audioDevice->InitMicrophone());
  EXPECT_EQ(0, audioDevice->MicrophoneVolume(&startVolume));

  // ====================================================================
  // Start recording from the microphone while the mic volume is changed
  // continuously
  // by the emulated AGC (implemented by our audio transport).
  // Also, start playing out the input to enable real-time verification.

  std::string recordingFile = filePath + "\\" + RecordedMicrophoneAGCFile;
  if (fileRecording)
  {
    EXPECT_EQ(0, audioDevice->StartRawInputFileRecording(recordingFile.c_str()));
  }
  EXPECT_EQ(0, audioDevice->RegisterAudioCallback(_audioTransport));
  EXPECT_EQ(0, audioDevice->RecordingIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audioDevice->SetAGC(true));
    EXPECT_EQ(0, audioDevice->InitRecording());
    EXPECT_EQ(0, audioDevice->StereoRecording(&enabled));
    if (enabled)
    {
      // ensures a mono file
      EXPECT_EQ(0, audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelRight));
    }
    EXPECT_EQ(0, audioDevice->StartRecording());
  }
  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audioDevice->InitPlayout());
    EXPECT_EQ(0, audioDevice->StartPlayout());
  }

  EXPECT_TRUE(audioDevice->AGC());
  EXPECT_TRUE(audioDevice->Recording());
  EXPECT_TRUE(audioDevice->Playing());
  if (audioDevice->Recording() && audioDevice->Playing())
  {
    TEST_LOG("\n> Speak into the microphone and verify that the volume of"
      " the selected microphone is varied between [~0] and [~MAX].\n"
      "> You should hear your own voice with an increasing volume level"
      " correlated to an emulated AGC setting.\n"
      "> After a finalized test (and if file recording was enabled) verify"
      " the recorded result off line.\n"
      "> Press any key to stop...\n \n");
    PAUSE(DEFAULT_PAUSE_TIME);
  }

  if (fileRecording)
  {
    EXPECT_EQ(0, audioDevice->StopRawInputFileRecording());
  }
  EXPECT_EQ(0, audioDevice->SetAGC(false));
  EXPECT_EQ(0, audioDevice->StopRecording());
  EXPECT_EQ(0, audioDevice->StopPlayout());
  EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));
  EXPECT_EQ(0, audioDevice->StereoRecordingIsAvailable(&available));

  _audioTransport->SetMicrophoneAGC(false);
  _audioTransport->SetFullDuplex(false);

  // restore volume setting
  EXPECT_EQ(0, audioDevice->SetMicrophoneVolume(startVolume));

  TEST_LOG("\n");
  PRINT_TEST_RESULTS;

  return 0;
}

int32_t WinRTTestManager::TestSpeakerVolume()
{
  TEST_LOG("\n=======================================\n");
  TEST_LOG(" Speaker Volume test:\n");
  TEST_LOG("=======================================\n");

  if (_audioDevice == NULL)
  {
    return -1;
  }

  RESET_TEST;

  AudioDeviceModule* audioDevice = _audioDevice;

  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());

  if (SelectPlayoutDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  bool available(false);
  uint32_t startVolume(0);
  uint32_t samplesPerSec(0);

  EXPECT_EQ(0, audioDevice->SpeakerVolumeIsAvailable(&available));
  if (available)
  {
    _audioTransport->SetSpeakerVolume(true);
  }
  else
  {
    TEST_LOG("\nERROR: Volume control is not available for the selected "
      "device!\n \n");
    return -1;
  }

  // store initial volume setting
  EXPECT_EQ(0, audioDevice->InitSpeaker());
  EXPECT_EQ(0, audioDevice->SpeakerVolume(&startVolume));

  // start at volume 0
  EXPECT_EQ(0, audioDevice->SetSpeakerVolume(0));

  // ======================================
  // Start playing out an existing PCM file

  EXPECT_EQ(0, audioDevice->RegisterAudioCallback(_audioTransport));
  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audioDevice->InitPlayout());
    EXPECT_EQ(0, audioDevice->PlayoutSampleRate(&samplesPerSec));
    if (48000 == samplesPerSec) {
      _audioTransport->SetFilePlayout(
        true, GetResource(_playoutFile48.c_str()));
    }
    else if (44100 == samplesPerSec || samplesPerSec == 44000) {
      _audioTransport->SetFilePlayout(
        true, GetResource(_playoutFile44.c_str()));
    }
    else if (samplesPerSec == 16000) {
      _audioTransport->SetFilePlayout(
        true, GetResource(_playoutFile16.c_str()));
    }
    else if (samplesPerSec == 8000) {
      _audioTransport->SetFilePlayout(
        true, GetResource(_playoutFile8.c_str()));
    }
    else {
      TEST_LOG("\nERROR: Sample rate (%d) is not supported!\n \n",
        samplesPerSec);
      return -1;
    }
    EXPECT_EQ(0, audioDevice->StartPlayout());
  }

  EXPECT_TRUE(audioDevice->Playing());
  if (audioDevice->Playing())
  {
    TEST_LOG("\n> Listen to the file being played out and verify that the "
      "selected speaker volume is varied between [~0] and [~MAX].\n"
      "> The file shall be played out with an increasing volume level "
      "correlated to the speaker volume.\n"
      "> Press any key to stop playing...\n \n");
    PAUSE(10000);
  }

  EXPECT_EQ(0, audioDevice->StopPlayout());
  EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));

  _audioTransport->SetSpeakerVolume(false);
  _audioTransport->SetFilePlayout(false);

  // restore volume setting
  EXPECT_EQ(0, audioDevice->SetSpeakerVolume(startVolume));

  TEST_LOG("\n");
  PRINT_TEST_RESULTS;

  return 0;
}

int32_t WinRTTestManager::TestMicrophoneVolume()
{
  TEST_LOG("\n=======================================\n");
  TEST_LOG(" Microphone Volume test:\n");
  TEST_LOG("=======================================\n");

  std::wstring filePathW = Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data();
  std::string filePath;
  filePath.assign(filePathW.begin(), filePathW.end());

  TEST_LOG(filePath.c_str());

  if (_audioDevice == NULL)
  {
    return -1;
  }

  RESET_TEST;

  AudioDeviceModule* audioDevice = _audioDevice;

  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());

  if (SelectRecordingDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  bool available(false);
  EXPECT_EQ(0, audioDevice->MicrophoneVolumeIsAvailable(&available));
  if (available)
  {
    _audioTransport->SetMicrophoneVolume(true);
  }
  else
  {
    TEST_LOG("\nERROR: Volume control is not available for the selected "
      "device!\n \n");
    return -1;
  }

  if (SelectPlayoutDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
  if (available)
  {
    _audioTransport->SetFullDuplex(true);
  }
  else
  {
    TEST_LOG("\nERROR: Playout is not available for the selected "
      "device!\n \n");
    return -1;
  }

  TEST_LOG("\nEnable recording of microphone input to file (%s) during this"
    " test (Y/N)?\n: ",
    RecordedMicrophoneVolumeFile);
  //char ch;
  bool fileRecording(false);
  //EXPECT_TRUE(scanf(" %c", &ch) > 0);
  //ch = toupper(ch);
  //if (ch == 'Y')
  //{
    fileRecording = true;
  //}

  uint32_t startVolume(0);
  bool enabled(false);

  // store initial volume setting
  EXPECT_EQ(0, audioDevice->InitMicrophone());
  EXPECT_EQ(0, audioDevice->MicrophoneVolume(&startVolume));

  // start at volume 0
  EXPECT_EQ(0, audioDevice->SetMicrophoneVolume(0));

  // ======================================================================
  // Start recording from the microphone while the mic volume is changed
  // continuously.
  // Also, start playing out the input to enable real-time verification.

  std::string recordingFile = filePath + "\\" + RecordedMicrophoneVolumeFile;
  if (fileRecording)
  {
    EXPECT_EQ(0, audioDevice->StartRawInputFileRecording(recordingFile.c_str()));
  }
  EXPECT_EQ(0, audioDevice->RegisterAudioCallback(_audioTransport));
  EXPECT_EQ(0, audioDevice->RecordingIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audioDevice->InitRecording());
    EXPECT_EQ(0, audioDevice->StereoRecording(&enabled));
    if (enabled)
    {
      // ensures a mono file
      EXPECT_EQ(0, audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelRight));
    }
    EXPECT_EQ(0, audioDevice->StartRecording());
  }
  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audioDevice->InitPlayout());
    EXPECT_EQ(0, audioDevice->StartPlayout());
  }

  EXPECT_TRUE(audioDevice->Recording());
  EXPECT_TRUE(audioDevice->Playing());
  if (audioDevice->Recording() && audioDevice->Playing())
  {
    TEST_LOG("\n> Speak into the microphone and verify that the selected "
      "microphone volume is varied between [~0] and [~MAX].\n"
      "> You should hear your own voice with an increasing volume level"
      " correlated to the microphone volume.\n"
      "> After a finalized test (and if file recording was enabled) "
      "verify the recorded result off line.\n"
      "> Press any key to stop...\n \n");
    PAUSE(DEFAULT_PAUSE_TIME);
  }

  if (fileRecording)
  {
    EXPECT_EQ(0, audioDevice->StopRawInputFileRecording());
  }
  EXPECT_EQ(0, audioDevice->StopRecording());
  EXPECT_EQ(0, audioDevice->StopPlayout());
  EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));
  EXPECT_EQ(0, audioDevice->StereoRecordingIsAvailable(&available));

  _audioTransport->SetMicrophoneVolume(false);
  _audioTransport->SetFullDuplex(false);

  // restore volume setting
  EXPECT_EQ(0, audioDevice->SetMicrophoneVolume(startVolume));

  TEST_LOG("\n");
  PRINT_TEST_RESULTS;

  return 0;
}

int32_t WinRTTestManager::TestSpeakerMute()
{
  TEST_LOG("\n=======================================\n");
  TEST_LOG(" Speaker Mute test:\n");
  TEST_LOG("=======================================\n");

  if (_audioDevice == NULL)
  {
    return -1;
  }

  RESET_TEST;

  AudioDeviceModule* audioDevice = _audioDevice;

  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());

  if (SelectPlayoutDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  bool available(false);
  bool startMute(false);
  uint32_t samplesPerSec(0);

  EXPECT_EQ(0, audioDevice->SpeakerMuteIsAvailable(&available));
  if (available)
  {
    _audioTransport->SetSpeakerMute(true);
  }
  else
  {
    TEST_LOG(
      "\nERROR: Mute control is not available for the selected"
      " device!\n \n");
    return -1;
  }

  // store initial mute setting
  EXPECT_EQ(0, audioDevice->InitSpeaker());
  EXPECT_EQ(0, audioDevice->SpeakerMute(&startMute));

  // start with no mute
  EXPECT_EQ(0, audioDevice->SetSpeakerMute(false));

  // ======================================
  // Start playing out an existing PCM file

  EXPECT_EQ(0, audioDevice->RegisterAudioCallback(_audioTransport));
  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audioDevice->InitPlayout());
    EXPECT_EQ(0, audioDevice->PlayoutSampleRate(&samplesPerSec));
    if (48000 == samplesPerSec)
      _audioTransport->SetFilePlayout(true, _playoutFile48.c_str());
    else if (44100 == samplesPerSec || 44000 == samplesPerSec)
      _audioTransport->SetFilePlayout(true, _playoutFile44.c_str());
    else
    {
      TEST_LOG("\nERROR: Sample rate (%d) is not supported!\n \n",
        samplesPerSec);
      return -1;
    }
    EXPECT_EQ(0, audioDevice->StartPlayout());
  }

  EXPECT_TRUE(audioDevice->Playing());
  if (audioDevice->Playing())
  {
    TEST_LOG("\n> Listen to the file being played out and verify that the"
      " selected speaker mute control is toggled between [MUTE ON] and"
      " [MUTE OFF].\n> You should only hear the file during the"
      " 'MUTE OFF' periods.\n"
      "> Press any key to stop playing...\n \n");
    PAUSE(DEFAULT_PAUSE_TIME);
  }

  EXPECT_EQ(0, audioDevice->StopPlayout());
  EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));

  _audioTransport->SetSpeakerMute(false);
  _audioTransport->SetFilePlayout(false);

  // restore mute setting
  EXPECT_EQ(0, audioDevice->SetSpeakerMute(startMute));

  TEST_LOG("\n");
  PRINT_TEST_RESULTS;

  return 0;
}

int32_t WinRTTestManager::TestMicrophoneMute()
{
  TEST_LOG("\n=======================================\n");
  TEST_LOG(" Microphone Mute test:\n");
  TEST_LOG("=======================================\n");

  std::wstring filePathW = Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data();
  std::string filePath;
  filePath.assign(filePathW.begin(), filePathW.end());

  TEST_LOG(filePath.c_str());

  if (_audioDevice == NULL)
  {
    return -1;
  }

  RESET_TEST;

  AudioDeviceModule* audioDevice = _audioDevice;

  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());

  if (SelectRecordingDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  bool available(false);
  EXPECT_EQ(0, audioDevice->MicrophoneMuteIsAvailable(&available));
  if (available)
  {
    _audioTransport->SetMicrophoneMute(true);
  }
  else
  {
    TEST_LOG("\nERROR: Mute control is not available for the selected"
      " device!\n \n");
    return -1;
  }

  if (SelectPlayoutDevice() == -1)
  {
    TEST_LOG("\nERROR: Device selection failed!\n \n");
    return -1;
  }

  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
  if (available)
  {
    _audioTransport->SetFullDuplex(true);
  }
  else
  {
    TEST_LOG("\nERROR: Playout is not available for the selected "
      "device!\n \n");
    return -1;
  }

  TEST_LOG("\nEnable recording of microphone input to file (%s) during this "
    "test (Y/N)?\n: ",
    RecordedMicrophoneMuteFile);
  //char ch;
  bool fileRecording(false);
  //EXPECT_TRUE(scanf(" %c", &ch) > 0);
  //ch = toupper(ch);
  //if (ch == 'Y')
  //{
    fileRecording = true;
  //}

  bool startMute(false);
  bool enabled(false);

  // store initial volume setting
  EXPECT_EQ(0, audioDevice->InitMicrophone());
  EXPECT_EQ(0, audioDevice->MicrophoneMute(&startMute));

  // start at no mute
  EXPECT_EQ(0, audioDevice->SetMicrophoneMute(false));

  // ==================================================================
  // Start recording from the microphone while the mic mute is toggled
  // continuously.
  // Also, start playing out the input to enable real-time verification.

  std::string recordingFile = filePath + "\\" + RecordedMicrophoneMuteFile;
  if (fileRecording)
  {
    EXPECT_EQ(0, audioDevice->StartRawInputFileRecording(recordingFile.c_str()));
  }
  EXPECT_EQ(0, audioDevice->RegisterAudioCallback(_audioTransport));
  EXPECT_EQ(0, audioDevice->RecordingIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audioDevice->InitRecording());
    EXPECT_EQ(0, audioDevice->StereoRecording(&enabled));
    if (enabled)
    {
      // ensure file recording in mono
      EXPECT_EQ(0, audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelLeft));
    }
    EXPECT_EQ(0, audioDevice->StartRecording());
  }
  EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audioDevice->InitPlayout());
    EXPECT_EQ(0, audioDevice->StartPlayout());
  }

  EXPECT_TRUE(audioDevice->Recording());
  EXPECT_TRUE(audioDevice->Playing());
  if (audioDevice->Recording() && audioDevice->Playing())
  {
    TEST_LOG("\n> Speak into the microphone and verify that the selected "
      "microphone mute control is toggled between [MUTE ON] and [MUTE OFF]."
      "\n> You should only hear your own voice in loopback during the"
      " 'MUTE OFF' periods.\n> After a finalized test (and if file "
      "recording was enabled) verify the recorded result off line.\n"
      "> Press any key to stop...\n \n");
    PAUSE(DEFAULT_PAUSE_TIME);
  }

  if (fileRecording)
  {
    EXPECT_EQ(0, audioDevice->StopRawInputFileRecording());
  }
  EXPECT_EQ(0, audioDevice->StopRecording());
  EXPECT_EQ(0, audioDevice->StopPlayout());
  EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));

  _audioTransport->SetMicrophoneMute(false);
  _audioTransport->SetFullDuplex(false);

  // restore volume setting
  EXPECT_EQ(0, audioDevice->SetMicrophoneMute(startMute));

  TEST_LOG("\n");
  PRINT_TEST_RESULTS;

  return 0;
}

int32_t WinRTTestManager::TestDeviceRemoval()
{
  TEST_LOG("\n=======================================\n");
  TEST_LOG(" Device removal test:\n");
  TEST_LOG("=======================================\n");

  if (_audioDevice == NULL)
  {
    return -1;
  }

  RESET_TEST;

  AudioDeviceModule* audioDevice = _audioDevice;

  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());

  bool recIsAvailable(false);
  bool playIsAvailable(false);
  uint8_t nPlayChannels(0);
  uint8_t nRecChannels(0);
  uint8_t loopCount(0);

  while (loopCount < 2)
  {
    if (SelectRecordingDevice() == -1)
    {
      TEST_LOG("\nERROR: Device selection failed!\n \n");
      return -1;
    }

    EXPECT_EQ(0, audioDevice->RecordingIsAvailable(&recIsAvailable));
    if (!recIsAvailable)
    {
      TEST_LOG("\nERROR: Recording is not available for the selected device!\n \n");
      return -1;
    }

    if (SelectPlayoutDevice() == -1)
    {
      TEST_LOG("\nERROR: Device selection failed!\n \n");
      return -1;
    }

    EXPECT_EQ(0, audioDevice->PlayoutIsAvailable(&playIsAvailable));
    if (recIsAvailable && playIsAvailable)
    {
      _audioTransport->SetFullDuplex(true);
    }
    else if (!playIsAvailable)
    {
      TEST_LOG("\nERROR: Playout is not available for the selected device!\n \n");
      return -1;
    }

    bool available(false);
    bool enabled(false);

    if (recIsAvailable && playIsAvailable)
    {
      uint32_t playSamplesPerSec(0);
      uint32_t recSamplesPerSecRec(0);

      EXPECT_EQ(0, audioDevice->RegisterAudioCallback(_audioTransport));

      _audioTransport->SetFullDuplex(true);

      EXPECT_EQ(0, audioDevice->StereoRecordingIsAvailable(&available));
      if (available)
      {
        EXPECT_EQ(0, audioDevice->SetStereoRecording(true));
      }

      EXPECT_EQ(0, audioDevice->StereoPlayoutIsAvailable(&available));
      if (available)
      {
        EXPECT_EQ(0, audioDevice->SetStereoPlayout(true));
      }

      EXPECT_EQ(0, audioDevice->MicrophoneVolumeIsAvailable(&available));
      if (available)
      {
        uint32_t maxVolume(0);
        EXPECT_EQ(0, audioDevice->MaxMicrophoneVolume(&maxVolume));
        EXPECT_EQ(0, audioDevice->SetMicrophoneVolume(maxVolume));
      }

      EXPECT_EQ(0, audioDevice->InitRecording());
      EXPECT_EQ(0, audioDevice->InitPlayout());
      EXPECT_EQ(0, audioDevice->PlayoutSampleRate(&playSamplesPerSec));
      EXPECT_EQ(0, audioDevice->RecordingSampleRate(&recSamplesPerSecRec));
      EXPECT_EQ(0, audioDevice->StereoPlayout(&enabled));
      enabled ? nPlayChannels = 2 : nPlayChannels = 1;
      EXPECT_EQ(0, audioDevice->StereoRecording(&enabled));
      enabled ? nRecChannels = 2 : nRecChannels = 1;
      EXPECT_EQ(0, audioDevice->StartRecording());
      EXPECT_EQ(0, audioDevice->StartPlayout());

      AudioDeviceModule::AudioLayer audioLayer;
      EXPECT_EQ(0, audioDevice->ActiveAudioLayer(&audioLayer));

      if (audioLayer == AudioDeviceModule::kLinuxPulseAudio)
      {
        TEST_LOG("\n \n> PulseAudio loopback audio is now active.\n"
          "> Rec : fs=%u, #channels=%u.\n"
          "> Play: fs=%u, #channels=%u.\n"
          "> Speak into the microphone and verify that your voice is"
          " played out in loopback.\n"
          "> Unplug the device and make sure that your voice is played"
          " out in loop back on the built-in soundcard.\n"
          "> Then press any key...\n",
          recSamplesPerSecRec, nRecChannels, playSamplesPerSec,
          nPlayChannels);

        PAUSE(DEFAULT_PAUSE_TIME);
      }
      else if (audioDevice->Playing() && audioDevice->Recording())
      {
        if (loopCount < 1)
        {
          TEST_LOG("\n \n> Loopback audio is now active.\n"
            "> Rec : fs=%u, #channels=%u.\n"
            "> Play: fs=%u, #channels=%u.\n"
            "> Speak into the microphone and verify that your voice"
            " is played out in loopback.\n"
            "> Unplug the device and wait for the error message...\n",
            recSamplesPerSecRec, nRecChannels,
            playSamplesPerSec, nPlayChannels);

          _audioEventObserver->_error
            = (AudioDeviceObserver::ErrorCode) (-1);
          while (_audioEventObserver->_error
            == (AudioDeviceObserver::ErrorCode) (-1))
          {
            SleepMs(500);
          }
        }
        else
        {
          TEST_LOG("\n \n> Loopback audio is now active.\n"
            "> Rec : fs=%u, #channels=%u.\n"
            "> Play: fs=%u, #channels=%u.\n"
            "> Speak into the microphone and verify that your voice"
            " is played out in loopback.\n"
            "> Press any key to stop...\n",
            recSamplesPerSecRec, nRecChannels,
            playSamplesPerSec, nPlayChannels);

          PAUSE(DEFAULT_PAUSE_TIME);
        }
      }

      EXPECT_EQ(0, audioDevice->StopRecording());
      EXPECT_EQ(0, audioDevice->StopPlayout());
      EXPECT_EQ(0, audioDevice->RegisterAudioCallback(NULL));

      _audioTransport->SetFullDuplex(false);

      if (loopCount < 1)
      {
        TEST_LOG("\n \n> Stopped!\n");
        TEST_LOG("> Now reinsert device if you want to enumerate it.\n");
        TEST_LOG("> Press any key when done.\n");
        PAUSE(DEFAULT_PAUSE_TIME);
      }

      loopCount++;
    }
  }  // loopCount

  EXPECT_EQ(0, audioDevice->Terminate());
  EXPECT_FALSE(audioDevice->Initialized());

  TEST_LOG("\n");
  PRINT_TEST_RESULTS;

  return 0;
}

int32_t WinRTTestManager::TestExtra()
{
  TEST_LOG("\n=======================================\n");
  TEST_LOG(" Extra test:\n");
  TEST_LOG("=======================================\n");

  if (_audioDevice == NULL)
  {
    return -1;
  }

  RESET_TEST;

  AudioDeviceModule* audioDevice = _audioDevice;

  //EXPECT_EQ(0, audioDevice->Init());
  //EXPECT_TRUE(audioDevice->Initialized());

  //EXPECT_EQ(0, audioDevice->Terminate());
  //EXPECT_FALSE(audioDevice->Initialized());
  EXPECT_FALSE(audioDevice->Initialized());
  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());
  EXPECT_EQ(0, audioDevice->Terminate());
  EXPECT_FALSE(audioDevice->Initialized());
  EXPECT_EQ(0, audioDevice->Init());
  EXPECT_TRUE(audioDevice->Initialized());
  EXPECT_EQ(0, audioDevice->Terminate());
  EXPECT_FALSE(audioDevice->Initialized());

  TEST_LOG("\n");
  PRINT_TEST_RESULTS;

  return 0;
}

#ifdef WINRT
void WinRTTestManager::waitForUserInput(int ms){

  SYSTEMTIME st;
  GetLocalTime(&st);

  TEST_LOG("waiting at %dh:%dm:%ds", st.wHour, st.wMinute,st.wSecond);
  WaitForSingleObjectEx(_semhandle, ms, FALSE);
  GetLocalTime(&st);
  TEST_LOG("wake up  at %dh:%dm:%ds", st.wHour, st.wMinute, st.wSecond);
}

void WinRTTestManager::userSignalToContinue(){
  ReleaseSemaphore(_semhandle, 1, NULL);
}
#endif