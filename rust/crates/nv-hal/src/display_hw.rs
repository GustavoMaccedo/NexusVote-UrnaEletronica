// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use std::path::PathBuf;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum QtPlatform {
    Eglfs,
    LinuxFb,
}

#[derive(Debug, Clone)]
pub struct DisplayDetect {
    pub drm_present: bool,
    pub fb_present: bool,
    pub backlight: Option<PathBuf>,
    pub recommended: Option<QtPlatform>,
}

pub fn detect_display() -> DisplayDetect {
    let drm = PathBuf::from("/dev/dri/card0");
    let fb = PathBuf::from("/dev/fb0");
    let drm_present = drm.exists();
    let fb_present = fb.exists();
    let backlight = find_backlight();
    let recommended = if drm_present {
        Some(QtPlatform::Eglfs)
    } else if fb_present {
        Some(QtPlatform::LinuxFb)
    } else {
        None
    };
    DisplayDetect {
        drm_present,
        fb_present,
        backlight,
        recommended,
    }
}

fn find_backlight() -> Option<PathBuf> {
    let base = PathBuf::from("/sys/class/backlight");
    let entries = std::fs::read_dir(&base).ok()?;
    for e in entries.flatten() {
        let p = e.path().join("brightness");
        if p.exists() {
            return Some(p);
        }
    }
    None
}

pub fn set_brightness(pct: u8) -> Result<(), String> {
    let pct = pct.min(100);
    let bl = find_backlight()
        .ok_or_else(|| "backlight ausente (display HDMI sem brilho sysfs)".to_string())?;
    let max_p = bl.parent().unwrap().join("max_brightness");
    let max: u32 = std::fs::read_to_string(&max_p)
        .ok()
        .and_then(|s| s.trim().parse().ok())
        .unwrap_or(255);
    let val = (max as u64 * pct as u64 / 100) as u32;
    std::fs::write(&bl, val.to_string()).map_err(|e| format!("brilho {}: {e}", bl.display()))
}

pub fn qt_platform_value() -> Result<&'static str, String> {
    let d = detect_display();
    match d.recommended {
        Some(QtPlatform::Eglfs) => Ok("eglfs"),
        Some(QtPlatform::LinuxFb) => Ok("linuxfb:fb=/dev/fb0"),
        None => Err("display ausente (/dev/dri/card0 nem /dev/fb0)".into()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn detect_nunca_panica() {
        let d = detect_display();
        if !d.drm_present && !d.fb_present {
            assert_eq!(d.recommended, None);
            assert!(qt_platform_value().is_err());
        }
    }
}
