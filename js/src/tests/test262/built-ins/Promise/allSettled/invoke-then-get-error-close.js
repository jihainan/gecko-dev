// |reftest| skip-if(release_or_beta) -- Promise.allSettled is not released yet
// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Error thrown when accesing the instance's `then` method (closing iterator)
esid: sec-promise.allsettled
info: |
  6. Let result be PerformPromiseAllSettled(iteratorRecord, C, promiseCapability).
  7. If result is an abrupt completion, then
    a. If iteratorRecord.[[Done]] is false, set result to IteratorClose(iteratorRecord, result).
    b. IfAbruptRejectPromise(result, promiseCapability).

  Runtime Semantics: PerformPromiseAllSettled

  z. Perform ? Invoke(nextPromise, "then", « resolveElement, rejectElement »).
features: [Promise.allSettled, Symbol.iterator]
---*/

var promise = new Promise(function() {});
var returnCount = 0;
var iter = {};
iter[Symbol.iterator] = function() {
  return {
    next() {
      return {
        done: false,
        value: promise
      };
    },
    return() {
      returnCount += 1;
      return {};
    }
  };
};

Object.defineProperty(promise, 'then', {
  get() {
    throw new Test262Error();
  }
});

Promise.allSettled(iter);

assert.sameValue(returnCount, 1);

reportCompare(0, 0);
