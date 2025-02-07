// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-json.stringify
description: Proxy of an array is treated as an array
info: |
  [...]
  12. Return ? SerializeJSONProperty(the empty String, wrapper).

  24.3.2.1 Runtime Semantics: SerializeJSONProperty

  [...]
  10. If Type(value) is Object and IsCallable(value) is false, then
      a. Let isArray be ? IsArray(value).
      b. If isArray is true, return ? SerializeJSONArray(value).
      c. Else, return ? SerializeJSONObject(value).
  [...]

  7.2.2 IsArray

  [...]
  3. If argument is a Proxy exotic object, then
     a. If the value of the [[ProxyHandler]] internal slot of argument is null,
        throw a TypeError exception.
     b. Let target be the value of the [[ProxyTarget]] internal slot of
        argument.
     c. Return ? IsArray(target).
features: [Proxy]
---*/

var objectProxy = new Proxy({}, {});
var arrayProxy = new Proxy([], {});
var arrayProxyProxy = new Proxy(arrayProxy, {});

assert.sameValue(JSON.stringify(objectProxy), '{}');
assert.sameValue(JSON.stringify(arrayProxy), '[]', 'proxy for an array');
assert.sameValue(
  JSON.stringify([[arrayProxy]]), '[[[]]]', 'proxy for an array (nested)'
);
assert.sameValue(
  JSON.stringify([[arrayProxyProxy]]),
  '[[[]]]',
  'proxy for a proxy for an array (nested)'
);

reportCompare(0, 0);
