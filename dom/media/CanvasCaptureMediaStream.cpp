/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasCaptureMediaStream.h"

#include "DOMMediaStream.h"
#include "ImageContainer.h"
#include "MediaStreamGraph.h"
#include "gfxPlatform.h"
#include "mozilla/Atomics.h"
#include "mozilla/dom/CanvasCaptureMediaStreamBinding.h"
#include "mozilla/gfx/2D.h"
#include "nsContentUtils.h"
#include "Tracing.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

OutputStreamDriver::OutputStreamDriver(SourceMediaStream* aSourceStream,
                                       const TrackID& aTrackId,
                                       const PrincipalHandle& aPrincipalHandle)
    : FrameCaptureListener(),
      mTrackId(aTrackId),
      mSourceStream(aSourceStream),
      mPrincipalHandle(aPrincipalHandle) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSourceStream);
  MOZ_ASSERT(IsTrackIDExplicit(mTrackId));
  mSourceStream->AddTrack(aTrackId, new VideoSegment());

  // All CanvasCaptureMediaStreams shall at least get one frame.
  mFrameCaptureRequested = true;
}

OutputStreamDriver::~OutputStreamDriver() {
  MOZ_ASSERT(NS_IsMainThread());
  EndTrack();
}

void OutputStreamDriver::EndTrack() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mSourceStream->IsDestroyed()) {
    mSourceStream->Destroy();
  }
}

void OutputStreamDriver::SetImage(const RefPtr<layers::Image>& aImage,
                                  const TimeStamp& aTime) {
  MOZ_ASSERT(NS_IsMainThread());

  TRACE_COMMENT("SourceMediaStream %p track %i", mSourceStream.get(), mTrackId);

  VideoSegment segment;
  segment.AppendFrame(do_AddRef(aImage), aImage->GetSize(), mPrincipalHandle,
                      false, aTime);
  mSourceStream->AppendToTrack(mTrackId, &segment);
}

// ----------------------------------------------------------------------

class TimerDriver : public OutputStreamDriver {
 public:
  explicit TimerDriver(SourceMediaStream* aSourceStream, const double& aFPS,
                       const TrackID& aTrackId,
                       const PrincipalHandle& aPrincipalHandle)
      : OutputStreamDriver(aSourceStream, aTrackId, aPrincipalHandle),
        mFPS(aFPS),
        mTimer(nullptr) {
    if (mFPS == 0.0) {
      return;
    }

    NS_NewTimerWithFuncCallback(
        getter_AddRefs(mTimer), &TimerTick, this, int(1000 / mFPS),
        nsITimer::TYPE_REPEATING_SLACK, "dom::TimerDriver::TimerDriver");
  }

  static void TimerTick(nsITimer* aTimer, void* aClosure) {
    MOZ_ASSERT(aClosure);
    TimerDriver* driver = static_cast<TimerDriver*>(aClosure);

    driver->RequestFrameCapture();
  }

  void NewFrame(already_AddRefed<Image> aImage,
                const TimeStamp& aTime) override {
    RefPtr<Image> image = aImage;

    if (!mFrameCaptureRequested) {
      return;
    }

    mFrameCaptureRequested = false;
    SetImage(image.forget(), aTime);
  }

  void Forget() override {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
  }

 protected:
  virtual ~TimerDriver() {}

 private:
  const double mFPS;
  nsCOMPtr<nsITimer> mTimer;
};

// ----------------------------------------------------------------------

class AutoDriver : public OutputStreamDriver {
 public:
  explicit AutoDriver(SourceMediaStream* aSourceStream, const TrackID& aTrackId,
                      const PrincipalHandle& aPrincipalHandle)
      : OutputStreamDriver(aSourceStream, aTrackId, aPrincipalHandle) {}

  void NewFrame(already_AddRefed<Image> aImage,
                const TimeStamp& aTime) override {
    // Don't reset `mFrameCaptureRequested` since AutoDriver shall always have
    // `mFrameCaptureRequested` set to true.
    // This also means we should accept every frame as NewFrame is called only
    // after something changed.

    RefPtr<Image> image = aImage;
    SetImage(image.forget(), aTime);
  }

 protected:
  virtual ~AutoDriver() {}
};

// ----------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_INHERITED(CanvasCaptureMediaStream, DOMMediaStream,
                                   mCanvas)

NS_IMPL_ADDREF_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanvasCaptureMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

CanvasCaptureMediaStream::CanvasCaptureMediaStream(nsPIDOMWindowInner* aWindow,
                                                   HTMLCanvasElement* aCanvas)
    : DOMMediaStream(aWindow), mCanvas(aCanvas) {}

CanvasCaptureMediaStream::~CanvasCaptureMediaStream() {
  if (mOutputStreamDriver) {
    mOutputStreamDriver->Forget();
  }
}

JSObject* CanvasCaptureMediaStream::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return dom::CanvasCaptureMediaStream_Binding::Wrap(aCx, this, aGivenProto);
}

void CanvasCaptureMediaStream::RequestFrame() {
  if (mOutputStreamDriver) {
    mOutputStreamDriver->RequestFrameCapture();
  }
}

nsresult CanvasCaptureMediaStream::Init(const dom::Optional<double>& aFPS,
                                        const TrackID aTrackId,
                                        nsIPrincipal* aPrincipal) {
  MediaStreamGraph* graph = MediaStreamGraph::GetInstance(
      MediaStreamGraph::SYSTEM_THREAD_DRIVER, mWindow,
      MediaStreamGraph::REQUEST_DEFAULT_SAMPLE_RATE);
  SourceMediaStream* source = graph->CreateSourceStream();
  PrincipalHandle principalHandle = MakePrincipalHandle(aPrincipal);
  if (!aFPS.WasPassed()) {
    mOutputStreamDriver = new AutoDriver(source, aTrackId, principalHandle);
  } else if (aFPS.Value() < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  } else {
    // Cap frame rate to 60 FPS for sanity
    double fps = std::min(60.0, aFPS.Value());
    mOutputStreamDriver =
        new TimerDriver(source, fps, aTrackId, principalHandle);
  }
  return NS_OK;
}

FrameCaptureListener* CanvasCaptureMediaStream::FrameCaptureListener() {
  return mOutputStreamDriver;
}

void CanvasCaptureMediaStream::StopCapture() {
  if (!mOutputStreamDriver) {
    return;
  }

  mOutputStreamDriver->EndTrack();
  mOutputStreamDriver->Forget();
  mOutputStreamDriver = nullptr;
}

SourceMediaStream* CanvasCaptureMediaStream::GetSourceStream() const {
  if (!mOutputStreamDriver) {
    return nullptr;
  }
  return mOutputStreamDriver->mSourceStream;
}

}  // namespace dom
}  // namespace mozilla
