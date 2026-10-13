// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#![forbid(unsafe_code)]

use nv_hal::{hw_log::HwLog, keypad_evdev, rtc};
use std::fs::OpenOptions;
use std::io::Write;
use std::path::PathBuf;

fn main() {
    let work = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "/var/lib/nexusvote-lab/work".into());
    let work = PathBuf::from(work);
    if let Err(e) = run(&work) {
        eprintln!("nv-input-bridge: {e}");
        std::process::exit(1);
    }
}

fn run(work: &PathBuf) -> Result<(), String> {
    std::fs::create_dir_all(work).map_err(|e| format!("workdir: {e}"))?;
    let mut keypad = keypad_evdev::open_keypad().map_err(|e| e.to_string())?;
    let log = HwLog::open(&work.join("hw-events.jsonl")).map_err(|e| e.to_string())?;
    let ts = rtc::iso_utc(rtc::read_time().epoch_secs);
    log.append(&ts, "boot", "nv-input-bridge")
        .map_err(|e| e.to_string())?;
    let fifo = work.join("hw-keys.fifo");
    if !fifo.exists() {
        let st = std::process::Command::new("mkfifo")
            .arg(&fifo)
            .status()
            .map_err(|e| format!("mkfifo spawn: {e}"))?;
        if !st.success() && !fifo.exists() {
            return Err("mkfifo falhou".into());
        }
    }
    let mut out = OpenOptions::new()
        .write(true)
        .open(&fifo)
        .map_err(|e| format!("fifo {}: {e}", fifo.display()))?;
    loop {
        match keypad_evdev::read_key_press(&mut keypad) {
            Ok(Some(k)) => {
                let line = match k {
                    nv_hal::Key::Digit(d) => d.to_string(),
                    nv_hal::Key::Branco => "BRANCO".into(),
                    nv_hal::Key::Corrige => "CORRIGE".into(),
                    nv_hal::Key::Confirma => "CONFIRMA".into(),
                    nv_hal::Key::CancelaMesario => "CANCELA".into(),
                };
                let ts = rtc::iso_utc(rtc::read_time().epoch_secs);
                let _ = log.append(&ts, "key_ok", &line);
                if writeln!(out, "{line}").is_err() {
                    out = OpenOptions::new()
                        .write(true)
                        .open(&fifo)
                        .map_err(|e| format!("fifo reopen: {e}"))?;
                }
                let _ = out.flush();
            }
            Ok(None) => return Err("teclado EOF (desconectado?)".into()),
            Err(e) => return Err(e.to_string()),
        }
    }
}
