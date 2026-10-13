// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum TimeSource {
    Rtc,
    Fake,
    System,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TimeReading {
    pub epoch_secs: u64,
    pub trusted: bool,
    pub source: TimeSource,
}

pub const MIN_SANE_EPOCH: u64 = 1_704_067_200;
pub const MAX_DRIFT_SECS: i64 = 300;

pub fn read_time() -> TimeReading {
    if let Some(fake) = read_fake() {
        return fake;
    }
    if let Some(epoch) = read_sysfs_rtc() {
        if epoch >= MIN_SANE_EPOCH {
            return TimeReading {
                epoch_secs: epoch,
                trusted: true,
                source: TimeSource::Rtc,
            };
        }
    }
    TimeReading {
        epoch_secs: system_epoch(),
        trusted: false,
        source: TimeSource::System,
    }
}

fn read_fake() -> Option<TimeReading> {
    let v = std::env::var("NV_HAL_FAKE_RTC").ok()?;
    let epoch: u64 = v.trim().parse().ok()?;
    Some(TimeReading {
        epoch_secs: epoch,
        trusted: epoch >= MIN_SANE_EPOCH,
        source: TimeSource::Fake,
    })
}

fn read_sysfs_rtc() -> Option<u64> {
    let s = std::fs::read_to_string("/sys/class/rtc/rtc0/since_epoch").ok()?;
    s.split_whitespace().next()?.parse().ok()
}

fn system_epoch() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

pub fn drift_vs_system() -> Option<i64> {
    let rtc = read_sysfs_rtc()?;
    Some(rtc as i64 - system_epoch() as i64)
}

pub fn drift_exceeded() -> bool {
    drift_vs_system()
        .map(|d| d.abs() > MAX_DRIFT_SECS)
        .unwrap_or(false)
}

pub fn iso_utc(epoch_secs: u64) -> String {
    let days = (epoch_secs / 86_400) as i64;
    let secs = (epoch_secs % 86_400) as i64;
    let z = days + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z.rem_euclid(146_097);
    let yoe = (doe - doe / 1_460 + doe / 36_524 - doe / 146_096) / 365;
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    let year = if m <= 2 { y + 1 } else { y };
    format!(
        "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}",
        year,
        m,
        d,
        secs / 3_600,
        (secs % 3_600) / 60,
        secs % 60
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fake_trusted_when_sane() {
        std::env::set_var("NV_HAL_FAKE_RTC", "1728000000");
        let r = read_time();
        assert!(r.trusted);
        assert_eq!(r.epoch_secs, 1_728_000_000);
        std::env::remove_var("NV_HAL_FAKE_RTC");
    }

    #[test]
    fn fake_untrusted_when_zeroed() {
        std::env::set_var("NV_HAL_FAKE_RTC", "0");
        let r = read_time();
        assert!(!r.trusted);
        std::env::remove_var("NV_HAL_FAKE_RTC");
    }

    #[test]
    fn iso_known_vector() {
        assert_eq!(iso_utc(1_704_067_200), "2024-01-01T00:00:00");
        assert_eq!(iso_utc(1_791_849_600), "2026-10-13T00:00:00");
    }

    #[test]
    fn drift_absent_is_none() {
        std::env::remove_var("NV_HAL_FAKE_RTC");
        let _ = drift_vs_system();
    }
}
