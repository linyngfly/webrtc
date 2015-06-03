﻿
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.


#ifndef WEBRTC_BUILD_WINRT_GYP_API_PEERCONNECTIONINTERFACE_H_
#define WEBRTC_BUILD_WINRT_GYP_API_PEERCONNECTIONINTERFACE_H_

#include <collection.h>
#include <vector>
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scopedptrcollection.h"
#include "GlobalObserver.h"
#include "DataChannel.h"

using Platform::String;
using Platform::IBox;
using Windows::Foundation::Collections::IVector;
using Windows::Foundation::IAsyncOperation;
using Windows::Foundation::IAsyncAction;
using webrtc_winrt_api_internal::CreateSdpObserver;
using webrtc_winrt_api_internal::SetSdpObserver;
using webrtc_winrt_api_internal::DataChannelObserver;
using webrtc_winrt_api_internal::GlobalObserver;

namespace webrtc_winrt_api {

ref class MediaStream;
ref class MediaStreamTrack;

[Windows::Foundation::Metadata::WebHostHidden]
public ref class WebRTC sealed {
public:
  static void Initialize(Windows::UI::Core::CoreDispatcher^ dispatcher);

private:
  // This type is not meant to be created.
  WebRTC();
};

public enum class RTCBundlePolicy {
  kBundlePolicyBalanced,
  kBundlePolicyMaxBundle,
  kBundlePolicyMaxCompat,
};

public enum class RTCIceTransportPolicy {
  kNone,
  kRelay,
  kNoHost,
  kAll
};

public enum class RTCSdpType {
  offer,
  pranswer,
  answer,
};

public enum class RTCSignalingState {
  stable,
  haveLocalOffer,
  haveRemoteOffer,
  haveLocalPranswer,
  haveRemotePranswer,
  closed
};

public ref class RTCIceServer sealed {
public:
  property String^ Url;
  property String^ Username;
  property String^ Credential;
};

public ref class RTCConfiguration sealed {
public:
  property IVector<RTCIceServer^>^ IceServers;
  property IBox<RTCIceTransportPolicy>^ IceTransportPolicy;
  property IBox<RTCBundlePolicy>^ BundlePolicy;
  // TODO(WINRT): DOMString PeerIdentity
};

public ref class RTCIceCandidate sealed {
public:
  RTCIceCandidate();
  RTCIceCandidate(String^ candidate, String^ sdpMid,
    uint16 sdpMLineIndex);
  property String^ Candidate;
  property String^ SdpMid;
  property uint16 SdpMLineIndex;
};

public ref class RTCSessionDescription sealed {
public:
  RTCSessionDescription();
  RTCSessionDescription(RTCSdpType type, String^ sdp);
  property IBox<RTCSdpType>^ Type;
  property String^ Sdp;
};

// Events and delegates
// ------------------
public ref class RTCPeerConnectionIceEvent sealed {
public:
  property RTCIceCandidate^ Candidate;
};

// ------------------
public ref class MediaStreamEvent sealed {
public:
  property MediaStream^ Stream;
};

public ref class RTCPeerConnection sealed {
public:
  // Required so the observer can raise events in this class.
  // By default event raising is protected.
  friend class GlobalObserver;

  RTCPeerConnection(RTCConfiguration^ configuration);

  event RTCPeerConnectionIceEventDelegate^ OnIceCandidate;
  event MediaStreamEventEventDelegate^ OnAddStream;
  event MediaStreamEventEventDelegate^ OnRemoveStream;
  event EventDelegate^ OnNegotiationNeeded;
  event EventDelegate^ OnSignalingStateChange;
  event RTCDataChannelEventDelegate^ OnDataChannel;

  IAsyncOperation<RTCSessionDescription^>^ CreateOffer();
  IAsyncOperation<RTCSessionDescription^>^ CreateAnswer();
  IAsyncAction^ SetLocalDescription(RTCSessionDescription^ description);
  IAsyncAction^ SetRemoteDescription(RTCSessionDescription^ description);
  void AddStream(MediaStream^ stream);
  RTCDataChannel^ CreateDataChannel(String^ label, RTCDataChannelInit^ init);
  IAsyncAction^ AddIceCandidate(RTCIceCandidate^ candidate);

  property RTCSessionDescription^ LocalDescription {
    RTCSessionDescription^ get();
  }
  property RTCSessionDescription^ RemoteDescription {
    RTCSessionDescription^ get();
  }
  property RTCSignalingState SignalingState {
    RTCSignalingState get();
  }

private:
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> _impl;
  GlobalObserver _observer;

  typedef std::vector<rtc::scoped_refptr<CreateSdpObserver>> CreateSdpObservers;
  typedef std::vector<rtc::scoped_refptr<SetSdpObserver>> SetSdpObservers;
  typedef rtc::ScopedPtrCollection<DataChannelObserver> DataChannelObservers;
  CreateSdpObservers _createSdpObservers;
  SetSdpObservers _setSdpObservers;
  DataChannelObservers _dataChannelObservers;
};

namespace globals {
extern rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
  gPeerConnectionFactory;
// The worker thread for webrtc.
extern rtc::Thread gThread;

template <typename T>
T RunOnGlobalThread(std::function<T()> fn) {
  return gThread.Invoke<T, std::function<T()>>(fn);
}

}  // namespace globals

}  // namespace webrtc_winrt_api

#endif  // WEBRTC_BUILD_WINRT_GYP_API_PEERCONNECTIONINTERFACE_H_
