// Copyright 2010 Google Inc. All Rights Reserved,
//
// Author: Justin Uberti (juberti@google.com)

#ifndef TALK_P2P_CLIENT_FAKEPORTALLOCATOR_H_
#define TALK_P2P_CLIENT_FAKEPORTALLOCATOR_H_

#include <string>
#include "talk/p2p/base/basicpacketsocketfactory.h"
#include "talk/p2p/base/portallocator.h"
#include "talk/p2p/base/udpport.h"
#include "webrtc/base/scoped_ptr.h"

namespace rtc {
class SocketFactory;
class Thread;
}

namespace cricket {

class FakePortAllocatorSession : public PortAllocatorSession {
 public:
  FakePortAllocatorSession(rtc::Thread* worker_thread,
                           rtc::PacketSocketFactory* factory,
                           const std::string& content_name,
                           int component,
                           const std::string& ice_ufrag,
                           const std::string& ice_pwd)
      : PortAllocatorSession(content_name, component, ice_ufrag, ice_pwd,
                             cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG),
        worker_thread_(worker_thread),
        factory_(factory),
        network_("network", "unittest",
                 rtc::IPAddress(INADDR_LOOPBACK), 8),
        port_(), running_(false),
        port_config_count_(0) {
    network_.AddIP(rtc::IPAddress(INADDR_LOOPBACK));
  }

  virtual void StartGettingPorts() {
    if (!port_) {
      port_.reset(cricket::UDPPort::Create(worker_thread_, factory_,
                      &network_, network_.ip(), 0, 0,
                      username(),
                      password()));
      AddPort(port_.get());
    }
    ++port_config_count_;
    running_ = true;
  }

  virtual void StopGettingPorts() { running_ = false; }
  virtual bool IsGettingPorts() { return running_; }
  int port_config_count() { return port_config_count_; }

  void AddPort(cricket::Port* port) {
    port->set_component(component_);
    port->set_generation(0);
    port->SignalPortComplete.connect(
        this, &FakePortAllocatorSession::OnPortComplete);
    port->PrepareAddress();
    SignalPortReady(this, port);
  }
  void OnPortComplete(cricket::Port* port) {
    SignalCandidatesReady(this, port->Candidates());
    SignalCandidatesAllocationDone(this);
  }

 private:
  rtc::Thread* worker_thread_;
  rtc::PacketSocketFactory* factory_;
  rtc::Network network_;
  rtc::scoped_ptr<cricket::Port> port_;
  bool running_;
  int port_config_count_;
};

class FakePortAllocator : public cricket::PortAllocator {
 public:
  FakePortAllocator(rtc::Thread* worker_thread,
                    rtc::PacketSocketFactory* factory)
      : worker_thread_(worker_thread), factory_(factory) {
    if (factory_ == NULL) {
      owned_factory_.reset(new rtc::BasicPacketSocketFactory(
          worker_thread_));
      factory_ = owned_factory_.get();
    }
  }

  virtual cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) {
    return new FakePortAllocatorSession(
        worker_thread_, factory_, content_name, component, ice_ufrag, ice_pwd);
  }

 private:
  rtc::Thread* worker_thread_;
  rtc::PacketSocketFactory* factory_;
  rtc::scoped_ptr<rtc::BasicPacketSocketFactory> owned_factory_;
};

}  // namespace cricket

#endif  // TALK_P2P_CLIENT_FAKEPORTALLOCATOR_H_