#include <gtest/gtest.h>
#include <MeshCore.h>

class FakeRTCClock : public mesh::RTCClock {
public:
  uint32_t now = 0;
  unsigned writes = 0;

  uint32_t getCurrentTime() override { return now; }
  void setCurrentTime(uint32_t time) override { now = time; ++writes; }
};

using SyncSource = mesh::RTCClock::SyncSource;

TEST(RTCClockSync, RestoredClockIsNotAnExternalSync) {
  FakeRTCClock rtc;
  rtc.setCurrentTime(1788900000);
  EXPECT_EQ(SyncSource::None, rtc.getLastSyncSource());
  EXPECT_EQ(0u, rtc.getLastSyncTime());
}

TEST(RTCClockSync, RecordsOnlyTheLatestAppliedReference) {
  FakeRTCClock rtc;
  rtc.setCurrentTimeFromSource(1788900000, SyncSource::Companion);
  EXPECT_EQ(1u, rtc.writes);
  EXPECT_EQ(1788900000u, rtc.now);
  EXPECT_EQ(SyncSource::Companion, rtc.getLastSyncSource());
  EXPECT_EQ(1788900000u, rtc.getLastSyncTime());

  // A GPS correction can legitimately move the clock backwards.
  rtc.setCurrentTimeFromSource(1788899940, SyncSource::GPS);
  EXPECT_EQ(2u, rtc.writes);
  EXPECT_EQ(1788899940u, rtc.now);
  EXPECT_EQ(SyncSource::GPS, rtc.getLastSyncSource());
  EXPECT_EQ(1788899940u, rtc.getLastSyncTime());

  rtc.now += 86400 * 60;
  EXPECT_EQ(86400u * 60, rtc.getCurrentTime() - rtc.getLastSyncTime());

  rtc.setCurrentTimeFromSource(1794100000, SyncSource::Companion);
  EXPECT_EQ(SyncSource::Companion, rtc.getLastSyncSource());
  EXPECT_EQ(1794100000u, rtc.getLastSyncTime());
}

TEST(RTCClockSync, InvalidOrUnknownReferencesDoNotClaimSynchronization) {
  FakeRTCClock rtc;
  rtc.setCurrentTimeFromSource(1788900000, SyncSource::GPS);
  rtc.setCurrentTimeFromSource(0, SyncSource::Companion);
  EXPECT_EQ(0u, rtc.now);
  EXPECT_EQ(SyncSource::None, rtc.getLastSyncSource());
  EXPECT_EQ(0u, rtc.getLastSyncTime());

  rtc.setCurrentTimeFromSource(946684799, SyncSource::GPS);
  EXPECT_EQ(SyncSource::None, rtc.getLastSyncSource());
  rtc.setCurrentTimeFromSource(946684800, SyncSource::GPS);
  EXPECT_EQ(SyncSource::GPS, rtc.getLastSyncSource());

  rtc.setCurrentTimeFromSource(1788900000, SyncSource::None);
  EXPECT_EQ(SyncSource::None, rtc.getLastSyncSource());
  EXPECT_EQ(0u, rtc.getLastSyncTime());
}

TEST(RTCClockSync, RebootStartsWithUnknownSource) {
  FakeRTCClock first_boot;
  first_boot.setCurrentTimeFromSource(1788900000, SyncSource::Companion);
  FakeRTCClock second_boot;
  second_boot.setCurrentTime(first_boot.now);
  EXPECT_EQ(SyncSource::None, second_boot.getLastSyncSource());
  EXPECT_EQ(0u, second_boot.getLastSyncTime());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
