/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nsThreadUtils.h"
#include "TelemetryFixture.h"
#include "streaming/GeckoViewStreamingTelemetry.h"

using namespace mozilla;
using namespace mozilla::Telemetry;
using namespace TelemetryTestHelpers;
using GeckoViewStreamingTelemetry::StreamingTelemetryDelegate;
using mozilla::Telemetry::ScalarID;
using ::testing::_;
using ::testing::Eq;

namespace {

const char* kGeckoViewStreamingPref = "toolkit.telemetry.geckoview.streaming";
const char* kBatchTimeoutPref = "toolkit.telemetry.geckoview.batchDurationMS";

NS_NAMED_LITERAL_CSTRING(kTestHgramName, "TELEMETRY_TEST_STREAMING");
NS_NAMED_LITERAL_CSTRING(kTestHgramName2, "TELEMETRY_TEST_STREAMING_2");
NS_NAMED_LITERAL_CSTRING(kTestCategoricalName,
                         "TELEMETRY_TEST_CATEGORICAL_OPTOUT");
const HistogramID kTestHgram = Telemetry::TELEMETRY_TEST_STREAMING;
const HistogramID kTestHgram2 = Telemetry::TELEMETRY_TEST_STREAMING_2;

class TelemetryStreamingFixture : public TelemetryTestFixture {
 protected:
  virtual void SetUp() {
    TelemetryTestFixture::SetUp();
    Preferences::SetBool(kGeckoViewStreamingPref, true);
    Preferences::SetInt(kBatchTimeoutPref, 5000);
  }
  virtual void TearDown() {
    TelemetryTestFixture::TearDown();
    Preferences::SetBool(kGeckoViewStreamingPref, false);
    GeckoViewStreamingTelemetry::RegisterDelegate(nullptr);
  }
};

class MockDelegate final : public StreamingTelemetryDelegate {
 public:
  MOCK_METHOD2(ReceiveHistogramSamples,
               void(const nsCString& aHistogramName,
                    const nsTArray<uint32_t>& aSamples));
  MOCK_METHOD2(ReceiveCategoricalHistogramSamples,
               void(const nsCString& aHistogramName,
                    const nsTArray<uint32_t>& aSamples));
  MOCK_METHOD2(ReceiveBoolScalarValue,
               void(const nsCString& aScalarName, bool aValue));
  MOCK_METHOD2(ReceiveStringScalarValue,
               void(const nsCString& aScalarName, const nsCString& aValue));
  MOCK_METHOD2(ReceiveUintScalarValue,
               void(const nsCString& aScalarName, uint32_t aValue));
};  // class MockDelegate

TEST_F(TelemetryStreamingFixture, HistogramSamples) {
  const uint32_t kSampleOne = 401;
  const uint32_t kSampleTwo = 2019;

  nsTArray<uint32_t> samplesArray;
  samplesArray.AppendElement(kSampleOne);
  samplesArray.AppendElement(kSampleTwo);

  auto md = MakeRefPtr<MockDelegate>();
  EXPECT_CALL(*md, ReceiveHistogramSamples(Eq(kTestHgramName),
                                           Eq(std::move(samplesArray))));
  GeckoViewStreamingTelemetry::RegisterDelegate(md);

  Telemetry::Accumulate(Telemetry::TELEMETRY_TEST_STREAMING, kSampleOne);
  Preferences::SetInt(kBatchTimeoutPref, 0);
  Telemetry::Accumulate(Telemetry::TELEMETRY_TEST_STREAMING, kSampleTwo);
}

TEST_F(TelemetryStreamingFixture, CategoricalHistogramSamples) {
  auto kSampleOne =
      Telemetry::LABELS_TELEMETRY_TEST_CATEGORICAL_OPTOUT::CommonLabel;
  auto kSampleTwo = Telemetry::LABELS_TELEMETRY_TEST_CATEGORICAL_OPTOUT::Label5;

  nsTArray<uint32_t> samplesArray;
  samplesArray.AppendElement(static_cast<uint32_t>(kSampleOne));
  samplesArray.AppendElement(static_cast<uint32_t>(kSampleOne));
  samplesArray.AppendElement(static_cast<uint32_t>(kSampleTwo));

  auto md = MakeRefPtr<MockDelegate>();
  EXPECT_CALL(*md, ReceiveCategoricalHistogramSamples(
                       Eq(kTestCategoricalName), Eq(std::move(samplesArray))));
  GeckoViewStreamingTelemetry::RegisterDelegate(md);

  Telemetry::AccumulateCategorical(kSampleOne);
  Telemetry::AccumulateCategorical(kSampleOne);
  Preferences::SetInt(kBatchTimeoutPref, 0);
  Telemetry::AccumulateCategorical(kSampleTwo);
}

TEST_F(TelemetryStreamingFixture, MultipleHistograms) {
  const uint32_t kSample1 = 400;
  const uint32_t kSample2 = 1 << 31;
  const uint32_t kSample3 = 7;
  nsTArray<uint32_t> samplesArray1;
  samplesArray1.AppendElement(kSample1);
  samplesArray1.AppendElement(kSample2);
  nsTArray<uint32_t> samplesArray2;
  samplesArray2.AppendElement(kSample3);

  auto md = MakeRefPtr<MockDelegate>();
  EXPECT_CALL(*md, ReceiveHistogramSamples(Eq(kTestHgramName),
                                           Eq(std::move(samplesArray1))));
  EXPECT_CALL(*md, ReceiveHistogramSamples(Eq(kTestHgramName2),
                                           Eq(std::move(samplesArray2))));

  GeckoViewStreamingTelemetry::RegisterDelegate(md);

  Telemetry::Accumulate(kTestHgram, kSample1);
  Telemetry::Accumulate(kTestHgram2, kSample3);
  Preferences::SetInt(kBatchTimeoutPref, 0);
  Telemetry::Accumulate(kTestHgram, kSample2);
}

// If we can find a way to convert the expectation's arg into an stl container,
// we can use gmock's own ::testing::UnorderedElementsAre() instead.
auto MatchUnordered(uint32_t sample1, uint32_t sample2) {
  nsTArray<uint32_t> samplesArray1;
  samplesArray1.AppendElement(sample1);
  samplesArray1.AppendElement(sample2);

  nsTArray<uint32_t> samplesArray2;
  samplesArray2.AppendElement(sample2);
  samplesArray2.AppendElement(sample1);

  return ::testing::AnyOf(Eq(std::move(samplesArray1)),
                          Eq(std::move(samplesArray2)));
}

TEST_F(TelemetryStreamingFixture, MultipleThreads) {
  const uint32_t kSample1 = 4;
  const uint32_t kSample2 = 14;

  auto md = MakeRefPtr<MockDelegate>();
  // In this test, samples for the second test hgram are uninteresting.
  EXPECT_CALL(*md, ReceiveHistogramSamples(Eq(kTestHgramName2), _));
  EXPECT_CALL(*md, ReceiveHistogramSamples(Eq(kTestHgramName),
                                           MatchUnordered(kSample1, kSample2)));

  GeckoViewStreamingTelemetry::RegisterDelegate(md);

  nsCOMPtr<nsIThread> t1;
  nsCOMPtr<nsIThread> t2;
  nsCOMPtr<nsIThread> t3;

  nsCOMPtr<nsIRunnable> r1 = NS_NewRunnableFunction(
      "accumulate 4", [&]() { Telemetry::Accumulate(kTestHgram, kSample1); });
  nsCOMPtr<nsIRunnable> r2 = NS_NewRunnableFunction(
      "accumulate 14", [&]() { Telemetry::Accumulate(kTestHgram, kSample2); });

  nsresult rv = NS_NewNamedThread("t1", getter_AddRefs(t1), r1);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  rv = NS_NewNamedThread("t2", getter_AddRefs(t2), r2);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  // Give the threads a chance to do their work.
  PR_Sleep(PR_MillisecondsToInterval(1));

  Preferences::SetInt(kBatchTimeoutPref, 0);
  Telemetry::Accumulate(kTestHgram2, kSample1);
}

TEST_F(TelemetryTestFixture, StreamingTelemetryStreamScalarValues) {
  NS_NAMED_LITERAL_CSTRING(kBoolScalarName, "telemetry.test.boolean_kind");
  NS_NAMED_LITERAL_CSTRING(kStringScalarName, "telemetry.test.string_kind");
  NS_NAMED_LITERAL_CSTRING(kUintScalarName, "telemetry.test.unsigned_int_kind");

  const bool kBoolScalarValue = true;
  NS_NAMED_LITERAL_CSTRING(kStringScalarValue, "a string scalar value");
  const uint32_t kUintScalarValue = 42;

  auto md = MakeRefPtr<MockDelegate>();
  EXPECT_CALL(
      *md, ReceiveBoolScalarValue(Eq(kBoolScalarName), Eq(kBoolScalarValue)));
  EXPECT_CALL(*md, ReceiveStringScalarValue(Eq(kStringScalarName),
                                            Eq(kStringScalarValue)));
  EXPECT_CALL(
      *md, ReceiveUintScalarValue(Eq(kUintScalarName), Eq(kUintScalarValue)));

  GeckoViewStreamingTelemetry::RegisterDelegate(md);

  Preferences::SetBool(kGeckoViewStreamingPref, true);
  Preferences::SetInt(kBatchTimeoutPref, 5000);

  Telemetry::ScalarSet(ScalarID::TELEMETRY_TEST_BOOLEAN_KIND, kBoolScalarValue);
  Telemetry::ScalarSet(ScalarID::TELEMETRY_TEST_STRING_KIND,
                       NS_ConvertUTF8toUTF16(kStringScalarValue));
  Preferences::SetInt(kBatchTimeoutPref,
                      0);  // Trigger batch on next accumulation.
  Telemetry::ScalarSet(ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND,
                       kUintScalarValue);

  // Clear delegate so it can be deleted when the batch is done.
  GeckoViewStreamingTelemetry::RegisterDelegate(nullptr);
}

}  // namespace
