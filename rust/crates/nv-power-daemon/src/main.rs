// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
//! PROTÓTIPO DE LABORATÓRIO. Laço simples, sem `unsafe`:

use nv_hal::{hw_log, power, rtc};
use std::path::PathBuf;
use std::time::Duration;

fn main() {
    let workdir = std::env::args().nth(1).unwrap_or_else(|| ".".to_string());
    let code = run(&workdir);
    std::process::exit(code);
}

fn run(workdir: &str) -> i32 {
    let log_path = PathBuf::from(workdir).join("hw-events.jsonl");
    let log = match hw_log::HwLog::open(&log_path) {
        Ok(l) => l,
        Err(e) => {
            eprintln!("bordo indisponível: {e}");
            return 3;
        }
    };
    let ts = rtc::iso_utc(rtc::read_time().epoch_secs);
    let _ = log.append(&ts, hw_log::event::BOOT, "nv-power-daemon");
    let _ = check_rtc(&log);

    let mut was_battery = false;
    loop {
        if std::env::var("NV_HAL_STOP").as_deref() == Ok("1")
            || PathBuf::from(workdir).join("STOP").exists()
        {
            return 0;
        }
        let reading = power::read_power();
        let on_battery = reading.mains == power::Mains::OnBattery;
        if on_battery && !was_battery {
            let ts = rtc::iso_utc(rtc::read_time().epoch_secs);
            let _ = log.append(&ts, hw_log::event::MAINS_LOST, "rede perdida");
        }
        was_battery = on_battery;
        if power::should_quiesce(&reading) {
            let ts = rtc::iso_utc(rtc::read_time().epoch_secs);
            let plan = power::quiesce_plan(&reading);
            let _ = log.append(
                &ts,
                hw_log::event::POWER_CRITICAL,
                &plan
                    .map(|p| p.reason)
                    .unwrap_or_else(|| "crítica".to_string()),
            );
            let _ = std::fs::write(PathBuf::from(workdir).join("QUIESCE"), "power_critical\n");
            return 2;
        }
        std::thread::sleep(poll_interval());
    }
}

fn poll_interval() -> Duration {
    if let Ok(v) = std::env::var("NV_HAL_POLL_MS") {
        if let Ok(ms) = v.parse::<u64>() {
            if (50..=60_000).contains(&ms) {
                return Duration::from_millis(ms);
            }
        }
    }
    Duration::from_secs(5)
}

fn check_rtc(log: &hw_log::HwLog) -> Result<(), ()> {
    let t = rtc::read_time();
    if !t.trusted {
        let now = rtc::iso_utc(t.epoch_secs);
        let _ = log.append(
            &now,
            hw_log::event::RTC_UNTRUSTED,
            "tempo do sistema, sem RTC",
        );
    }
    Ok(())
}
