// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use crate::{HalError, Key};
use std::fs::File;
use std::io::Read;
use std::path::{Path, PathBuf};

pub mod code {
    pub const KEY_1: u16 = 2;
    pub const KEY_2: u16 = 3;
    pub const KEY_3: u16 = 4;
    pub const KEY_4: u16 = 5;
    pub const KEY_5: u16 = 6;
    pub const KEY_6: u16 = 7;
    pub const KEY_7: u16 = 8;
    pub const KEY_8: u16 = 9;
    pub const KEY_9: u16 = 10;
    pub const KEY_0: u16 = 11;
    pub const KEY_ENTER: u16 = 28;
    pub const KEY_A: u16 = 30;
    pub const KEY_D: u16 = 32;
    pub const KEY_C: u16 = 46;
    pub const KEY_B: u16 = 48;
    pub const KEY_KPASTERISK: u16 = 55;
    pub const KEY_KPSLASH: u16 = 98;
}

pub const EV_KEY: u16 = 0x01;

pub fn evdev_code_to_key(code: u16) -> Option<Key> {
    match code {
        code::KEY_1 => Some(Key::Digit(1)),
        code::KEY_2 => Some(Key::Digit(2)),
        code::KEY_3 => Some(Key::Digit(3)),
        code::KEY_4 => Some(Key::Digit(4)),
        code::KEY_5 => Some(Key::Digit(5)),
        code::KEY_6 => Some(Key::Digit(6)),
        code::KEY_7 => Some(Key::Digit(7)),
        code::KEY_8 => Some(Key::Digit(8)),
        code::KEY_9 => Some(Key::Digit(9)),
        code::KEY_0 => Some(Key::Digit(0)),
        code::KEY_A => Some(Key::Branco),
        code::KEY_B => Some(Key::Corrige),
        code::KEY_C | code::KEY_ENTER | code::KEY_KPSLASH => Some(Key::Confirma),
        code::KEY_KPASTERISK => Some(Key::CancelaMesario),
        _ => None,
    }
}

pub fn decode_event_bytes(buf: &[u8]) -> Option<(u16, u16, i32)> {
    if buf.len() < 24 {
        return None;
    }
    let ty = u16::from_le_bytes([buf[16], buf[17]]);
    let code = u16::from_le_bytes([buf[18], buf[19]]);
    let value = i32::from_le_bytes([buf[20], buf[21], buf[22], buf[23]]);
    Some((ty, code, value))
}

#[cfg(test)]
fn encode_event(ty: u16, code: u16, value: i32) -> [u8; 24] {
    let mut b = [0u8; 24];
    b[16..18].copy_from_slice(&ty.to_le_bytes());
    b[18..20].copy_from_slice(&code.to_le_bytes());
    b[20..24].copy_from_slice(&value.to_le_bytes());
    b
}

pub fn find_keypad_device() -> Option<PathBuf> {
    if let Ok(p) = std::env::var("NV_KEYPAD_DEV") {
        let pb = PathBuf::from(&p);
        if pb.exists() {
            return Some(pb);
        }
    }
    let link = Path::new("/dev/input/nv-keypad");
    if link.exists() {
        return Some(link.to_path_buf());
    }
    for n in 0..32 {
        let ev = format!("/dev/input/event{n}");
        let sys = format!("/sys/class/input/event{n}/device/name");
        if let Ok(name) = std::fs::read_to_string(&sys) {
            let l = name.to_lowercase();
            if l.contains("matrix") || l.contains("keypad") {
                let p = PathBuf::from(&ev);
                if p.exists() {
                    return Some(p);
                }
            }
        }
    }
    None
}

pub fn open_keypad() -> Result<File, HalError> {
    match find_keypad_device() {
        Some(p) => {
            File::open(&p).map_err(|e| HalError::Io(format!("teclado {}: {e}", p.display())))
        }
        None => Err(HalError::Io(
            "teclado matricial ausente (/dev/input/nv-keypad; dtoverlay=nv-keypad-4x4 + reboot)"
                .into(),
        )),
    }
}

pub fn read_key_press(f: &mut File) -> Result<Option<Key>, HalError> {
    let mut buf = [0u8; 24];
    loop {
        match f.read_exact(&mut buf) {
            Ok(()) => {}
            Err(e) if e.kind() == std::io::ErrorKind::UnexpectedEof => return Ok(None),
            Err(e) => return Err(HalError::Io(format!("evdev read: {e}"))),
        }
        let Some((ty, code, value)) = decode_event_bytes(&buf) else {
            continue;
        };
        if ty != EV_KEY || value != 1 {
            continue;
        }
        match evdev_code_to_key(code) {
            Some(k) => return Ok(Some(k)),
            None => continue,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn digits_map() {
        assert_eq!(evdev_code_to_key(2), Some(Key::Digit(1)));
        assert_eq!(evdev_code_to_key(11), Some(Key::Digit(0)));
    }

    #[test]
    fn funcoes_map() {
        assert_eq!(evdev_code_to_key(code::KEY_A), Some(Key::Branco));
        assert_eq!(evdev_code_to_key(code::KEY_B), Some(Key::Corrige));
        assert_eq!(evdev_code_to_key(code::KEY_C), Some(Key::Confirma));
        assert_eq!(evdev_code_to_key(code::KEY_ENTER), Some(Key::Confirma));
        assert_eq!(
            evdev_code_to_key(code::KEY_KPASTERISK),
            Some(Key::CancelaMesario)
        );
        assert_eq!(evdev_code_to_key(code::KEY_D), None);
    }

    #[test]
    fn decode_roundtrip() {
        let b = encode_event(EV_KEY, code::KEY_5, 1);
        assert_eq!(decode_event_bytes(&b), Some((EV_KEY, 6, 1)));
        assert_eq!(decode_event_bytes(&b[..10]), None);
    }

    #[test]
    fn release_ignored_by_reader() {
        let mut data = Vec::new();
        data.extend(encode_event(EV_KEY, code::KEY_B, 0));
        data.extend(encode_event(EV_KEY, code::KEY_7, 1));
        let dir = std::env::temp_dir().join(format!("nv-ev-{}", std::process::id()));
        let _ = std::fs::create_dir_all(&dir);
        let p = dir.join("ev.bin");
        std::fs::write(&p, &data).unwrap();
        let mut f = File::open(&p).unwrap();
        assert_eq!(read_key_press(&mut f).unwrap(), Some(Key::Digit(7)));
        let _ = std::fs::remove_file(&p);
    }

    #[test]
    fn sem_hw_falha() {
        let old = std::env::var("NV_KEYPAD_DEV").ok();
        std::env::set_var("NV_KEYPAD_DEV", "/dev/input/nv-inexistente-xyz");
        assert!(open_keypad().is_err());
        if let Some(v) = old {
            std::env::set_var("NV_KEYPAD_DEV", v);
        } else {
            std::env::remove_var("NV_KEYPAD_DEV");
        }
    }
}
