// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use std::fs::OpenOptions;
use std::io::Write;
use std::path::{Path, PathBuf};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum HwLogError {
    #[error("io: {0}")]
    Io(String),
}

pub mod event {
    pub const BOOT: &str = "boot";
    pub const MAINS_LOST: &str = "power_mains_lost";
    pub const POWER_CRITICAL: &str = "power_critical";
    pub const QUIESCE_DONE: &str = "quiesce_done";
    pub const RTC_UNTRUSTED: &str = "rtc_untrusted";
    pub const PRINT_OK: &str = "print_ok";
    pub const PRINT_FAIL: &str = "print_fail";
}

pub struct HwLog {
    path: PathBuf,
}

impl HwLog {
    pub fn open(path: &Path) -> Result<Self, HwLogError> {
        if let Some(parent) = path.parent() {
            if !parent.as_os_str().is_empty() {
                std::fs::create_dir_all(parent).map_err(|e| HwLogError::Io(e.to_string()))?;
            }
        }
        Ok(Self {
            path: path.to_path_buf(),
        })
    }

    pub fn path(&self) -> &Path {
        &self.path
    }

    pub fn append(&self, ts_iso: &str, evento: &str, detalhe: &str) -> Result<(), HwLogError> {
        let line = format!(
            "{{\"ts\":\"{}\",\"evento\":\"{}\",\"detalhe\":\"{}\"}}\n",
            escape(ts_iso),
            escape(evento),
            escape(detalhe)
        );
        let mut f = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&self.path)
            .map_err(|e| HwLogError::Io(e.to_string()))?;
        f.write_all(line.as_bytes())
            .map_err(|e| HwLogError::Io(e.to_string()))?;
        f.flush().map_err(|e| HwLogError::Io(e.to_string()))?;
        f.sync_all().map_err(|e| HwLogError::Io(e.to_string()))?;
        Ok(())
    }
}

fn escape(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn append_and_read_back() {
        let dir = std::env::temp_dir().join(format!("nv-hwlog-{}", std::process::id()));
        let p = dir.join("hw-events.jsonl");
        let _ = std::fs::remove_file(&p);
        let log = HwLog::open(&p).unwrap();
        log.append("2026-10-13T08:00:00", event::BOOT, "nv-te-01")
            .unwrap();
        log.append("2026-10-13T08:01:00", event::MAINS_LOST, "rede caiu")
            .unwrap();
        let text = std::fs::read_to_string(&p).unwrap();
        assert_eq!(text.lines().count(), 2);
        assert!(text.contains("\"evento\":\"boot\""));
        let _ = std::fs::remove_file(&p);
    }

    #[test]
    fn escape_quotes() {
        assert_eq!(escape("a\"b\\c"), "a\\\"b\\\\c");
    }
}
