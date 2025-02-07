/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * https://wiki.whatwg.org/wiki/OffscreenCanvas
 */

[Exposed=(Window,Worker),
 Pref="gfx.offscreencanvas.enabled"]
interface OffscreenCanvas : EventTarget {
  constructor(unsigned long width, unsigned long height);

  [Pure, SetterThrows]
  attribute unsigned long width;
  [Pure, SetterThrows]
  attribute unsigned long height;

  [Throws]
  nsISupports? getContext(DOMString contextId,
                          optional any contextOptions = null);

  [Throws]
  ImageBitmap transferToImageBitmap();
  [Throws]
  Promise<Blob> toBlob(optional DOMString type = "",
                       optional any encoderOptions);
};

// OffscreenCanvas implements Transferable;
