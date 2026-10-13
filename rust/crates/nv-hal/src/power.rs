// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum PowerSource {
    Fake,
    Sysfs,
    Degraded,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Mains {
    OnMains,
    OnBattery,
    Unknown,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PowerReading {
    pub mains: Mains,
    pub battery_pct: Option<u8>,
    pub critical: bool,
    pub source: PowerSource,
}

pub const CRITICAL_PCT: u8 = 5;

pub fn read_power() -> PowerReading {
    if let Some(fake) = read_fake() {
        return fake;
    }
    if let Some(r) = read_sysfs() {
        return r;
    }
    PowerReading {
        mains: Mains::Unknown,
        battery_pct: None,
        critical: false,
        source: PowerSource::Degraded,
    }
}

fn read_fake() -> Option<PowerReading> {
    let v = std::env::var("NV_HAL_FAKE_POWER").ok()?;
    let t = v.trim().to_lowercase();
    if t == "mains" {
        return Some(PowerReading {
            mains: Mains::OnMains,
            battery_pct: Some(100),
            critical: false,
            source: PowerSource::Fake,
        });
    }
    if t == "critical" {
        return Some(PowerReading {
            mains: Mains::OnBattery,
            battery_pct: Some(0),
            critical: true,
            source: PowerSource::Fake,
        });
    }
    if let Some(pct) = t.strip_prefix("battery:") {
        let p: u8 = pct.parse().ok()?;
        if p > 100 {
            return None;
        }
        return Some(PowerReading {
            mains: Mains::OnBattery,
            battery_pct: Some(p),
            critical: p <= CRITICAL_PCT,
            source: PowerSource::Fake,
        });
    }
    None
}

fn read_sysfs() -> Option<PowerReading> {
    let base = std::path::Path::new("/sys/class/power_supply");
    let entries = std::fs::read_dir(base).ok()?;
    let mut mains: Option<bool> = None;
    let mut pct: Option<u8> = None;
    let mut seen = false;
    for e in entries.flatten() {
        let dir = e.path();
        let typ = std::fs::read_to_string(dir.join("type")).unwrap_or_default();
        let typ = typ.trim().to_lowercase();
        if typ.contains("mains") || typ.contains("usb") || typ.contains("ac") {
            if let Ok(on) = std::fs::read_to_string(dir.join("online")) {
                mains = Some(on.trim() == "1");
                seen = true;
            }
        }
        if typ.contains("battery") || typ.contains("ups") {
            if let Ok(cap) = std::fs::read_to_string(dir.join("capacity")) {
                if let Ok(p) = cap.trim().parse::<u8>() {
                    if p <= 100 {
                        pct = Some(p);
                        seen = true;
                    }
                }
            }
            if mains.is_none() {
                if let Ok(st) = std::fs::read_to_string(dir.join("status")) {
                    let s = st.trim().to_lowercase();
                    if s.contains("discharging") {
                        mains = Some(false);
                    } else if s.contains("charging") || s.contains("full") {
                        mains = Some(true);
                    }
                    seen = true;
                }
            }
        }
    }
    if !seen {
        return None;
    }
    let on_battery = mains == Some(false);
    let critical = on_battery && pct.unwrap_or(100) <= CRITICAL_PCT;
    Some(PowerReading {
        mains: match mains {
            Some(true) => Mains::OnMains,
            Some(false) => Mains::OnBattery,
            None => Mains::Unknown,
        },
        battery_pct: pct,
        critical,
        source: PowerSource::Sysfs,
    })
}

pub fn should_quiesce(r: &PowerReading) -> bool {
    r.critical
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct QuiescePlan {
    pub reason: String,
    pub drain_secs_estimate: Option<u32>,
}

pub fn quiesce_plan(r: &PowerReading) -> Option<QuiescePlan> {
    if !should_quiesce(r) {
        return None;
    }
    Some(QuiescePlan {
        reason: format!(
            "energia crítica (fonte {:?}, bateria {:?})",
            r.mains, r.battery_pct
        ),
        drain_secs_estimate: r.battery_pct.map(|p| (p as u32) * 60),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fake_mains_ok() {
        std::env::set_var("NV_HAL_FAKE_POWER", "mains");
        let r = read_power();
        assert_eq!(r.mains, Mains::OnMains);
        assert!(!should_quiesce(&r));
        std::env::remove_var("NV_HAL_FAKE_POWER");
    }

    #[test]
    fn fake_critical_quiesces() {
        std::env::set_var("NV_HAL_FAKE_POWER", "critical");
        let r = read_power();
        assert!(r.critical);
        assert!(should_quiesce(&r));
        let plan = quiesce_plan(&r).unwrap();
        assert!(plan.reason.contains("crítica"));
        std::env::remove_var("NV_HAL_FAKE_POWER");
    }

    #[test]
    fn fake_battery_threshold() {
        std::env::set_var("NV_HAL_FAKE_POWER", "battery:20");
        assert!(!should_quiesce(&read_power()));
        std::env::set_var("NV_HAL_FAKE_POWER", "battery:5");
        assert!(should_quiesce(&read_power()));
        std::env::remove_var("NV_HAL_FAKE_POWER");
    }

    #[test]
    fn degraded_never_forces() {
        std::env::remove_var("NV_HAL_FAKE_POWER");
        let r = PowerReading {
            mains: Mains::Unknown,
            battery_pct: None,
            critical: false,
            source: PowerSource::Degraded,
        };
        assert!(!should_quiesce(&r));
        assert!(quiesce_plan(&r).is_none());
    }
}
