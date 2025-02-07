/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkletPrincipals.h"

#include "mozilla/Assertions.h"

namespace mozilla {
namespace dom {

WorkletPrincipals::WorkletPrincipals(WorkletImpl* aWorkletImpl)
    : JSPrincipals(), mWorkletImpl(aWorkletImpl) {
  setDebugToken(kJSPrincipalsDebugToken);
}

WorkletPrincipals::~WorkletPrincipals() = default;

bool WorkletPrincipals::write(JSContext* aCx,
                              JSStructuredCloneWriter* aWriter) {
  MOZ_CRASH("WorkletPrincipals::write not implemented");
  return false;
}

void WorkletPrincipals::Destroy(JSPrincipals* aPrincipals) {
  delete static_cast<WorkletPrincipals*>(aPrincipals);
}

}  // namespace dom
}  // namespace mozilla
