// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#![forbid(unsafe_code)]

pub mod beep;
pub mod display_hw;
pub mod hw_log;
pub mod keypad_evdev;
pub mod power;
pub mod printer;
pub mod rtc;

use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum HalError {
    #[error("io: {0}")]
    Io(String),
    #[error("tecla inválida: {0}")]
    BadKey(String),
    #[error("placa não suportada: {0}")]
    UnsupportedBoard(String),
    #[error("energia crítica: {0}")]
    Power(String),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Key {
    Digit(u8),
    Branco,
    Corrige,
    Confirma,
    CancelaMesario,
}

impl Key {
    pub fn parse(raw: &str) -> Result<Self, HalError> {
        let t = raw.trim().to_uppercase();
        match t.as_str() {
            "BRANCO" | "B" | "A" => Ok(Key::Branco),
            "CORRIGE" | "C" => Ok(Key::Corrige),
            "CONFIRMA" | "ENTER" | "#" => Ok(Key::Confirma),
            "CANCELA" | "CANCELA-MESARIO" | "*" => Ok(Key::CancelaMesario),
            s if s.len() == 1 && s.as_bytes()[0].is_ascii_digit() => {
                Ok(Key::Digit(s.as_bytes()[0] - b'0'))
            }
            other => Err(HalError::BadKey(other.to_string())),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DisplayState {
    pub lines: Vec<String>,
    pub brightness: u8,
}

impl DisplayState {
    pub fn new() -> Self {
        Self {
            lines: Vec::new(),
            brightness: 80,
        }
    }

    pub fn set_lines(&mut self, lines: Vec<String>) -> Result<(), HalError> {
        if lines.len() > 8 {
            return Err(HalError::Io("display: máx 8 linhas".into()));
        }
        for l in &lines {
            if l.len() > 64 {
                return Err(HalError::Io("display: linha > 64 chars".into()));
            }
        }
        self.lines = lines;
        Ok(())
    }
}

impl Default for DisplayState {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum PowerState {
    Ok,
    OnBattery(u32),
    Critical,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BoardInfo {
    pub model: String,
    pub arch: String,
    pub supported: bool,
}

impl BoardInfo {
    pub fn detect() -> Self {
        let model = std::fs::read_to_string("/proc/device-tree/model")
            .map(|s| s.trim_matches('\0').trim().to_string())
            .unwrap_or_else(|_| "LAB-HOST (sem device-tree)".to_string());
        let arch = std::env::consts::ARCH.to_string();
        let supported = model.contains("Raspberry Pi 4") || model.contains("Raspberry Pi 5");
        Self {
            model,
            arch,
            supported,
        }
    }

    pub fn require_supported(&self) -> Result<(), HalError> {
        if self.supported {
            Ok(())
        } else {
            Err(HalError::UnsupportedBoard(self.model.clone()))
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OsChecklist {
    pub kernel_64: bool,
    pub dm_verity: bool,
    pub clock_sync: bool,
    pub fs_exec: bool,
    pub power: PowerState,
}

impl OsChecklist {
    pub fn collect() -> Self {
        let kernel_64 = std::env::consts::ARCH == "aarch64"
            && std::fs::read_to_string("/proc/version")
                .unwrap_or_default()
                .contains("aarch64");
        let dm_verity = std::fs::read_to_string("/proc/cmdline")
            .unwrap_or_default()
            .contains("verity");
        let clock_sync = std::path::Path::new("/run/systemd/timesync/synchronized").exists()
            || std::path::Path::new("/var/lib/chrony").exists();
        let fs_exec = {
            let p = std::env::temp_dir().join(format!("nv-exec-{}", std::process::id()));
            std::fs::write(&p, b"#!/bin/sh\nexit 0\n").is_ok()
        };
        Self {
            kernel_64,
            dm_verity,
            clock_sync,
            fs_exec,
            power: PowerState::Ok,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keypad_parse_ok() {
        assert_eq!(Key::parse("5").unwrap(), Key::Digit(5));
        assert_eq!(Key::parse("branco").unwrap(), Key::Branco);
        assert_eq!(Key::parse("C").unwrap(), Key::Corrige);
        assert_eq!(Key::parse("ENTER").unwrap(), Key::Confirma);
        assert!(Key::parse("X").is_err());
        assert!(Key::parse("").is_err());
    }

    #[test]
    fn display_limits() {
        let mut d = DisplayState::new();
        assert!(d.set_lines(vec!["ok".into()]).is_ok());
        assert!(d.set_lines(vec!["x".repeat(65)]).is_err());
        assert!(d.set_lines(vec!["a".into(); 9]).is_err());
    }

    #[test]
    fn board_detect_never_panics() {
        let b = BoardInfo::detect();
        assert!(!b.model.is_empty());
        assert!(!b.arch.is_empty());
    }

    #[test]
    fn os_checklist_collects() {
        let c = OsChecklist::collect();
        let _ = serde_json::to_string(&c).unwrap();
    }
}
